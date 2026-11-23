#include <manifest/market/Marketplace.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

#include <jettison/core/Assert.h>


namespace Manifest::Core
{
namespace
{
// A quantity at or under this is nothing: the book counts in whole units of a good,
// and the arithmetic of doubles leaves a residue under the dust of a fill.
constexpr double quantityEpsilon = 1e-9;

// The most basis points a fee, premium or rebate is taken at, a hundredfold, so each sum of them
// fits an int; one that is no number is none.
constexpr double kMaxBps = 1'000'000.0;

double BoundedBps(double bps)
{
	return std::isfinite(bps) ? std::clamp(bps, -kMaxBps, kMaxBps) : 0.0;
}

std::optional<long long> CheckedFeeFor(long long gross, int feeBps)
{
	if (gross <= 0 || feeBps <= 0)
	{
		return 0;
	}
	const __int128 fee = (static_cast<__int128>(gross) * feeBps + 9999) / 10000;
	if (fee > std::numeric_limits<long long>::max())
	{
		return std::nullopt;
	}
	return static_cast<long long>(fee);
}

std::optional<long long> WholeCost(Decimal price, double quantity, bool roundUp)
{
	if (price < 0 || !std::isfinite(quantity) || quantity < 0.0)
	{
		return std::nullopt;
	}
	// Quantities retain the development model's double arithmetic at this boundary.
	const double value = price.ToDouble() * quantity;
	const double whole = roundUp ? std::ceil(value) : std::round(value);
	// 2^63 is exactly representable; LLONG_MAX rounds up when converted to double.
	if (!std::isfinite(whole) || whole < 0 || whole >= std::ldexp(1.0, 63))
	{
		return std::nullopt;
	}
	return static_cast<long long>(whole);
}

bool CanRefundOrders(
    const EntityStore &world, const std::vector<std::string> &orders, std::string &error)
{
	std::map<std::string, __int128> creditRefunds;
	for (const auto &orderId : orders)
	{
		const auto *info = world.Get<MarketOrderInfo>(orderId);
		const auto *owner = world.Get<OrderOwner>(orderId);
		if (info != nullptr && owner != nullptr)
		{
			creditRefunds[owner->civilisationId] += info->reservedCredits;
		}
	}
	for (const auto &[ownerId, refund] : creditRefunds)
	{
		if (const auto *treasury = world.Get<Treasury>(ownerId))
		{
			if (static_cast<__int128>(treasury->credits) + refund >
			    std::numeric_limits<long long>::max())
			{
				error = "Marketplace: reservation refunds exceed the credit range";
				return false;
			}
		}
	}
	return true;
}

double OpenQuantity(const MarketOrderInfo &request)
{
	return request.quantity - request.filled;
}

bool IsFulfilled(const MarketOrderInfo &request)
{
	return OpenQuantity(request) <= quantityEpsilon;
}

// The resting-first rule: of two orders that meet, the one that reached the book first
// is the maker and its limit price is the price of the fill.
bool RestsFirst(const MarketOrderInfo &left, const MarketOrderInfo &right)
{
	if (left.placedTick != right.placedTick)
	{
		return left.placedTick < right.placedTick;
	}

	return left.orderId < right.orderId;
}

// An order still in the book: not filled, not retired.
bool IsLive(const EntityStore &entityStore, EntityStore::Entity entity)
{
	const auto *info = entityStore.IsValid(entity) ? entityStore.Get<MarketOrderInfo>(entity) : nullptr;

	return info != nullptr && info->status == OrderStatus::Active && !IsFulfilled(*info);
}

// Price then time, the rule every book of this shape keeps: the best price first, and
// inside a price the order that posted first. The order id breaks the last tie, so the
// book is one order on every platform, and a fill log that says "Order-7 filled first"
// says it in every run.
void SortBestFirst(
    const EntityStore &entityStore, std::vector<EntityStore::Entity> &orders, OrderSide side)
{
	std::sort(
	    orders.begin(), orders.end(),
	    [&entityStore, side](EntityStore::Entity left, EntityStore::Entity right)
	    {
		    const auto *a = entityStore.Get<MarketOrderInfo>(left);
		    const auto *b = entityStore.Get<MarketOrderInfo>(right);

		    if (a == nullptr || b == nullptr)
		    {
			    return false;
		    }

		    // A sell book runs cheapest first and a buy book richest first, which is one
		    // ascending sort with the buy side's price turned over.
		    const Decimal keyA = side == OrderSide::Sell ? a->limitPrice : -a->limitPrice;
		    const Decimal keyB = side == OrderSide::Sell ? b->limitPrice : -b->limitPrice;

		    if (keyA != keyB)
		    {
			    return keyA < keyB;
		    }

		    if (a->placedTick != b->placedTick)
		    {
			    return a->placedTick < b->placedTick;
		    }

		    return a->orderId < b->orderId;
	    });
}

// One venue's book, every good and side, each best first: one pass over the orders, where
// ActiveOrders is a pass for each good and side.
using Books = std::map<std::pair<std::string, OrderSide>, std::vector<EntityStore::Entity>>;

Books BooksAt(const EntityStore &entityStore, const std::string &marketId)
{
	Books books;

	for (const EntityStore::Entity entity : entityStore.View<MarketOrderInfo, OrderAt>())
	{
		const auto *at = entityStore.Get<OrderAt>(entity);
		const auto *info = entityStore.Get<MarketOrderInfo>(entity);

		if (at != nullptr && info != nullptr && at->marketId == marketId &&
		    info->status == OrderStatus::Active && !IsFulfilled(*info))
		{
			books[{info->itemId, info->side}].push_back(entity);
		}
	}

	for (auto &[key, orders] : books)
	{
		SortBestFirst(entityStore, orders, key.second);
	}

	return books;
}

// The orders resting at a price, and their units: the screen that shows a price shows
// the size of the pile at it in units and in orders, and the orders in the whole book are
// the pile below the best price, which is a number a screen has no use for.
std::pair<unsigned, double> AtLevel(
    const EntityStore &entityStore, const std::vector<EntityStore::Entity> &orders, Decimal price)
{
	std::pair<unsigned, double> level{0, 0.0};

	for (const EntityStore::Entity entity : orders)
	{
		const auto *info = entityStore.Get<MarketOrderInfo>(entity);

		if (info != nullptr && info->limitPrice == price)
		{
			++level.first;
			level.second += OpenQuantity(*info);
		}
	}

	return level;
}
} // namespace


MarketplaceService::MarketplaceService(MarketplaceOptions options) : m_options(options)
{
}


const MarketplaceOptions &MarketplaceService::Options() const
{
	return m_options;
}


void MarketplaceService::SetOptions(MarketplaceOptions options)
{
	m_options = options;
}


std::string MarketplaceService::VenueBlockReason(
    const EntityStore &entityStore, const std::string &bodyId)
{
	if (bodyId.empty())
	{
		return "the body id is empty: a marketplace sits at a body, and a body is an entity of this "
		       "world";
	}

	if (!entityStore.Has(bodyId))
	{
		return "'" + bodyId +
		       "' is not an entity of this world: a venue is a body, and a body is an entity";
	}

	const auto *body = entityStore.Get<BodyInfo>(bodyId);

	if (body == nullptr)
	{
		return "'" + bodyId +
		       "' carries no BodyInfo: a market sits at a planet or at a satellite in orbit around one";
	}

	if (entityStore.HasComponent<MarketplaceInfo>(bodyId))
	{
		return "'" + bodyId + "' already hosts a marketplace";
	}

	if (body->civilisationId.empty())
	{
		return "'" + bodyId +
		       "' belongs to no civilisation: the venue's fee, its risk and its take all belong to a "
		       "world";
	}

	switch (body->kind)
	{
		case BodyKind::Planet: return std::string{};

		case BodyKind::Satellite:
			if (body->primaryBodyId.empty())
			{
				return "the satellite '" + bodyId +
				       "' orbits no planet: a station hosts a market because it sits above a world's "
				       "gravity well, and "
				       "this one sits above nothing";
			}

			if (body->primaryBodyId == body->bodyId)
			{
				return "the satellite '" + bodyId + "' orbits itself";
			}

			{
				const auto *primary = entityStore.Get<BodyInfo>(body->primaryBodyId);

				if (primary == nullptr)
				{
					return "the satellite '" + bodyId + "' orbits '" + body->primaryBodyId +
					       "', which is not a body of this world";
				}

				if (primary->kind != BodyKind::Planet)
				{
					return "the satellite '" + bodyId + "' orbits " + DescribeBodyKind(primary->kind) +
					       " '" + body->primaryBodyId +
					       "': the station trade is the trade of the ships that come up a planet's "
					       "gravity well, and a star has no market at the bottom of its";
				}
			}

			return std::string{};

		case BodyKind::Star: return "'" + bodyId + "' is a star: no people, so no market";
	}

	return "'" + bodyId + "' is no kind of body this core knows";
}


bool MarketplaceService::Open(
    EntityStore &entityStore, const std::string &bodyId, std::string &errorMessage)
{
	const std::string reason = VenueBlockReason(entityStore, bodyId);

	if (!reason.empty())
	{
		errorMessage = "Marketplace: " + reason;

		return false;
	}

	const BodyInfo *body = entityStore.Get<BodyInfo>(bodyId);

	MarketplaceInfo venue;
	venue.marketId = bodyId;
	venue.venueName = body->displayName;
	venue.civilisationId = body->civilisationId;
	venue.atPlanet = body->kind == BodyKind::Planet;
	venue.atSatellite = body->kind == BodyKind::Satellite;
	venue.baseListingFeeBps = m_options.listingFeeBps;
	venue.orderLifeTicks = m_options.orderLifeTicks;
	venue.openedTick = 0;

	entityStore.Put<MarketplaceInfo>(bodyId, std::move(venue));

	// The venue's own record of itself: the quote table, the volume, the prices the
	// fills left behind, the log. A market that has never traded has a record and no
	// quotes, which is the reading a UI prints as "nothing traded here yet".
	entityStore.Put<MarketRecord>(bodyId, MarketRecord{});

	return true;
}


bool MarketplaceService::Close(
    EntityStore &entityStore, const std::string &bodyId, std::string &errorMessage)
{
	auto *venue = entityStore.Get<MarketplaceInfo>(bodyId);

	if (venue == nullptr)
	{
		errorMessage = std::format("Marketplace: '{}' hosts no marketplace", bodyId);

		return false;
	}

	// Every order of the book goes back to the trader that posted it: a venue that
	// closes in the middle of a session does not keep the goods or the credits of the
	// orders it was holding.
	std::vector<std::string> orders;

	const auto view = entityStore.View<MarketOrderInfo, OrderAt>();

	for (const EntityStore::Entity entity : view)
	{
		const OrderAt *at = entityStore.Get<OrderAt>(entity);

		if (at != nullptr && at->marketId == bodyId)
		{
			const std::string name = entityStore.NameOf(entity);

			if (!name.empty())
			{
				orders.push_back(name);
			}
		}
	}

	std::sort(orders.begin(), orders.end());
	if (!CanRefundOrders(entityStore, orders, errorMessage))
	{
		return false;
	}

	for (const auto &orderId : orders)
	{
		if (!RetireOrder(entityStore, orderId, errorMessage))
		{
			return false;
		}
	}

	// The venue closes; the market effects of the civilisation stay bought. The tiers
	// were paid for, and a venue that reopens reads the same clearance and the same
	// risk the player bought, so the component is left where it is.
	entityStore.Remove<MarketRecord>(bodyId);
	entityStore.Remove<MarketplaceInfo>(bodyId);

	return true;
}


bool MarketplaceService::HasMarket(const EntityStore &entityStore, const std::string &bodyId) const
{
	return entityStore.HasComponent<MarketplaceInfo>(bodyId);
}


std::vector<std::string> MarketplaceService::MarketIds(
    const EntityStore &entityStore, const std::string &civilisationId) const
{
	std::vector<std::string> markets;

	const auto view = entityStore.View<MarketplaceInfo>();

	for (const EntityStore::Entity entity : view)
	{
		const auto *venue = entityStore.Get<MarketplaceInfo>(entity);

		if (venue == nullptr || (!civilisationId.empty() && venue->civilisationId != civilisationId))
		{
			continue;
		}

		markets.push_back(venue->marketId);
	}

	std::sort(markets.begin(), markets.end());

	return markets;
}


std::size_t MarketplaceService::MarketCount(const EntityStore &entityStore) const
{
	return entityStore.ComponentCount<MarketplaceInfo>();
}


const MarketplaceInfo *MarketplaceService::MarketAt(
    const EntityStore &entityStore, const std::string &bodyId) const
{
	return entityStore.Get<MarketplaceInfo>(bodyId);
}


bool MarketplaceService::PostOrder(
    EntityStore &entityStore, const OrderRequest &request, std::string &orderId,
    std::string &errorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(entityStore, request.marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	if (request.itemId.empty())
	{
		errorMessage = std::format("Marketplace '{}': the itemId is empty", request.marketId);

		return false;
	}

	if (request.limitPrice < 0)
	{
		errorMessage = std::format(
		    "Marketplace '{}': a limit price is never negative: a trader that would pay negative for a "
		    "good, or be paid negative to part with one, has a venue's problem and not this book's",
		    request.marketId);

		return false;
	}

	if (request.limitPrice == 0)
	{
		errorMessage = std::format(
		    "Marketplace '{}': a resting order has to state a limit price; the order that walks the "
		    "book is ExecuteOrder, and it names no price",
		    request.marketId);

		return false;
	}

	const std::string marketId = request.marketId;

	// The depth one side of one good's book may hold: a venue with a deck of a certain
	// size turns a quoter away when the deck is full, and the harbours and the tribunal
	// are what buy the size. A resting order's limit alone: an order that walks the book
	// leaves nothing on the deck.
	const double cap = DepthUnits(entityStore, marketId);

	if (cap > 0.0)
	{
		double sideDepth = 0.0;

		for (const EntityStore::Entity entity :
		     ActiveOrders(entityStore, marketId, request.itemId, request.side))
		{
			if (const MarketOrderInfo *resting = entityStore.Get<MarketOrderInfo>(entity))
			{
				sideDepth += OpenQuantity(*resting);
			}
		}

		if (sideDepth + request.quantity > cap + quantityEpsilon)
		{
			errorMessage = std::format(
			    "Marketplace '{}': the {} book of '{}' holds {} units and the deck of the venue holds "
			    "{}, the depth the harbours and the tribunal buy",
			    marketId, DescribeOrderSide(request.side), request.itemId, FormatNumber(sideDepth),
			    FormatNumber(cap));

			return false;
		}
	}

	if (!CreateOrder(entityStore, request, marketId, orderId, errorMessage))
	{
		return false;
	}

	// A post that crosses the book trades at once: the seller who posts at or under the
	// best bid has sold, and the order that filled is out of the book and in the log.
	std::vector<TradeFill> fills;

	if (!MatchInto(entityStore, marketId, fills, errorMessage))
	{
		return false;
	}

	return RefreshQuotes(entityStore, marketId, errorMessage);
}


std::vector<TradeFill> MarketplaceService::ExecuteOrder(
    EntityStore &entityStore, const OrderRequest &request, std::string &errorMessage)
{
	std::vector<TradeFill> fills;

	const MarketplaceInfo *venue = RequireVenue(entityStore, request.marketId, errorMessage);

	if (venue == nullptr)
	{
		return fills;
	}

	if (request.itemId.empty())
	{
		errorMessage = std::format("Marketplace '{}': the itemId is empty", request.marketId);

		return fills;
	}

	if (!std::isfinite(request.quantity) || request.quantity <= quantityEpsilon)
	{
		errorMessage =
		    std::format("Marketplace '{}': the quantity has to be above nothing", request.marketId);

		return fills;
	}

	if (request.limitPrice < 0)
	{
		errorMessage = "Marketplace: a limit price cannot be negative";
		return fills;
	}

	const std::string marketId = request.marketId;
	const OrderSide opposite = request.side == OrderSide::Buy ? OrderSide::Sell : OrderSide::Buy;

	// The order that walks the book is an order like any other, and it is posted like
	// any other, with one difference: it leaves nothing resting. Posting it reserves
	// what a walk needs - the goods of a sell, the credits of a buy - so a walk cannot
	// trade a unit the trader has not got, and the reservation that the walk leaves
	// behind goes back at the end.
	OrderRequest resting = request;

	// The other side of the book, best first: the walk takes it in this order, and a fill only
	// ever spends the order at the front.
	const std::vector<EntityStore::Entity> book =
	    ActiveOrders(entityStore, marketId, request.itemId, opposite);

	if (resting.limitPrice <= 0)
	{
		// A marketable order names no price, and a resting order has to. The limit is the
		// price of the order at which the book's depth reaches the quantity: a buy reserves
		// what the asks it walks cost, not the dearest ask on the book.
		double depth = 0.0;

		for (const EntityStore::Entity entity : book)
		{
			const MarketOrderInfo *info = entityStore.Get<MarketOrderInfo>(entity);

			if (info == nullptr)
			{
				continue;
			}

			resting.limitPrice = info->limitPrice;
			depth += OpenQuantity(*info);

			if (depth + quantityEpsilon >= request.quantity)
			{
				break;
			}
		}

		if (resting.limitPrice <= 0)
		{
			errorMessage = std::format(
			    "Marketplace '{}': the book of '{}' has no {} for a marketable order to walk, and an "
			    "order with nothing to walk has to name a price",
			    marketId, request.itemId, request.side == OrderSide::Buy ? "asks" : "bids");

			return fills;
		}
	}

	std::string takerId;

	if (!CreateOrder(entityStore, resting, marketId, takerId, errorMessage))
	{
		return fills;
	}

	EntityStore::Entity taker = entityStore.Find(takerId);

	if (!entityStore.IsValid(taker))
	{
		errorMessage =
		    std::format("Marketplace '{}': the order '{}' is gone before it walked", marketId, takerId);

		return fills;
	}

	std::size_t next = 0;

	while (true)
	{
		auto *walking = entityStore.Get<MarketOrderInfo>(taker);

		if (walking == nullptr || IsFulfilled(*walking))
		{
			break;
		}

		// Past the makers the last fill completed or retired.
		while (next < book.size() && !IsLive(entityStore, book[next]))
		{
			++next;
		}

		if (next == book.size())
		{
			break;
		}

		const EntityStore::Entity maker = book[next];
		const MarketOrderInfo *makerInfo = entityStore.Get<MarketOrderInfo>(maker);

		// The walk's own limit: the asks above a buy limit, and the bids below a sell
		// limit, are refused, and the book is sorted best first, so the first refusal
		// is the end of the walk.
		if (request.side == OrderSide::Buy && makerInfo->limitPrice > walking->limitPrice)
		{
			break;
		}

		if (request.side == OrderSide::Sell && makerInfo->limitPrice < walking->limitPrice)
		{
			break;
		}

		if (!FillPair(entityStore, marketId, maker, taker, fills, errorMessage))
		{
			// The pair cannot trade a whole unit: the trader on the other side has run
			// out, so the walk has run out of book.
			break;
		}

		// The taker entity can be the one that filled, and a filled order leaves the
		// book, which destroys it: the walk is over with it.
		if (!entityStore.IsValid(taker))
		{
			break;
		}
	}

	if (entityStore.IsValid(taker))
	{
		if (!RetireOrder(entityStore, takerId, errorMessage))
		{
			return fills;
		}
	}

	RefreshQuotes(entityStore, marketId, errorMessage);

	return fills;
}


bool MarketplaceService::CancelOrder(
    EntityStore &entityStore, const std::string &orderId, std::string &errorMessage)
{
	// An empty id names no order at all, which is a caller's error and not an order that
	// is gone: the venue answers it with a message, because a cancel posted with no id is
	// a call that has nothing to take off the book.
	if (orderId.empty())
	{
		errorMessage = "Marketplace: the orderId is empty; a cancel is answered by the id of the "
		               "order, and an id of nothing names no order";

		return false;
	}

	const MarketOrderInfo *info = RequireOrder(entityStore, orderId, errorMessage);

	if (info == nullptr)
	{
		// The order is not on the book. The venues keep the trades they have made in
		// their records, oldest first and capped to a window, and a number that a log
		// names is a number a book has traded: the promise the order kept is in the log,
		// the goods it reserved left the stockpile in the fill, and a refund would pay
		// the trader the cargo twice, so the venue refuses and names the number. A number
		// the venues have never seen, or one that expired or was cancelled, is gone,
		// which is the target state of a cancel, and the venue answers that with the
		// answer that the book is clean.
		for (const EntityStore::Entity venueEntity : entityStore.View<MarketRecord>())
		{
			const MarketRecord *record = entityStore.Get<MarketRecord>(venueEntity);

			if (record == nullptr)
			{
				continue;
			}

			for (const TradeFill &fill : record->fills)
			{
				if (fill.makerOrderId == orderId || fill.takerOrderId == orderId)
				{
					errorMessage = std::format(
					    "Marketplace: the order '{}' is in the fill log of venue '{}': the order "
					    "traded, its promise is in the log, its reserved goods left the stockpile in "
					    "the fill, and a withdraw would pay the trader the cargo twice",
					    orderId, fill.marketId);

					return false;
				}
			}
		}

		// The target state of a cancel is that the order is gone, and an order that
		// expired, was cancelled, or was never posted is gone. A cancel that reports
		// failure for an order that is already gone leaves the caller with a book it
		// cannot clean, and the books of a tick are cleaned by cancelling what the venue
		// listed.
		errorMessage.clear();

		return true;
	}

	const OrderAt *at = entityStore.Get<OrderAt>(orderId);
	const std::string marketId = at != nullptr ? at->marketId : std::string{};

	if (!RetireOrder(entityStore, orderId, errorMessage))
	{
		return false;
	}

	// The cancel lifts a price off the book, and the orders behind it can now meet: the
	// bid that was third is the bid a resting ask can trade.
	std::vector<TradeFill> fills;

	if (!marketId.empty() && HasMarket(entityStore, marketId))
	{
		return MatchInto(entityStore, marketId, fills, errorMessage) &&
		       RefreshQuotes(entityStore, marketId, errorMessage);
	}

	return true;
}


bool MarketplaceService::CreateOrder(
    EntityStore &entityStore, const OrderRequest &request, const std::string &marketId,
    std::string &orderId, std::string &errorMessage)
{
	orderId.clear();

	const MarketplaceInfo *venue = RequireVenue(entityStore, marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	if (request.itemId.empty())
	{
		errorMessage = std::format(
		    "Marketplace '{}': the itemId is empty; a book is a pile of orders in one good", marketId);

		return false;
	}

	if (!std::isfinite(request.quantity) || request.quantity <= quantityEpsilon)
	{
		errorMessage = std::format("Marketplace '{}': the quantity has to be above nothing", marketId);

		return false;
	}

	if (request.limitPrice < 0)
	{
		errorMessage = std::format(
		    "Marketplace '{}': a limit price is never negative; a trader that would pay negative for a "
		    "good is a venue's problem, not this book's",
		    marketId);

		return false;
	}

	if (m_options.minQuantity > 0.0 && request.quantity + quantityEpsilon < m_options.minQuantity)
	{
		errorMessage = std::format(
		    "Marketplace '{}': the venue takes no order under {} units of a good", marketId,
		    FormatNumber(m_options.minQuantity));

		return false;
	}

	if (m_options.minPrice > 0 && request.limitPrice > 0 && request.limitPrice < m_options.minPrice)
	{
		errorMessage = std::format(
		    "Marketplace '{}': the venue takes no order under a price of {}", marketId,
		    m_options.minPrice.ToString());

		return false;
	}

	EntityStore::Entity trader = EntityStore::Null;

	if (!RequireTrader(entityStore, request.civilisationId, trader, errorMessage))
	{
		return false;
	}

	// The fee of the moment the order posts: a venue's fee moves with the risk of the
	// place and the clearance of the licence, and an order that posted at 40 bps is an
	// order that pays 40 bps, whatever the venue's charges read at the fill.
	const int feeBps = ListingFeeBps(entityStore, marketId);

	MarketOrderInfo info;
	info.itemId = request.itemId;
	info.side = request.side;
	info.status = OrderStatus::Active;
	info.quantity = request.quantity;
	info.filled = 0.0;
	info.limitPrice = request.limitPrice;
	info.feeBps = feeBps;
	info.placedTick = request.tick;
	// The life of an order is the life the service posting it gives an order at the tick
	// it lands: the venue's own record keeps the life of the venue as it opened, and the
	// service that writes the order is the service whose charges the order is priced by,
	// the same service whose fee the reservation is built on. The life is added plain, and
	// a life of nothing leaves expiresTick at the tick the order was posted: an order that
	// rests without a life is an order that rests until it trades, is cancelled, or the
	// venue closes, which is the reading AdvanceTick keeps and the reason the expiry of an
	// order is read against the tick it was posted at and not against nothing.
	if (m_options.orderLifeTicks > 0 &&
	    request.tick > std::numeric_limits<long long>::max() - m_options.orderLifeTicks)
	{
		errorMessage = "Marketplace: the order's life runs past the tick range";
		return false;
	}
	info.expiresTick = request.tick + m_options.orderLifeTicks;

	if (!WholeCost(request.limitPrice, request.quantity, true))
	{
		errorMessage = "Marketplace: order notional exceeds the credit range";
		return false;
	}

	// The reservation, the reason a fill never fails for want of the thing the fill
	// trades: the goods leave the seller's stockpile when the order posts and sit in the
	// order until it fills, leaves the book, or expires, and the credits leave the
	// buyer's treasury at the same time and sit at the worst price the limit allows.
	//
	// RequireTrader checked that the trader carries both components, so neither pointer
	// is null here, and the two pointers are read and written in the same pool for the
	// whole of the reservation.
	auto *treasury = entityStore.Get<Treasury>(trader);
	auto *stock = entityStore.Get<Stockpile>(trader);

	if (request.side == OrderSide::Sell)
	{
		const auto holding = stock->units.find(request.itemId);
		const double available = holding != stock->units.end() ? holding->second : 0.0;

		if (m_options.requireStock)
		{
			if (available + quantityEpsilon < request.quantity)
			{
				errorMessage = std::format(
				    "Marketplace '{}': the seller '{}' holds {} units of '{}' and posts {}; a sell "
				    "order has to be covered by the stockpile it sells from",
				    marketId, request.civilisationId, FormatNumber(available), request.itemId,
				    FormatNumber(request.quantity));

				return false;
			}

			stock->units[request.itemId] -= request.quantity;
			info.reservedGoods = request.quantity;
		}
		// A venue that allows a sell order for cargo no one holds leaves the goods in the
		// seller's stockpile and spends them at the fill, which is the promises venue and
		// is off by default.
	}
	else if (m_options.requireFunds)
	{
		const auto cost = WholeCost(request.limitPrice, request.quantity, true);
		const auto fee = cost ? CheckedFeeFor(*cost, feeBps) : std::nullopt;
		if (!cost || !fee ||
		    static_cast<__int128>(*cost) + *fee > std::numeric_limits<long long>::max())
		{
			errorMessage = "Marketplace: buy reservation exceeds the credit range";
			return false;
		}
		const long long need = *cost + *fee;

		if (treasury->credits < need)
		{
			errorMessage = std::format(
			    "Marketplace '{}': the buyer '{}' has {} credits and the order needs {}, the goods at "
			    "the limit price and the venue's fee on them",
			    marketId, request.civilisationId, std::to_string(treasury->credits),
			    std::to_string(need));

			return false;
		}

		treasury->credits -= need;
		info.reservedCredits = need;
	}

	orderId = entityStore.NextId("Order");
	info.orderId = orderId;

	EntityStore::Entity order = EntityStore::Null;

	if (!entityStore.Create(orderId, order, errorMessage))
	{
		// The id the world hands out is never one it has handed out before, so the only
		// way here is the caller's own id in the way, and a reservation that is refused
		// has to go back the way it came.
		if (info.reservedCredits > 0)
		{
			treasury->credits += info.reservedCredits;
		}

		if (info.reservedGoods > quantityEpsilon)
		{
			stock->units[info.itemId] += info.reservedGoods;
		}

		return false;
	}

	entityStore.Add<MarketOrderInfo>(order, std::move(info));
	entityStore.Add<OrderAt>(order, OrderAt{marketId});
	entityStore.Add<OrderOwner>(order, OrderOwner{request.civilisationId});

	return true;
}


bool MarketplaceService::MatchInto(
    EntityStore &entityStore, const std::string &marketId, std::vector<TradeFill> &fillLog,
    std::string &errorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(entityStore, marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	// The goods cross in item order, the books' own, the same on every platform.
	const Books books = BooksAt(entityStore, marketId);

	for (const auto &[key, asks] : books)
	{
		const auto bidBook = books.find({key.first, OrderSide::Buy});

		if (key.second != OrderSide::Sell || bidBook == books.end())
		{
			continue;
		}

		const std::vector<EntityStore::Entity> &bids = bidBook->second;
		std::size_t nextAsk = 0;
		std::size_t nextBid = 0;

		// The book of one good, crossed to the end: a fill that leaves both sides
		// standing lets the next pair meet, and the crossing stops when the best bid
		// no longer reaches the best ask. A fill only spends the two fronts, so the
		// sides keep their order and the fronts move past what it completed or retired.
		while (true)
		{
			while (nextAsk < asks.size() && !IsLive(entityStore, asks[nextAsk]))
			{
				++nextAsk;
			}

			while (nextBid < bids.size() && !IsLive(entityStore, bids[nextBid]))
			{
				++nextBid;
			}

			if (nextAsk == asks.size() || nextBid == bids.size())
			{
				break;
			}

			const MarketOrderInfo *ask = entityStore.Get<MarketOrderInfo>(asks[nextAsk]);
			const MarketOrderInfo *bid = entityStore.Get<MarketOrderInfo>(bids[nextBid]);

			if (bid->limitPrice < ask->limitPrice)
			{
				break;
			}

			// The resting-first rule: the order that reached the book first is the
			// maker, and its limit price is the price both sides trade at.
			const bool askRestsFirst = RestsFirst(*ask, *bid);
			const EntityStore::Entity maker = askRestsFirst ? asks[nextAsk] : bids[nextBid];
			const EntityStore::Entity taker = askRestsFirst ? bids[nextBid] : asks[nextAsk];

			std::string fillError;

			if (!FillPair(entityStore, marketId, maker, taker, fillLog, fillError))
			{
				// Without a message the pair cannot trade a whole unit - a seller at a venue
				// that spends the stockpile at the fill has run out - and the pair is the
				// front of both books. With one, settlement failed, and the caller hears it.
				if (fillError.empty())
				{
					break;
				}

				errorMessage = std::move(fillError);

				return false;
			}
		}
	}

	return true;
}


std::vector<TradeFill> MarketplaceService::MatchBook(
    EntityStore &entityStore, const std::string &marketId, std::string &errorMessage)
{
	std::vector<TradeFill> fills;

	if (!MatchInto(entityStore, marketId, fills, errorMessage))
	{
		return {};
	}

	return fills;
}


std::vector<std::string> MarketplaceService::Book(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId,
    OrderSide side) const
{
	std::vector<std::string> orders;

	for (const EntityStore::Entity entity : ActiveOrders(entityStore, marketId, itemId, side))
	{
		const std::string name = entityStore.NameOf(entity);

		if (!name.empty())
		{
			orders.push_back(name);
		}
	}

	return orders;
}


std::vector<MarketOrderInfo> MarketplaceService::OrdersOf(
    const EntityStore &entityStore, const std::string &marketId,
    const std::string &civilisationId) const
{
	std::vector<MarketOrderInfo> orders;

	const auto view = entityStore.View<MarketOrderInfo, OrderAt, OrderOwner>();

	for (const EntityStore::Entity entity : view)
	{
		const auto *info = entityStore.Get<MarketOrderInfo>(entity);
		const auto *at = entityStore.Get<OrderAt>(entity);
		const auto *owner = entityStore.Get<OrderOwner>(entity);

		if (info == nullptr || at == nullptr || owner == nullptr)
		{
			continue;
		}

		if (!marketId.empty() && at->marketId != marketId)
		{
			continue;
		}

		if (!civilisationId.empty() && owner->civilisationId != civilisationId)
		{
			continue;
		}

		orders.push_back(*info);
	}

	std::sort(
	    orders.begin(), orders.end(), [](const MarketOrderInfo &left, const MarketOrderInfo &right)
	    { return left.orderId < right.orderId; });

	return orders;
}


std::size_t MarketplaceService::OpenOrderCount(
    const EntityStore &entityStore, const std::string &marketId) const
{
	std::size_t open = 0;

	const auto view = entityStore.View<MarketOrderInfo, OrderAt>();

	for (const EntityStore::Entity entity : view)
	{
		const auto *info = entityStore.Get<MarketOrderInfo>(entity);
		const auto *at = entityStore.Get<OrderAt>(entity);

		if (info != nullptr && at != nullptr && (marketId.empty() || at->marketId == marketId) &&
		    info->status == OrderStatus::Active && OpenQuantity(*info) > quantityEpsilon)
		{
			++open;
		}
	}

	return open;
}


bool MarketplaceService::RefreshQuotes(
    EntityStore &entityStore, const std::string &marketId, std::string &errorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(entityStore, marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	// The record keeps what the fills wrote - the last price, its tick, the volume ever
	// traded - and the quote table is rebuilt from the book: a quote is a fact about
	// the book, and a book that changed is a quote table that changed.
	MarketRecord record;

	const MarketRecord *existing = entityStore.Get<MarketRecord>(marketId);

	if (existing != nullptr)
	{
		record = *existing;
	}

	// The goods of the table the venue published keep their rows, traded or not: the
	// venue has seen them, and the row of a good whose last order left the book is the row
	// a screen shows beside a good nobody is quoting, at zero depth and with the numbers
	// of the last trade beside it, which is what a market's list of goods is. A good whose
	// last trade is in the record keeps its row even when the venue published no quote for
	// it, because the price of the last trade is the number the UI prints beside it.
	std::set<std::string> items;

	for (const auto &entry : record.quotes)
	{
		items.insert(entry.first);
	}

	for (const auto &entry : record.volume)
	{
		items.insert(entry.first);
	}

	record.quotes.clear();

	Books books = BooksAt(entityStore, marketId);

	for (const auto &entry : books)
	{
		items.insert(entry.first.first);
	}

	for (const auto &itemId : items)
	{
		MarketQuote quote;
		quote.itemId = itemId;

		const std::vector<EntityStore::Entity> &asks = books[{itemId, OrderSide::Sell}];
		const std::vector<EntityStore::Entity> &bids = books[{itemId, OrderSide::Buy}];

		if (!asks.empty())
		{
			const MarketOrderInfo *best = entityStore.Get<MarketOrderInfo>(asks.front());

			quote.bestAsk = best != nullptr ? best->limitPrice : Decimal{};
			std::tie(quote.askOrders, quote.askDepth) = AtLevel(entityStore, asks, quote.bestAsk);
		}

		if (!bids.empty())
		{
			const MarketOrderInfo *best = entityStore.Get<MarketOrderInfo>(bids.front());

			quote.bestBid = best != nullptr ? best->limitPrice : Decimal{};
			std::tie(quote.bidOrders, quote.bidDepth) = AtLevel(entityStore, bids, quote.bestBid);
		}

		if (quote.bestAsk > 0 && quote.bestBid > 0 && quote.bestBid <= quote.bestAsk)
		{
			quote.mid =
			    Decimal::FromRaw(quote.bestBid.Raw() + (quote.bestAsk.Raw() - quote.bestBid.Raw()) / 2);

			if (quote.mid > 0)
			{
				quote.spreadBps =
				    (quote.bestAsk - quote.bestBid).ToDouble() / quote.mid.ToDouble() * 10000.0;
			}
		}

		const auto lastPrice = record.lastPrice.find(itemId);
		const auto lastTick = record.lastTick.find(itemId);
		const auto volume = record.volume.find(itemId);

		if (lastPrice != record.lastPrice.end())
		{
			quote.lastPrice = lastPrice->second;
		}

		if (lastTick != record.lastTick.end())
		{
			quote.lastTick = lastTick->second;
		}

		if (volume != record.volume.end())
		{
			quote.volume = volume->second;
		}

		record.quotes[itemId] = quote;
	}

	return entityStore.Put<MarketRecord>(marketId, std::move(record)) != nullptr;
}


std::vector<MarketQuote> MarketplaceService::Quotes(
    const EntityStore &entityStore, const std::string &marketId) const
{
	std::vector<MarketQuote> quotes;

	const auto *record = entityStore.Get<MarketRecord>(marketId);

	if (record == nullptr)
	{
		return quotes;
	}

	for (const auto &entry : record->quotes)
	{
		quotes.push_back(entry.second);
	}

	return quotes;
}


const MarketQuote *MarketplaceService::QuoteFor(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId) const
{
	const auto *record = entityStore.Get<MarketRecord>(marketId);

	if (record == nullptr)
	{
		return nullptr;
	}

	const auto entry = record->quotes.find(itemId);

	return entry != record->quotes.end() ? &entry->second : nullptr;
}


bool MarketplaceService::BestAsk(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId,
    Decimal &price) const
{
	const MarketQuote *quote = QuoteFor(entityStore, marketId, itemId);

	if (quote == nullptr || quote->bestAsk <= 0)
	{
		return false;
	}

	price = quote->bestAsk;

	return true;
}


bool MarketplaceService::BestBid(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId,
    Decimal &price) const
{
	const MarketQuote *quote = QuoteFor(entityStore, marketId, itemId);

	if (quote == nullptr || quote->bestBid <= 0)
	{
		return false;
	}

	price = quote->bestBid;

	return true;
}


bool MarketplaceService::MidPrice(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId,
    Decimal &price) const
{
	const MarketQuote *quote = QuoteFor(entityStore, marketId, itemId);

	if (quote == nullptr || quote->mid <= 0)
	{
		return false;
	}

	price = quote->mid;

	return true;
}


double MarketplaceService::SpreadBps(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId) const
{
	const MarketQuote *quote = QuoteFor(entityStore, marketId, itemId);

	return quote != nullptr ? quote->spreadBps : 0.0;
}


Decimal MarketplaceService::ReferencePrice(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId) const
{
	const MarketQuote *quote = QuoteFor(entityStore, marketId, itemId);

	if (quote == nullptr)
	{
		return {};
	}

	if (quote->mid > 0)
	{
		return quote->mid;
	}

	if (quote->bestAsk > 0)
	{
		return quote->bestAsk;
	}

	if (quote->bestBid > 0)
	{
		return quote->bestBid;
	}

	return quote->lastPrice;
}


int MarketplaceService::RiskPremiumBps(
    const EntityStore &entityStore, const std::string &marketId) const
{
	const auto *venue = entityStore.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		return 0;
	}

	// The venue reads the components its civilisation's tiers wrote, and the lanes of
	// the same civilisation: the risk of trading at a venue is the risk of the place
	// and the risk of the lane the goods came down. The components are written by
	// CivilisationEffectEngine, and this reads them, which is the half of the split
	// that keeps a marketplace from knowing anything about the skill tree.
	double risk = 0.0;

	if (const auto *effects = entityStore.Get<MarketEffects>(marketId))
	{
		risk += ReadEffectStat(effects->values, EffectStat::MarketRiskPremium);
	}

	const auto laneId = CivilisationEffectEngine::LaneStoreId(venue->civilisationId);

	if (const auto *lanes = entityStore.Get<RouteEffects>(laneId))
	{
		risk += ReadEffectStat(lanes->values, EffectStat::RouteRiskPremium);
	}

	return static_cast<int>(std::llround(BoundedBps(risk)));
}


int MarketplaceService::ListingFeeBps(const EntityStore &entityStore, const std::string &marketId) const
{
	const auto *venue = entityStore.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		return m_options.listingFeeBps;
	}

	long long fee =
	    static_cast<long long>(venue->baseListingFeeBps) + RiskPremiumBps(entityStore, marketId);

	// The credit for the berth: one basis point off the fee for every two berths a day
	// the licence and the warrant clear, because a licence that clears the deck faster
	// is a deck that fills the same goods at a cheaper berth.
	if (const auto *effects = entityStore.Get<MarketEffects>(marketId))
	{
		const double clearance = ReadEffectStat(effects->values, EffectStat::MarketClearance);

		fee -= static_cast<long long>(BoundedBps(clearance / 2.0));
	}

	return static_cast<int>(std::clamp<long long>(fee, 0, static_cast<long long>(kMaxBps)));
}


int MarketplaceService::InsuranceBps(const EntityStore &entityStore, const std::string &marketId) const
{
	const auto *venue = entityStore.Get<MarketplaceInfo>(marketId);
	long long bps = venue != nullptr ? m_options.insuranceBps : 0;

	if (const auto *effects = entityStore.Get<MarketEffects>(marketId))
	{
		bps += static_cast<long long>(
		    BoundedBps(ReadEffectStat(effects->values, EffectStat::MarketInsurance)));
	}

	return static_cast<int>(std::clamp<long long>(bps, 0, static_cast<long long>(kMaxBps)));
}


double MarketplaceService::DepthUnits(const EntityStore &entityStore, const std::string &marketId) const
{
	const auto *venue = entityStore.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr || m_options.depthUnits <= 0.0)
	{
		// No cap: a venue that has set no depth is a venue that quoters arrive at, not
		// a venue that turns them away, and the scaffold's default is that.
		return 0.0;
	}

	double depth = m_options.depthUnits;

	if (const auto *effects = entityStore.Get<MarketEffects>(marketId))
	{
		depth += ReadEffectStat(effects->values, EffectStat::MarketDepth);
	}

	return depth > 0.0 ? depth : 0.0;
}


bool MarketplaceService::ApplyProgression(
    EntityStore &entityStore, const ProgressionManager &progression,
    const CivilisationEffectEngine &effects, const std::string &civilisationId, unsigned &applied,
    unsigned &skipped, std::string &errorMessage)
{
	if (!effects.ApplyProgression(
	        entityStore, progression, civilisationId, applied, skipped, errorMessage))
	{
		return false;
	}

	// The clearance of a venue moves its fee, and the fee is the number beside the
	// price in every quote the UI shows, so a tree that has been bought is a set of
	// venues whose charges have to be re-read.
	for (const auto &marketId : effects.MarketsOf(entityStore, civilisationId))
	{
		if (!RefreshQuotes(entityStore, marketId, errorMessage))
		{
			return false;
		}
	}

	return true;
}


unsigned MarketplaceService::AdvanceTick(
    EntityStore &entityStore, long long tick, std::string &errorMessage)
{
	unsigned venues = 0;

	// Validate the complete expiry batch, including refunds shared across venues. An expiry
	// that does not run past the tick the order was posted at is an order posted with no
	// order life, which rests until it trades, is cancelled, or the venue closes.
	std::vector<std::string> allExpired;
	for (const auto entity : entityStore.View<MarketOrderInfo, OrderAt>())
	{
		const auto *info = entityStore.Get<MarketOrderInfo>(entity);
		const auto *at = entityStore.Get<OrderAt>(entity);
		if (info != nullptr && at != nullptr && HasMarket(entityStore, at->marketId) &&
		    info->status == OrderStatus::Active && info->expiresTick > info->placedTick &&
		    info->expiresTick <= tick)
		{
			const auto name = entityStore.NameOf(entity);
			if (!name.empty())
			{
				allExpired.push_back(name);
			}
		}
	}
	std::sort(allExpired.begin(), allExpired.end());
	if (!CanRefundOrders(entityStore, allExpired, errorMessage))
	{
		return 0;
	}

	// The orders that have outlived their order life leave, and their goods and their
	// credits go back: the berths they held are the berths this tick's quoters need.
	for (const auto &orderId : allExpired)
	{
		if (!RetireOrder(entityStore, orderId, errorMessage))
		{
			return 0;
		}
	}

	for (const auto &marketId : MarketIds(entityStore, {}))
	{
		if (!RefreshQuotes(entityStore, marketId, errorMessage))
		{
			return venues;
		}

		++venues;
	}

	return venues;
}


std::vector<TradeFill> MarketplaceService::Fills(
    const EntityStore &entityStore, const std::string &marketId) const
{
	std::vector<TradeFill> fills;

	const auto *record = entityStore.Get<MarketRecord>(marketId);

	if (record == nullptr)
	{
		return fills;
	}

	return record->fills;
}


std::string MarketplaceService::DescribeMarket(
    const EntityStore &entityStore, const std::string &marketId) const
{
	const auto *venue = entityStore.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		return "Market '" + marketId + "': the world holds no such venue";
	}

	const auto *body = entityStore.Get<BodyInfo>(marketId);

	std::ostringstream out;

	out << "Market " << venue->marketId << " at "
	    << (venue->venueName.empty() ? venue->marketId : venue->venueName) << " ("
	    << DescribeBodyKind(body != nullptr ? body->kind : BodyKind::Planet) << " of "
	    << venue->civilisationId << ")";

	if (body != nullptr && body->kind == BodyKind::Satellite)
	{
		out << ", in orbit around " << body->primaryBodyId;
	}

	out << "\n  Charges " << ListingFeeBps(entityStore, venue->marketId) << " bps a side (base "
	    << venue->baseListingFeeBps << ", risk " << RiskPremiumBps(entityStore, venue->marketId)
	    << "), insurance " << InsuranceBps(entityStore, venue->marketId) << " bps, orders rest "
	    << venue->orderLifeTicks << " ticks, take " << venue->venueTake << " credits\n";

	out << "  " << OpenOrderCount(entityStore, venue->marketId) << " open orders, ";

	const std::vector<MarketQuote> quotes = Quotes(entityStore, venue->marketId);

	out << quotes.size() << " goods quoted";

	if (quotes.empty())
	{
		out << ": nothing traded here yet";

		return out.str();
	}

	// The most traded goods first, and inside a volume the item id: a venue quotes
	// dozens of goods and a screen shows a handful.
	std::vector<MarketQuote> ranked = quotes;

	std::sort(
	    ranked.begin(), ranked.end(),
	    [](const MarketQuote &left, const MarketQuote &right)
	    {
		    if (left.volume != right.volume)
		    {
			    return left.volume > right.volume;
		    }

		    return left.itemId < right.itemId;
	    });

	for (std::size_t index = 0; index < ranked.size() && index < 5; ++index)
	{
		const MarketQuote &quote = ranked[index];

		out << "\n  " << quote.itemId << ": ask " << quote.bestAsk.ToString() << " bid "
		    << quote.bestBid.ToString() << ", mid " << quote.mid.ToString() << ", spread "
		    << FormatNumber(quote.spreadBps) << " bps, depth " << FormatNumber(quote.askDepth) << "/"
		    << FormatNumber(quote.bidDepth) << ", last " << quote.lastPrice.ToString() << " at tick "
		    << quote.lastTick << ", volume " << FormatNumber(quote.volume);
	}

	return out.str();
}


std::string MarketplaceService::DescribeOrder(
    const EntityStore &entityStore, const std::string &orderId) const
{
	const auto *info = entityStore.Get<MarketOrderInfo>(orderId);

	if (info == nullptr)
	{
		return "Order '" + orderId +
		       "': no order in the world by that name - it filled, expired, was cancelled, or "
		       "was never posted";
	}

	const auto *at = entityStore.Get<OrderAt>(orderId);
	const auto *owner = entityStore.Get<OrderOwner>(orderId);

	std::ostringstream out;

	out << "Order " << info->orderId << " " << DescribeOrderSide(info->side) << " "
	    << FormatNumber(info->quantity) << " " << info->itemId << " @ " << info->limitPrice.ToString()
	    << " at " << (at != nullptr ? at->marketId : std::string("<no venue>")) << " for "
	    << (owner != nullptr ? owner->civilisationId : std::string("<no trader>")) << ", "
	    << DescribeOrderStatus(info->status) << ", " << FormatNumber(info->filled)
	    << " filled, posted at tick " << info->placedTick << ", leaves at tick " << info->expiresTick;

	return out.str();
}


bool MarketplaceService::LoadSimulation(
    EntityStore &entityStore, const Simulation &sim, const MarketplaceOptions &options,
    std::size_t &venuesOpened, std::string &errorMessage)
{
	venuesOpened = 0;

	if (entityStore.EntityCount() != 0)
	{
		errorMessage = std::format(
		    "MarketplaceService: the world holds {} entities: the generated universe loads into a "
		    "fresh world, because the ids of a second load collide with the first",
		    std::to_string(entityStore.EntityCount()));

		return false;
	}

	if (!sim.IsGenerated())
	{
		errorMessage = "MarketplaceService: the simulation has not generated any worlds";

		return false;
	}

	MarketplaceService service(options);

	for (const Civilisation &world : sim.GetCivilisations())
	{
		if (!AddPlanet(
		        entityStore, world.id, world.name, world.systemName, world.id, world.ageId,
		        errorMessage))
		{
			errorMessage.insert(0, "MarketplaceService: ");

			return false;
		}

		// The economy model's own numbers, in the store's own components: the same
		// stockpile, the same treasury, the same decks, so a market settles against the
		// world the tick model produced and consumed.
		entityStore.Put<Population>(world.id, Population{world.population, world.population});
		entityStore.Put<Treasury>(world.id, Treasury{world.treasury});
		entityStore.Put<Stockpile>(world.id, Stockpile{world.stockpile});
		entityStore.Put<Production>(world.id, Production{world.production});
		entityStore.Put<Demand>(world.id, Demand{world.demand});

		if (!service.Open(entityStore, world.id, errorMessage))
		{
			errorMessage = std::format("MarketplaceService: world '{}': {}", world.id, errorMessage);

			return false;
		}

		++venuesOpened;
	}

	return true;
}


bool MarketplaceService::AddPlanet(
    EntityStore &entityStore, const std::string &bodyId, const std::string &displayName,
    const std::string &systemName, const std::string &civilisationId, const std::string &ageId,
    std::string &errorMessage)
{
	if (bodyId.empty())
	{
		errorMessage = "MarketplaceService: the bodyId is empty";

		return false;
	}

	if (civilisationId.empty())
	{
		errorMessage = std::format(
		    "MarketplaceService: the planet '{}' belongs to no civilisation, and a venue's fee, risk "
		    "and take belong to a world",
		    bodyId);

		return false;
	}

	EntityStore::Entity body = EntityStore::Null;

	if (!entityStore.Create(bodyId, body, errorMessage))
	{
		return false;
	}

	BodyInfo info;
	info.bodyId = bodyId;
	info.kind = BodyKind::Planet;
	info.displayName = displayName;
	info.systemName = systemName;
	info.civilisationId = civilisationId;
	info.ageId = ageId;

	entityStore.Add<BodyInfo>(body, std::move(info));

	// A world created here settles: an empty treasury and an empty stockpile are the
	// facts a market needs to post against, and a body that cannot settle is a body a
	// trader cannot trade at.
	entityStore.Add<Treasury>(body, Treasury{0});
	entityStore.Add<Stockpile>(body, Stockpile{});
	entityStore.Add<Population>(body, Population{1.0, 1.0});

	return true;
}


bool MarketplaceService::AddSatellite(
    EntityStore &entityStore, const std::string &satelliteId, const std::string &planetBodyId,
    const std::string &displayName, double radiusKm, double periodDays, std::string &errorMessage)
{
	if (satelliteId.empty())
	{
		errorMessage = "MarketplaceService: the satelliteId is empty";

		return false;
	}

	const BodyInfo *planet = entityStore.Get<BodyInfo>(planetBodyId);

	if (planet == nullptr)
	{
		errorMessage = std::format(
		    "MarketplaceService: the satellite '{}' orbits '{}', which is not a body of this world",
		    satelliteId, planetBodyId);

		return false;
	}

	if (planet->kind != BodyKind::Planet)
	{
		errorMessage = std::format(
		    "MarketplaceService: the satellite '{}' orbits {} '{}'; the station trade is the trade of "
		    "the ships that come up a planet's gravity well, and a star has no market at the bottom of "
		    "its",
		    satelliteId, DescribeBodyKind(planet->kind), planetBodyId);

		return false;
	}

	EntityStore::Entity body = EntityStore::Null;

	if (!entityStore.Create(satelliteId, body, errorMessage))
	{
		return false;
	}

	BodyInfo info;
	info.bodyId = satelliteId;
	info.kind = BodyKind::Satellite;
	info.displayName = displayName.empty() ? satelliteId : displayName;
	info.primaryBodyId = planet->bodyId;
	info.systemName = planet->systemName;

	// A station belongs to the world beneath it: the crews that trade there are the
	// crews of the ships that come up from that world's market, and the fee, the risk
	// and the take of the station's venue are that civilisation's.
	info.civilisationId = planet->civilisationId;

	entityStore.Add<BodyInfo>(body, std::move(info));
	entityStore.Add<Orbit>(body, Orbit{planet->bodyId, radiusKm, periodDays});
	entityStore.Add<Treasury>(body, Treasury{0});
	entityStore.Add<Stockpile>(body, Stockpile{});

	return true;
}


long long MarketplaceService::FeeFor(long long gross, int feeBps)
{
	const auto fee = CheckedFeeFor(gross, feeBps);
	JETTISON_VERIFY(fee.has_value());
	return *fee;
}


double MarketplaceService::AffordableQuantity(Decimal price, int feeBps, long long budget)
{
	if (price <= 0 || budget <= 0)
	{
		return 0.0;
	}

	// The whole the budget pays: the goods and the fee on them, at the fee rate, in
	// integer arithmetic, and the units that whole buys.
	const __int128 rate = static_cast<__int128>(10000) + std::max(feeBps, 0);
	const __int128 whole = static_cast<__int128>(budget) * 10000 / rate;

	if (whole <= 0)
	{
		return 0.0;
	}

	return static_cast<double>(whole) / price.ToDouble();
}


const MarketplaceInfo *MarketplaceService::RequireVenue(
    const EntityStore &entityStore, const std::string &marketId, std::string &errorMessage) const
{
	if (marketId.empty())
	{
		errorMessage =
		    "Marketplace: the marketId is empty; a market is named by the body id of its venue";

		return nullptr;
	}

	const auto *venue = entityStore.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		errorMessage =
		    std::format("Marketplace '{}': no marketplace at that body; Open it first", marketId);

		return nullptr;
	}

	return venue;
}


bool MarketplaceService::RequireTrader(
    const EntityStore &entityStore, const std::string &civilisationId, EntityStore::Entity &trader,
    std::string &errorMessage) const
{
	trader = entityStore.Find(civilisationId);

	if (!entityStore.IsValid(trader))
	{
		errorMessage = std::format(
		    "Marketplace: '{}' is not an entity of this world: a trader is a world, and a world is a "
		    "body",
		    civilisationId);

		return false;
	}

	if (entityStore.Get<Treasury>(trader) == nullptr || entityStore.Get<Stockpile>(trader) == nullptr)
	{
		errorMessage = std::format(
		    "Marketplace: '{}' carries no Treasury and Stockpile, and a trader without both cannot "
		    "settle a fill",
		    civilisationId);

		return false;
	}

	return true;
}


const MarketOrderInfo *MarketplaceService::RequireOrder(
    const EntityStore &entityStore, const std::string &orderId, std::string &errorMessage) const
{
	if (orderId.empty())
	{
		errorMessage = "Marketplace: the orderId is empty";

		return nullptr;
	}

	const auto *info = entityStore.Get<MarketOrderInfo>(orderId);

	if (info == nullptr)
	{
		errorMessage = std::format(
		    "Marketplace: '{}' is no order of this world: it filled, expired, was cancelled, or was "
		    "never posted",
		    orderId);

		return nullptr;
	}

	return info;
}


std::vector<EntityStore::Entity> MarketplaceService::ActiveOrders(
    const EntityStore &entityStore, const std::string &marketId, const std::string &itemId,
    OrderSide side) const
{
	std::vector<EntityStore::Entity> orders;

	const auto view = entityStore.View<MarketOrderInfo, OrderAt>();

	for (const EntityStore::Entity entity : view)
	{
		const auto *info = entityStore.Get<MarketOrderInfo>(entity);
		const auto *at = entityStore.Get<OrderAt>(entity);

		if (info == nullptr || at == nullptr)
		{
			continue;
		}

		if (!marketId.empty() && at->marketId != marketId)
		{
			continue;
		}

		if (!itemId.empty() && info->itemId != itemId)
		{
			continue;
		}

		if (info->side != side || info->status != OrderStatus::Active)
		{
			continue;
		}

		if (OpenQuantity(*info) <= quantityEpsilon)
		{
			continue;
		}

		orders.push_back(entity);
	}

	SortBestFirst(entityStore, orders, side);

	return orders;
}


bool MarketplaceService::RetireOrder(
    EntityStore &entityStore, const std::string &orderId, std::string &errorMessage)
{
	const MarketOrderInfo *info = entityStore.Get<MarketOrderInfo>(orderId);

	if (info == nullptr)
	{
		// The order is gone, and a thing that is gone has left nothing to refund.
		return true;
	}

	const OrderOwner *owner = entityStore.Get<OrderOwner>(orderId);

	// The reservation goes back: what the book held for an order that did not fill is
	// the trader's again, in full, and the ledger of the fill log is the only record of
	// what did trade.
	if (owner != nullptr)
	{
		EntityStore::Entity trader = entityStore.Find(owner->civilisationId);

		if (entityStore.IsValid(trader))
		{
			const auto *refundTreasury = entityStore.Get<Treasury>(trader);
			if (refundTreasury != nullptr &&
			    static_cast<__int128>(refundTreasury->credits) + info->reservedCredits >
			        std::numeric_limits<long long>::max())
			{
				errorMessage = "Marketplace: reservation refund exceeds the credit range";
				return false;
			}
			if (info->reservedCredits > 0)
			{
				if (auto *treasury = entityStore.Get<Treasury>(trader))
				{
					treasury->credits += info->reservedCredits;
				}
			}

			if (info->reservedGoods > quantityEpsilon)
			{
				if (auto *stock = entityStore.Get<Stockpile>(trader))
				{
					stock->units[info->itemId] += info->reservedGoods;
				}
			}
		}
	}

	return entityStore.Destroy(orderId, errorMessage);
}


bool MarketplaceService::FillPair(
    EntityStore &entityStore, const std::string &marketId, EntityStore::Entity makerEntity,
    EntityStore::Entity takerEntity, std::vector<TradeFill> &fillLog, std::string &errorMessage)
{
	auto *venue = entityStore.Get<MarketplaceInfo>(marketId);
	auto *maker = entityStore.Get<MarketOrderInfo>(makerEntity);
	auto *taker = entityStore.Get<MarketOrderInfo>(takerEntity);

	if (venue == nullptr || maker == nullptr || taker == nullptr)
	{
		errorMessage =
		    std::format("Marketplace '{}': the pair to fill is not a pair of live orders", marketId);

		return false;
	}

	if (maker->side == taker->side)
	{
		errorMessage = std::format(
		    "Marketplace '{}': two {} orders cannot trade with each other", marketId,
		    DescribeOrderSide(maker->side));

		return false;
	}

	if (maker->itemId != taker->itemId)
	{
		errorMessage = std::format(
		    "Marketplace '{}': '{}' cannot trade against '{}'", marketId, maker->itemId, taker->itemId);

		return false;
	}

	const OrderOwner *makerOwner = entityStore.Get<OrderOwner>(makerEntity);
	const OrderOwner *takerOwner = entityStore.Get<OrderOwner>(takerEntity);

	if (makerOwner == nullptr || takerOwner == nullptr)
	{
		errorMessage =
		    std::format("Marketplace '{}': an order of the pair carries no OrderOwner", marketId);

		return false;
	}

	const bool makerSells = maker->side == OrderSide::Sell;
	const std::string &sellerId = makerSells ? makerOwner->civilisationId : takerOwner->civilisationId;
	const std::string &buyerId = makerSells ? takerOwner->civilisationId : makerOwner->civilisationId;

	// The cargo and the money live on the trader, not on the order: a fill of a venue
	// that requires neither stock nor funds - the game of promises, off by default -
	// draws the stockpile of the seller and the treasury of the buyer, and it reads
	// them where they are, through the ids the orders name, as SettleParty and
	// RetireOrder read them. An order entity carries no Stockpile and no Treasury, so
	// a lookup on the order answers null for both, and both branches below then set
	// the quantity to nothing: the pair trades nothing and says nothing, at a venue
	// that requires funds and stock never noticed, because the reservations cover it.
	const EntityStore::Entity sellerTrader = entityStore.Find(sellerId);
	const EntityStore::Entity buyerTrader = entityStore.Find(buyerId);

	MarketOrderInfo *sellOrder = makerSells ? maker : taker;
	MarketOrderInfo *buyOrder = makerSells ? taker : maker;

	// The maker's limit price is the price: the resting trader set it, and the trader
	// that came looking took the book as it found it.
	const Decimal price = maker->limitPrice;

	if (price <= 0)
	{
		errorMessage =
		    std::format("Marketplace '{}': the maker '{}' names no price", marketId, maker->orderId);

		return false;
	}

	const std::string itemId = maker->itemId;

	double quantity = std::min(OpenQuantity(*maker), OpenQuantity(*taker));

	// The goods: a sell order that posted under a venue that requires stock has the
	// units in the reservation, and the fill spends the reservation. A venue that
	// allows a sell order for cargo no one holds - the game of promises, off by default
	// - spends the seller's stockpile at the fill, and takes only what is there.
	if (sellOrder->reservedGoods > quantityEpsilon)
	{
		quantity = std::min(quantity, sellOrder->reservedGoods);
	}
	else if (const Stockpile *stock = entityStore.Get<Stockpile>(sellerTrader))
	{
		const auto holding = stock->units.find(itemId);
		const double available = holding != stock->units.end() ? holding->second : 0.0;

		quantity = std::min(quantity, available);
	}
	else
	{
		errorMessage = std::format(
		    "Marketplace '{}': the sell order '{}' names '{}', which carries no Stockpile to spend on "
		    "a fill",
		    marketId, sellOrder->orderId, sellerId);

		return false;
	}

	// The credits: a buy order that posted under a venue that requires funds has the
	// money in the reservation, and the fill spends the reservation at the price it
	// trades at, which is never worse than the limit the reservation was priced at, and
	// at the fee the order posted at. A venue that allows a buy order the buyer cannot
	// pay - off by default - spends the treasury at the fill, and takes only what the
	// money buys.
	if (m_options.requireFunds)
	{
		const double affordable =
		    AffordableQuantity(price, buyOrder->feeBps, buyOrder->reservedCredits);

		// The rounding of many partial fills can spend a reservation before its last
		// units: what is left of the order cannot pay, so it leaves the book with what it
		// holds rather than block the orders behind it from meeting the ask.
		if (affordable <= quantityEpsilon)
		{
			const std::string buyOrderId = buyOrder->orderId;

			return RetireOrder(entityStore, buyOrderId, errorMessage);
		}

		quantity = std::min(quantity, affordable);
	}
	else if (const Treasury *treasury = entityStore.Get<Treasury>(buyerTrader))
	{
		quantity = std::min(quantity, AffordableQuantity(price, buyOrder->feeBps, treasury->credits));
	}
	else
	{
		errorMessage = std::format(
		    "Marketplace '{}': the buy order '{}' names '{}', which carries no Treasury to spend on a "
		    "fill",
		    marketId, buyOrder->orderId, buyerId);

		return false;
	}

	if (quantity <= quantityEpsilon)
	{
		// Nothing of a whole unit can trade: the book has the price and the trader has
		// run out of the thing the price is paid in.
		return false;
	}

	const auto cost = WholeCost(price, quantity, false);
	if (!cost)
	{
		errorMessage = "Marketplace: fill gross exceeds the credit range";
		return false;
	}
	const long long gross = *cost;
	const auto buyerFeeOf = CheckedFeeFor(gross, buyOrder->feeBps);
	const auto sellerFeeOf = CheckedFeeFor(gross, sellOrder->feeBps);
	if (!buyerFeeOf || !sellerFeeOf)
	{
		errorMessage = "Marketplace: fill fee exceeds the credit range";
		return false;
	}
	const long long buyerFee = *buyerFeeOf;
	const long long sellerFee = *sellerFeeOf;
	const long long makerFee = makerSells ? sellerFee : buyerFee;
	const long long takerFee = makerSells ? buyerFee : sellerFee;

	// Validate both parties, the venue and the eventual refund before settlement writes.
	auto *buyerTreasury = entityStore.Get<Treasury>(buyerTrader);
	auto *sellerTreasury = entityStore.Get<Treasury>(sellerTrader);
	auto *buyerStock = entityStore.Get<Stockpile>(buyerTrader);
	auto *sellerStock = entityStore.Get<Stockpile>(sellerTrader);
	const __int128 bill = static_cast<__int128>(gross) + buyerFee;
	const __int128 take = static_cast<__int128>(venue->venueTake) + makerFee + takerFee;
	const auto fits = [](__int128 value)
	{ return value >= 0 && value <= std::numeric_limits<long long>::max(); };
	if (buyerTreasury == nullptr || sellerTreasury == nullptr || buyerStock == nullptr ||
	    sellerStock == nullptr || !fits(bill) || !fits(take))
	{
		errorMessage = "Marketplace: settlement exceeds the credit range or lacks a ledger";
		return false;
	}
	const auto billCredits = static_cast<long long>(bill);
	const long long fromOrder = std::min(billCredits, buyOrder->reservedCredits);
	const long long fromTreasury = billCredits - fromOrder;
	const bool selfTrade = buyerTrader == sellerTrader;
	const __int128 nextBuyer = static_cast<__int128>(buyerTreasury->credits) - fromTreasury;
	const __int128 proceeds = static_cast<__int128>(gross) - sellerFee;
	const bool completesBuy = OpenQuantity(*buyOrder) - quantity <= quantityEpsilon;
	const long long refund = completesBuy ? buyOrder->reservedCredits - fromOrder : 0;
	const __int128 finalBuyer = nextBuyer + refund + (selfTrade ? proceeds : 0);
	const __int128 finalSeller = static_cast<__int128>(sellerTreasury->credits) + proceeds;
	const double holding = buyerStock->units.contains(itemId) ? buyerStock->units.at(itemId) : 0.0;
	if (!fits(nextBuyer) || !fits(finalBuyer) || (!selfTrade && !fits(finalSeller)) ||
	    !std::isfinite(holding + quantity))
	{
		errorMessage = "Marketplace: settlement or refund exceeds the available ledger range";
		return false;
	}

	// Each side pays the fee its own order posted at, and the venue is paid by both,
	// because the berth and the deck are the venue's. The buyer's money comes out of the
	// pocket its order holds, and the seller's proceeds go to its treasury.
	if (!SettleParty(
	        entityStore, buyerId, itemId, quantity, gross, buyerFee, buyOrder->reservedCredits, true,
	        errorMessage))
	{
		return false;
	}

	if (!SettleParty(
	        entityStore, sellerId, itemId, quantity, gross, sellerFee, sellOrder->reservedCredits,
	        false, errorMessage))
	{
		return false;
	}

	// A sell order with no reservation spends the seller's stockpile at the fill: the
	// promises venue, and the goods are taken from where they are.
	if (sellOrder->reservedGoods <= quantityEpsilon)
	{
		if (auto *stock = entityStore.Get<Stockpile>(sellerTrader))
		{
			stock->units[itemId] -= quantity;
		}
	}

	sellOrder->reservedGoods = std::max(0.0, sellOrder->reservedGoods - quantity);
	sellOrder->filled += quantity;

	// The buyer's pocket was spent by the settlement, at the price the fill traded at,
	// which is never worse than the limit the pocket was priced at; the cargo left the
	// seller's order the same way, and what is left of either is what the order gives
	// back to its trader when it leaves the book.
	buyOrder->filled += quantity;

	venue->venueTake += makerFee + takerFee;

	TradeFill fill;
	fill.marketId = venue->marketId;
	fill.itemId = itemId;
	fill.makerOrderId = maker->orderId;
	fill.takerOrderId = taker->orderId;
	fill.sellerId = sellerId;
	fill.buyerId = buyerId;
	fill.quantity = quantity;
	fill.unitPrice = price;
	fill.gross = gross;
	fill.makerFee = makerFee;
	fill.takerFee = takerFee;

	// The fill happens at the taker's tick: the trader that came looking is the one
	// that moves now, and the resting order was priced at the tick before it.
	fill.tick = taker->placedTick;

	fillLog.push_back(fill);

	// The venue's record of the trade: the price, its tick, the volume, and the fill in
	// the log, oldest last and capped, so a long game does not grow the store for ever.
	MarketRecord record;

	if (const MarketRecord *existing = entityStore.Get<MarketRecord>(marketId); existing != nullptr)
	{
		record = *existing;
	}

	record.volume[itemId] += quantity;
	record.lastPrice[itemId] = price;
	record.lastTick[itemId] = fill.tick;
	record.fills.push_back(fill);

	if (record.fills.size() > marketFillLogSize)
	{
		record.fills.erase(
		    record.fills.begin(), record.fills.begin() + static_cast<std::ptrdiff_t>(
		                                                     record.fills.size() - marketFillLogSize));
	}

	entityStore.Put<MarketRecord>(marketId, std::move(record));

	// An order that has filled leaves the book: its entity goes, and the id stays in
	// the fill log as the number it was posted with.
	for (const EntityStore::Entity settled : {makerEntity, takerEntity})
	{
		const MarketOrderInfo *done = entityStore.Get<MarketOrderInfo>(settled);

		if (done == nullptr || !IsFulfilled(*done))
		{
			continue;
		}

		const std::string name = entityStore.NameOf(settled);

		if (!name.empty())
		{
			MarketOrderInfo settledInfo = *done;
			settledInfo.status = OrderStatus::Filled;

			entityStore.Put<MarketOrderInfo>(settled, std::move(settledInfo));

			if (!RetireOrder(entityStore, name, errorMessage))
			{
				return false;
			}
		}
	}

	return true;
}


bool MarketplaceService::SettleParty(
    EntityStore &entityStore, const std::string &civilisationId, const std::string &itemId,
    double quantity, long long gross, long long fee, long long &orderCredits, bool buys,
    std::string &errorMessage)
{
	const EntityStore::Entity trader = entityStore.Find(civilisationId);

	if (!entityStore.IsValid(trader))
	{
		errorMessage =
		    std::format("Marketplace: '{}' is not an entity of this world at the fill", civilisationId);

		return false;
	}

	auto *treasury = entityStore.Get<Treasury>(trader);
	auto *stock = entityStore.Get<Stockpile>(trader);

	if (treasury == nullptr || stock == nullptr)
	{
		errorMessage = std::format(
		    "Marketplace: '{}' carries no Treasury and Stockpile, and a fill settles in credits and "
		    "goods",
		    civilisationId);

		return false;
	}

	if (buys)
	{
		const long long bill = gross + fee;

		// The money a buy pays is the money the venue took out of the treasury when the
		// order posted and put in the order, at the worst price the limit allows: the fill
		// spends that pocket, and the treasury pays only what the pocket does not cover,
		// which is nothing at a venue that requires funds. What is left of the pocket goes
		// back to the treasury when the order leaves the book, and a trader that paid at
		// the post is never charged a second time at the fill.
		const long long fromOrder = bill < orderCredits ? bill : orderCredits;

		orderCredits -= fromOrder;
		treasury->credits -= bill - fromOrder;
		stock->units[itemId] += quantity;
	}
	else
	{
		// The seller's cargo left its stockpile at the post and is handed over out of the
		// order it was posted in, and the proceeds of the sale arrive at its treasury,
		// less the fee of its own side of the trade.
		treasury->credits += gross - fee;
	}

	return true;
}
} // namespace Manifest::Core
