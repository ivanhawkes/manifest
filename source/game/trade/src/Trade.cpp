#include <manifest/trade/Trade.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <jettison/core/Assert.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/profile/Profile.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/spatial/SystemIndices.h>

#include <manifest/sim/Simulation.h>
#include <manifest/trade/Economy.h>
#include <manifest/travel/Travel.h>
#include <manifest/types/Decimal.h>

namespace Manifest::Trade
{
namespace
{
using Command::Actor;
using Command::Result;
using Core::Decimal;
using Jettison::Sim::SimWorld;
using World::Kind;
using World::Side;

constexpr std::int64_t kMaxCredits = std::numeric_limits<std::int64_t>::max();


// The book's orders by venue and item, in the registry's context, kept as each is made and retired,
// so a post reads its own market's orders and not the whole book's. Not saved: a registry without it,
// a world just loaded, lists its book on first reading it.
struct Markets
{
	std::map<std::string, std::map<std::string, std::vector<entt::entity>, std::less<>>, std::less<>>
	    orders;
};


void Listed(entt::registry &registry, entt::entity entity)
{
	const auto &order = registry.get<const World::Order>(entity);
	registry.ctx().get<Markets>().orders[order.venueId][order.itemId].push_back(entity);
}


void Delisted(entt::registry &registry, entt::entity entity)
{
	const auto &order = registry.get<const World::Order>(entity);
	std::vector<entt::entity> &listed =
	    registry.ctx().get<Markets>().orders[order.venueId][order.itemId];
	std::erase(listed, entity);
}


Markets &MarketsOf(entt::registry &registry)
{
	if (auto *pMarkets = registry.ctx().find<Markets>())
		return *pMarkets;
	auto &markets = registry.ctx().emplace<Markets>();
	for (const auto [entity, order] : registry.view<const World::Order>().each())
		markets.orders[order.venueId][order.itemId].push_back(entity);
	registry.on_construct<World::Order>().connect<&Listed>();
	registry.on_destroy<World::Order>().connect<&Delisted>();
	return markets;
}


// The market's orders, by entity, in no order.
std::span<const entt::entity> Listing(
    entt::registry &registry, std::string_view venueId, std::string_view itemId)
{
	const Markets &markets = MarketsOf(registry);
	const auto venue = markets.orders.find(venueId);
	if (venue == markets.orders.end())
		return {};
	const auto item = venue->second.find(itemId);
	return item == venue->second.end() ? std::span<const entt::entity>{} : item->second;
}


// A good a venue's broker prices (D-102): its native price, the curve's reference, and the stock it
// is quoted at, the scenario's seeded totes, at least one.
struct Good
{
	std::string itemId;
	Decimal native;
	std::uint64_t target{1};
	// The count of its units a tote holds.
	double unitsPerTote{0.0};
};


struct Venue
{
	// The venue's market's goods in its order, and each one's place in it by its item.
	std::vector<Good> goods;
	std::unordered_map<std::string_view, std::size_t> goodIndex;
};


// A market row a region's employer makes, or its households want, in the item's unit per day: an
// optional good's at its bucket's share of its demand, bought at up to its premium over the native
// price (D-102).
struct Row
{
	std::string venueId;
	std::string itemId;
	double perDay{0.0};
	double premium{0.0};
};


// A region's part in the economy, from its venues' markets.
struct Economy
{
	std::string regionId;
	// Where its households buy and keep their goods: the first venue serving it; empty for none.
	std::string venueId;
	// In (venue, item) order, as its employer's batches are.
	std::vector<Row> made;
	// Its households' optional goods at its venue, in the market's order.
	std::vector<Row> wanted;
};


struct Need
{
	std::string needId;
	double weight{0.0};
	double perThousandPerDay{0.0};
	// Those the scenario trades, in the need's order.
	std::vector<std::string> items;
};


// What the commands and systems run on, worked out once from the content.
struct Rules
{
	World::Identities &identities;
	// The scenario's items, each the count of its units a tote holds.
	std::map<std::string, double, std::less<>> unitsPerTote;
	std::map<std::string, Venue, std::less<>> venues;
	// The venues in the scenario's order.
	std::vector<std::string> venueOrder;
	std::uint64_t lifeSteps{0};
	std::uint64_t turnSteps{0};
	std::uint64_t windowSteps{0};
	// The scenario's venues, by ID, for the reports of their markets.
	std::map<std::string, const Content::VenueDefinition *, std::less<>> definitions;
	int feeBps{0};
	int spreadBps{0};

	// The regions in the scenario's order, and the needs in the tuning's.
	std::vector<Economy> economies;
	std::vector<Need> needs;
	// Each scenario item's base price, raw microcredits per its unit.
	std::map<std::string, std::int64_t, std::less<>> basePrices;
	double intervalSeconds{0.0};
	// The gross paid for each credit withheld; 0 withholds none.
	std::int64_t perTaxCredit{0};
	Content::ProductionTuning production;
	Content::ConditionTuning condition;
};


std::optional<std::uint64_t> WholeSteps(double seconds, double step)
{
	const double steps = seconds / step;
	const double whole = std::round(steps);
	// The steps of a 1/60 s world are not exact in binary; a part in a billion is.
	if (!std::isfinite(steps) || whole < 0.0 || whole >= 0x1p63 ||
	    std::abs(steps - whole) > 1e-9 * std::max(1.0, whole))
		return std::nullopt;
	return static_cast<std::uint64_t>(whole);
}


// How far a venue's own making and needing move its price from the market's, either way (W19's
// balance, 2026-11-21): a venue that only makes a good prices it at 0.7 of the market, one that only
// wants it at 1.3.
constexpr double kLocalBias = 0.3;


std::shared_ptr<const Rules> MakeRules(
    const SimWorld &world, World::Identities &identities, const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	const Content::ScenarioDefinition &scenario = selected->second;
	auto rules = std::make_shared<Rules>(identities);

	std::map<std::string, const Core::ItemNode *, std::less<>> items;
	for (const Content::ItemSelection &selection : scenario.items)
	{
		const auto item =
		    std::ranges::find(content.gameMode.items, selection.itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(item != content.gameMode.items.end());
		items.emplace(selection.itemId, &*item);
		rules->unitsPerTote.emplace(
		    selection.itemId, item->unitsPerCubicMetre / static_cast<double>(Core::kTotesPerIbc));
		rules->basePrices.emplace(selection.itemId, Core::Simulation::BasePrice(*item).Raw());
	}

	for (const Content::RegionDefinition &region : scenario.regions)
	{
		Economy &economy = rules->economies.emplace_back();
		economy.regionId = region.regionId;
		for (const Content::VenueDefinition &venue : scenario.venues)
		{
			const bool serves = std::ranges::find(
			                        venue.regions, region.regionId,
			                        &Content::RegionSelection::regionId) != venue.regions.end();
			if (serves && economy.venueId.empty())
				economy.venueId = venue.venueId;
			for (const Content::MarketEntry &entry : venue.market)
				if (entry.regionId == region.regionId && entry.production > 0.0)
					economy.made.push_back({venue.venueId, entry.itemId, entry.production});
		}
		std::ranges::sort(
		    economy.made, {}, [](const Row &row) { return std::tie(row.venueId, row.itemId); });
		const auto home =
		    std::ranges::find(scenario.venues, economy.venueId, &Content::VenueDefinition::venueId);
		if (home != scenario.venues.end())
			for (const Content::MarketEntry &entry : home->market)
				// A need's good is wanted by its need, its row's demand only pricing it.
				if (entry.regionId == region.regionId && entry.demand > 0.0 &&
				    std::ranges::none_of(
				        content.tuning.needs,
				        [&](const Content::NeedDefinition &need)
				        {
					        return std::ranges::find(
					                   need.items, entry.itemId, &Content::ItemSelection::itemId) !=
					               need.items.end();
				        }))
				{
					// A category in no bucket is bought at its native price, all of its demand.
					double premium = 0.0;
					double share = 1.0;
					for (const Content::DemandBucket &bucket : content.tuning.optionalGoods)
						if (std::ranges::find(
						        bucket.categories, items.at(entry.itemId)->categoryId,
						        &Content::CategorySelection::categoryId) != bucket.categories.end())
						{
							premium = bucket.premium;
							share = bucket.demandShare;
						}
					economy.wanted.push_back(
					    {home->venueId, entry.itemId, entry.demand * share, premium});
				}
	}
	for (const Content::NeedDefinition &need : content.tuning.needs)
	{
		Need &rule = rules->needs.emplace_back(need.needId, need.weight, need.perThousandPerDay);
		for (const Content::ItemSelection &item : need.items)
			if (items.contains(item.itemId))
				rule.items.push_back(item.itemId);
	}
	rules->intervalSeconds = content.tuning.economy.intervalSeconds;
	if (content.tuning.economy.wageTaxRate > 0.0)
		rules->perTaxCredit = std::llround(1.0 / content.tuning.economy.wageTaxRate);
	rules->production = content.tuning.production;
	rules->condition = content.tuning.condition;

	// The whole market's scarcity (Simulation::MarketPrice), then each venue's own making and needing,
	// up to kLocalBias of it either way: a venue that only makes a good prices it lowest, one that only
	// wants it highest, however many venues trade it.
	std::map<std::string, double, std::less<>> supply;
	std::map<std::string, double, std::less<>> demand;
	for (const Content::VenueDefinition &venue : scenario.venues)
		for (const Content::MarketEntry &entry : venue.market)
		{
			supply[entry.itemId] += entry.production;
			demand[entry.itemId] += entry.demand;
		}
	for (const Content::VenueDefinition &venue : scenario.venues)
	{
		Venue &hosted = rules->venues[venue.venueId];
		rules->venueOrder.push_back(venue.venueId);
		rules->definitions.emplace(venue.venueId, &venue);
		for (const Content::MarketEntry &entry : venue.market)
		{
			const Decimal market = Core::Simulation::MarketPrice(
			    Core::Simulation::BasePrice(*items.at(entry.itemId)), supply[entry.itemId],
			    demand[entry.itemId]);
			const double own = entry.production + entry.demand;
			const double bias = own > 0.0 ? kLocalBias * (entry.demand - entry.production) / own : 0.0;
			const std::optional<Decimal> local =
			    Decimal::TryFromDouble(market.ToDouble() * (1.0 + bias));
			JETTISON_VERIFY(local.has_value());
			hosted.goods.push_back(
			    {entry.itemId, *local, std::max<std::uint64_t>(entry.stock, 1),
			     rules->unitsPerTote.at(entry.itemId)});
		}
		for (std::size_t at = 0; at < hosted.goods.size(); ++at)
			hosted.goodIndex.emplace(hosted.goods[at].itemId, at);
	}

	const std::optional<std::uint64_t> life =
	    WholeSteps(content.tuning.market.orderLifeSeconds, world.FixedStep());
	const std::optional<std::uint64_t> turn =
	    WholeSteps(content.tuning.economy.intervalSeconds, world.FixedStep());
	if (!life)
		throw std::invalid_argument{std::format(
		    "market.orderLifeSeconds, {}, is not a whole number of {} s steps",
		    content.tuning.market.orderLifeSeconds, world.FixedStep())};
	if (!turn || *turn == 0)
		throw std::invalid_argument{std::format(
		    "economy.intervalSeconds, {}, is not a whole number of {} s steps",
		    content.tuning.economy.intervalSeconds, world.FixedStep())};
	rules->lifeSteps = *life;
	rules->turnSteps = *turn;
	rules->windowSteps = SalesWindowSteps(world, content);
	rules->feeBps = content.tuning.market.listingFeeBps;
	rules->spreadBps = content.tuning.brokers.spreadBps;
	return rules;
}


} // namespace


// ─── Money ───
// The market price rules (docs/market-prices.md), over totes (D-083, D-087).

std::optional<std::int64_t> Fee(std::int64_t gross, int feeBps)
{
	if (gross <= 0 || feeBps <= 0)
		return 0;
	const __int128 fee = (static_cast<__int128>(gross) * feeBps + 9'999) / 10'000;
	if (fee > kMaxCredits)
		return std::nullopt;
	return static_cast<std::int64_t>(fee);
}


// Whole credits for a count of the item's units at a price: the price times the units, rounded up
// or to the nearest.
std::optional<std::int64_t> Cost(std::int64_t price, double units, bool roundUp)
{
	const double value = Decimal::FromRaw(price).ToDouble() * units;
	const double whole = roundUp ? std::ceil(value) : std::round(value);
	// 2^63 is exactly representable; INT64_MAX rounds up to it.
	if (!std::isfinite(whole) || whole < 0.0 || whole >= 0x1p63)
		return std::nullopt;
	return static_cast<std::int64_t>(whole);
}


// What a buy order holds in escrow for each tote: a tote's cost at its limit, rounded up, and the fee
// on that. A fill of n totes at a price at or under the limit costs at most n of these, its gross
// rounded to the nearest and its fee rounded up, so the escrow always pays for what is left.
std::optional<std::int64_t> PerTote(std::int64_t limit, double unitsPerTote, int feeBps)
{
	const std::optional<std::int64_t> cost = Cost(limit, unitsPerTote, true);
	const std::optional<std::int64_t> fee = cost ? Fee(*cost, feeBps) : std::nullopt;
	if (!fee || *cost > kMaxCredits - *fee)
		return std::nullopt;
	return *cost + *fee;
}


namespace
{


// Copies of what one unit of work changes, written back together by Commit: everything a command or
// a system decides is planned here, so an expected refusal leaves the world as it was. An account
// or an owner reached twice, by a self-trade or a broker's own venue, is the one copy.
// Where the holding of the item at the venue is, or would go, in a stock's (venue, item) order.
template <typename Holdings>
auto Find(Holdings &holdings, const std::string &venueId, const std::string &itemId)
{
	return std::ranges::lower_bound(
	    holdings, std::tie(venueId, itemId), std::less<>{},
	    [](const World::Holding &held) { return std::tie(held.venueId, held.itemId); });
}


class Ledger
{
  public:
	Ledger(entt::registry &registry, World::Identities &identities)
	    : m_registry{registry}, m_identities{identities}
	{
	}

	World::Account &Account(Kind kind, const std::string &id)
	{
		const entt::entity entity = Require(kind, id);
		return m_accounts.try_emplace(entity, m_registry.get<const World::Account>(entity))
		    .first->second;
	}

	// Whether the account holds the credit as well as what it has.
	[[nodiscard]] bool Credit(Kind kind, const std::string &id, std::int64_t amount)
	{
		World::Account &account = Account(kind, id);
		if (amount > kMaxCredits - account.balance)
			return false;
		account.balance += amount;
		return true;
	}

	// The owner's holding of the item at the venue, made empty if it has none. Only the holdings
	// touched are staged, not the owner's whole stock.
	World::Holding &Holding(
	    Kind owner, const std::string &ownerId, const std::string &venueId, const std::string &itemId)
	{
		const entt::entity entity = Require(owner, ownerId);
		auto staged = m_holdings.find(std::tie(entity, venueId, itemId));
		if (staged != m_holdings.end())
			return staged->second;
		const std::vector<World::Holding> &holdings =
		    m_registry.get<const World::Stock>(entity).holdings;
		const auto at = Find(holdings, venueId, itemId);
		World::Holding held = at != holdings.end() && at->venueId == venueId && at->itemId == itemId
		                          ? *at
		                          : World::Holding{venueId, itemId, 0, 0};
		return m_holdings.emplace(std::tuple{entity, venueId, itemId}, std::move(held)).first->second;
	}

	World::Order &Order(entt::entity entity)
	{
		return m_orders.try_emplace(entity, m_registry.get<const World::Order>(entity)).first->second;
	}

	// The order leaves the book; Retire gives back what it holds first.
	void Close(entt::entity entity)
	{
		m_closed.insert(entity);
	}

	void Commit()
	{
		for (const auto &[entity, account] : m_accounts)
			m_registry.replace<World::Account>(entity, account);
		// Each touched stock with its staged holdings in place, and none left empty.
		std::set<entt::entity> touched;
		for (auto &[key, holding] : m_holdings)
		{
			const entt::entity entity = std::get<0>(key);
			touched.insert(entity);
			std::vector<World::Holding> &holdings = m_registry.get<World::Stock>(entity).holdings;
			const auto at = Find(holdings, holding.venueId, holding.itemId);
			if (at != holdings.end() && at->venueId == holding.venueId && at->itemId == holding.itemId)
				*at = std::move(holding);
			else
				holdings.insert(at, std::move(holding));
		}
		for (const entt::entity entity : touched)
			m_registry.patch<World::Stock>(
			    entity,
			    [](World::Stock &stock)
			    {
				    std::erase_if(
				        stock.holdings, [](const World::Holding &holding)
				        { return holding.available == 0 && holding.reserved == 0; });
			    });
		for (const auto &[entity, order] : m_orders)
			if (!m_closed.contains(entity))
				m_registry.replace<World::Order>(entity, order);
		for (const entt::entity entity : m_closed)
			m_identities.Destroy(m_registry, entity);
	}

  private:
	entt::entity Require(Kind kind, const std::string &id) const
	{
		const entt::entity entity = m_identities.Find(kind, id);
		JETTISON_VERIFY(entity != entt::null);
		return entity;
	}

	entt::registry &m_registry;
	World::Identities &m_identities;
	std::map<entt::entity, World::Account> m_accounts;
	std::map<std::tuple<entt::entity, std::string, std::string>, World::Holding, std::less<>>
	    m_holdings;
	std::map<entt::entity, World::Order> m_orders;
	std::set<entt::entity> m_closed;
};


// The order leaves the book, what it still reserves back with its owner: a sell order's totes to
// available, a buy order's escrow to its account. False when the account cannot hold the refund.
[[nodiscard]] bool Retire(Ledger &ledger, entt::entity entity, World::Order &order)
{
	if (order.side == Side::Sell)
	{
		World::Holding &holding =
		    ledger.Holding(order.ownerKind, order.ownerId, order.venueId, order.itemId);
		JETTISON_VERIFY(holding.reserved >= order.remaining);
		holding.reserved -= order.remaining;
		holding.available += order.remaining;
	}
	else if (!ledger.Credit(World::AccountOf(order.ownerKind), order.ownerId, order.escrow))
		return false;
	order.remaining = 0;
	order.escrow = 0;
	ledger.Close(entity);
	return true;
}


bool Crosses(const World::Order &taker, const World::Order &maker)
{
	return taker.side == Side::Buy ? maker.limit <= taker.limit : maker.limit >= taker.limit;
}


// A matching pass's fills in the venue's Sales: their totes in the step's entry for the item, and the
// last one's price as the item's last sale. What is older than any window still to be reported drops.
void Record(
    entt::registry &registry, const Rules &rules, const std::string &venueId, const std::string &itemId,
    std::uint64_t step, std::uint64_t totes, std::int64_t price)
{
	const entt::entity market = rules.identities.Find(Kind::Market, venueId);
	JETTISON_VERIFY(market != entt::null);
	World::Sales &sales = registry.get<World::Sales>(market);
	// No report yet to be observed, at this step or after it, has a window reaching back so far.
	std::erase_if(
	    sales.recent,
	    [&](const World::Sold &recent) { return recent.step + rules.windowSteps < step; });
	const auto at = std::ranges::lower_bound(
	    sales.recent, std::tie(step, itemId), std::less<>{},
	    [](const World::Sold &recent) { return std::tie(recent.step, recent.itemId); });
	if (at != sales.recent.end() && at->step == step && at->itemId == itemId)
		at->totes += totes;
	else
		sales.recent.insert(at, {step, itemId, totes});
	const auto last = std::ranges::lower_bound(sales.last, itemId, {}, &World::LastSale::itemId);
	if (last != sales.last.end() && last->itemId == itemId)
		*last = {itemId, step, price};
	else
		sales.last.insert(last, {itemId, step, price});
}


// The order reserved, then matched against the book, the whole planned in one ledger and written only
// when every check holds; what is left rests under a new ID.
Result Post(SimWorld &world, const Rules &rules, World::Order order)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Post");
	entt::registry &registry = world.Registry();
	const std::uint64_t step = world.Clock().Step();
	const double unitsPerTote = rules.unitsPerTote.at(order.itemId);
	order.remaining = order.totes;
	order.feeBps = rules.feeBps;
	order.escrow = 0;
	order.expiresStep = rules.lifeSteps == 0 ? 0 : step + rules.lifeSteps;
	Ledger ledger{registry, rules.identities};

	if (order.side == Side::Sell)
	{
		World::Holding &holding =
		    ledger.Holding(order.ownerKind, order.ownerId, order.venueId, order.itemId);
		if (holding.available < order.totes)
			return Result::InsufficientStock;
		holding.available -= order.totes;
		holding.reserved += order.totes;
	}
	else
	{
		const std::optional<std::int64_t> perTote = PerTote(order.limit, unitsPerTote, order.feeBps);
		if (!perTote ||
		    order.totes > static_cast<std::uint64_t>(kMaxCredits / std::max<std::int64_t>(*perTote, 1)))
			return Result::InvalidPrice;
		const std::int64_t reservation = *perTote * static_cast<std::int64_t>(order.totes);
		World::Account &account = ledger.Account(World::AccountOf(order.ownerKind), order.ownerId);
		if (account.balance < reservation)
			return Result::InsufficientFunds;
		account.balance -= reservation;
		order.escrow = reservation;
	}

	// One pass of its market's orders: the makers it crosses, and the owner's resting buys there.
	std::vector<std::pair<entt::entity, const World::Order *>> makers;
	std::size_t restingHere = 0;
	std::uint64_t buying = 0;
	for (const entt::entity entity : Listing(registry, order.venueId, order.itemId))
	{
		const auto &resting = registry.get<const World::Order>(entity);
		if (resting.side == order.side)
		{
			++restingHere;
			if (resting.side == Side::Buy && resting.ownerKind == order.ownerKind &&
			    resting.ownerId == order.ownerId)
				buying += resting.remaining;
		}
		else if ((resting.expiresStep == 0 || step < resting.expiresStep) && Crosses(order, resting))
			makers.emplace_back(entity, &resting);
	}
	if (order.side == Side::Buy)
	{
		// Room for everything it and the owner's resting buys there may still fill.
		const World::Holding &holding =
		    ledger.Holding(order.ownerKind, order.ownerId, order.venueId, order.itemId);
		const std::uint64_t held = holding.available + holding.reserved + buying;
		if (held > Core::kMaxWarehouseTotes || order.totes > Core::kMaxWarehouseTotes - held)
			return Result::CapacityExceeded;
	}
	// The makers it crosses, best price first, then in time priority; an expired one no longer trades.
	std::ranges::sort(
	    makers,
	    [buying = order.side == Side::Buy](const auto &left, const auto &right)
	    {
		    const World::Order &a = *left.second;
		    const World::Order &b = *right.second;
		    if (a.limit != b.limit)
			    return buying ? a.limit < b.limit : a.limit > b.limit;
		    return a.sequence < b.sequence;
	    });

	std::size_t fills = 0;
	std::uint64_t traded = 0;
	std::int64_t lastPrice = 0;
	// Each company's buy filled, by its ID, at its fill's price, in fill order.
	std::vector<std::pair<std::string, std::int64_t>> paid;
	for (const auto &[entity, resting] : makers)
	{
		if (order.remaining == 0 || fills == kMaxFills)
			break;
		World::Order &maker = ledger.Order(entity);
		World::Order &buy = order.side == Side::Buy ? order : maker;
		World::Order &sell = order.side == Side::Buy ? maker : order;
		const std::uint64_t totes = std::min(order.remaining, maker.remaining);
		const std::optional<std::int64_t> gross =
		    Cost(maker.limit, unitsPerTote * static_cast<double>(totes), false);
		const std::optional<std::int64_t> buyerFee = gross ? Fee(*gross, buy.feeBps) : std::nullopt;
		const std::optional<std::int64_t> sellerFee = gross ? Fee(*gross, sell.feeBps) : std::nullopt;
		if (!buyerFee || !sellerFee)
			return Result::InvalidPrice;
		JETTISON_VERIFY(*gross <= buy.escrow && *buyerFee <= buy.escrow - *gross);
		buy.escrow -= *gross + *buyerFee;
		if (!ledger.Credit(World::AccountOf(sell.ownerKind), sell.ownerId, *gross - *sellerFee) ||
		    !ledger.Credit(Kind::VenueAccount, order.venueId, *buyerFee) ||
		    !ledger.Credit(Kind::VenueAccount, order.venueId, *sellerFee))
			return Result::InvalidQuantity;
		World::Holding &sold =
		    ledger.Holding(sell.ownerKind, sell.ownerId, order.venueId, order.itemId);
		JETTISON_VERIFY(sold.reserved >= totes);
		sold.reserved -= totes;
		World::Holding &bought =
		    ledger.Holding(buy.ownerKind, buy.ownerId, order.venueId, order.itemId);
		if (bought.available + bought.reserved > Core::kMaxWarehouseTotes - totes)
			return Result::CapacityExceeded;
		bought.available += totes;
		buy.remaining -= totes;
		sell.remaining -= totes;
		if (maker.remaining == 0 && !Retire(ledger, entity, maker))
			return Result::InvalidQuantity;
		++fills;
		traded += totes;
		lastPrice = maker.limit;
		if (buy.ownerKind == Kind::Company)
			paid.emplace_back(buy.ownerId, maker.limit);
	}

	if (order.remaining == 0)
	{
		if (!ledger.Credit(World::AccountOf(order.ownerKind), order.ownerId, order.escrow))
			return Result::InvalidQuantity;
		order.escrow = 0;
	}
	else if (restingHere >= kMaxRestingOrders)
		return Result::BookFull;

	ledger.Commit();
	if (traded > 0)
		Record(registry, rules, order.venueId, order.itemId, step, traded, lastPrice);
	for (const auto &[companyId, price] : paid)
	{
		const entt::entity company = rules.identities.Find(Kind::Company, companyId);
		JETTISON_VERIFY(company != entt::null);
		std::vector<World::PaidPrice> &prices = registry.get_or_emplace<World::Paid>(company).prices;
		const auto at = std::ranges::lower_bound(prices, order.itemId, {}, &World::PaidPrice::itemId);
		if (at != prices.end() && at->itemId == order.itemId)
			at->price = price;
		else
			prices.insert(at, {order.itemId, price});
	}
	if (order.remaining > 0)
	{
		std::optional<std::string> id = rules.identities.Issue(Kind::Order);
		// No refusal can give back what the commit wrote.
		if (!id)
			throw std::overflow_error{"the order ID counter is exhausted"};
		order.sequence = rules.identities.LastIssued(Kind::Order);
		const entt::entity created = rules.identities.Create(registry, Kind::Order, *std::move(id));
		JETTISON_VERIFY(created != entt::null);
		registry.emplace<World::Order>(created, std::move(order));
	}
	return Result::Succeeded;
}


// The company the actor's principal directs: empty for none.
std::string CompanyOf(const entt::registry &registry, const Actor &actor)
{
	for (const auto [entity, identity, company] :
	     registry.view<const World::Identity, const World::Company>().each())
		if (company.principalId == actor.id)
			return identity.id;
	return {};
}


Result Placed(
    SimWorld &world, const Rules &rules, const Command::Requests &requests, const Actor &actor,
    const Place &place)
{
	std::string company = CompanyOf(world.Registry(), actor);
	if (company.empty())
		return Result::NotAuthorised;
	if (!rules.venues.contains(place.venueId) || !rules.unitsPerTote.contains(place.itemId))
		return Result::UnknownTarget;
	if (place.totes == 0 || place.totes > Core::kMaxWarehouseTotes)
		return Result::InvalidQuantity;
	if (place.limit <= 0)
		return Result::InvalidPrice;
	World::Order order;
	order.ownerKind = Kind::Company;
	order.ownerId = std::move(company);
	order.venueId = place.venueId;
	order.itemId = place.itemId;
	order.side = place.side;
	order.limit = place.limit;
	order.totes = place.totes;
	order.request = requests.Running();
	return Post(world, rules, std::move(order));
}


Result Cancelled(SimWorld &world, const Rules &rules, const Actor &actor, const Cancel &cancel)
{
	entt::registry &registry = world.Registry();
	const entt::entity entity = rules.identities.Find(Kind::Order, cancel.orderId);
	if (entity == entt::null)
		return Result::UnknownTarget;
	const World::Order &order = registry.get<const World::Order>(entity);
	if (order.ownerKind != Kind::Company || order.ownerId != CompanyOf(registry, actor))
		return Result::NotAuthorised;
	Ledger ledger{registry, rules.identities};
	if (!Retire(ledger, entity, ledger.Order(entity)))
		return Result::InvalidQuantity;
	ledger.Commit();
	return Result::Succeeded;
}


// Retires the orders in placement order, in one ledger. Their refunds were escrowed, so one that
// cannot be paid is an invariant breach.
void RetireAll(
    entt::registry &registry, World::Identities &identities,
    std::vector<std::pair<std::uint64_t, entt::entity>> orders)
{
	if (orders.empty())
		return;
	std::ranges::sort(orders);
	Ledger ledger{registry, identities};
	for (const auto &[sequence, entity] : orders)
		if (!Retire(ledger, entity, ledger.Order(entity)))
			throw std::overflow_error{
			    std::format("order {}'s refund exceeds its owner's account", sequence)};
	ledger.Commit();
}


void Expire(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Expire");
	const std::uint64_t step = world.Clock().Step();
	std::vector<std::pair<std::uint64_t, entt::entity>> expired;
	for (const auto [entity, order] : world.Registry().view<const World::Order>().each())
		if (order.expiresStep != 0 && step >= order.expiresStep)
			expired.emplace_back(order.sequence, entity);
	RetireAll(world.Registry(), rules.identities, std::move(expired));
}


// The curve's price empty, as a multiple of the native price: r C T / (T + (C - 1) h), r at the target
// and r / C from (C + 1) T up (D-102).
constexpr std::int64_t kCurveCeiling = 4;
// Basis points of a price, doubled, over which half the spread is the spread's basis points.
constexpr std::int64_t kHalfSpreadScale = 20'000;


// A tote of the good at a point of the broker's stock, in half totes so a rung is priced at its
// middle, about its native price r and target T on the curve (kCurveCeiling); half the spread above it
// for an ask, rounded up, or below it for a bid, rounded down. Exact, so every build prices alike.
std::int64_t CurvePrice(const Good &good, std::uint64_t halfTotes, int spreadBps, Side side)
{
	const __int128 target = good.target;
	const __int128 stock = std::min<__int128>(halfTotes, target * 2 * (kCurveCeiling + 1));
	const bool ask = side == Side::Sell;
	const __int128 numerator = static_cast<__int128>(good.native.Raw()) * 2 * kCurveCeiling * target *
	                           (ask ? kHalfSpreadScale + spreadBps : kHalfSpreadScale - spreadBps);
	const __int128 denominator = (2 * target + (kCurveCeiling - 1) * stock) * kHalfSpreadScale;
	const __int128 price = ask ? (numerator + denominator - 1) / denominator : numerator / denominator;
	return static_cast<std::int64_t>(std::min<__int128>(price, kMaxCredits));
}


// The totes of the item the broker holds at its venue, free or in its asks.
std::uint64_t BrokerStock(
    const entt::registry &registry, entt::entity broker, const std::string &itemId)
{
	for (const World::Holding &holding : registry.get<const World::Stock>(broker).holdings)
		if (holding.itemId == itemId)
			return holding.available + holding.reserved;
	return 0;
}


// A rung of the good's ladder: a kRungsPerTarget-th of its target, at least a tote, so the kLevels a
// side reach half the target and the quote there stays close to the native price (D-102).
constexpr std::uint64_t kRungsPerTarget = 8;
std::uint64_t Rung(const Good &good)
{
	return (good.target + kRungsPerTarget - 1) / kRungsPerTarget;
}


// The good at the venue its market prices; none for one it does not.
const Good *GoodAt(const Rules &rules, const std::string &venueId, const std::string &itemId)
{
	const auto venue = rules.venues.find(venueId);
	if (venue == rules.venues.end())
		return nullptr;
	const auto good = std::ranges::find(venue->second.goods, itemId, &Good::itemId);
	return good == venue->second.goods.end() || good->native <= Decimal{} ? nullptr : &*good;
}


// The best bid and ask the venue's broker quotes the item at as its stock stands, its ladder's top
// rungs; none for a good its market does not price.
std::optional<std::pair<std::int64_t, std::int64_t>> Quotes(
    const entt::registry &registry, const Rules &rules, const std::string &venueId,
    const std::string &itemId)
{
	const Good *pGood = GoodAt(rules, venueId, itemId);
	if (!pGood)
		return std::nullopt;
	const std::uint64_t held =
	    BrokerStock(registry, rules.identities.Find(Kind::Broker, venueId), itemId);
	const std::uint64_t rung = Rung(*pGood);
	return std::pair{
	    CurvePrice(*pGood, 2 * held + rung, rules.spreadBps, Side::Buy),
	    CurvePrice(*pGood, 2 * held - std::min(rung, held), rules.spreadBps, Side::Sell)};
}


// A rung of a broker's ladder as it would rest: its side, totes and limit, and a bid's escrow.
struct Posting
{
	Side side{Side::Sell};
	std::uint64_t totes{0};
	std::int64_t limit{0};
	std::int64_t escrow{0};
};


// The good's asks at the stock it holds, after what the ladder has: up to kLevels, a rung each,
// priced at the curve at the rung's middle, so they climb as they sell it down.
void Asks(const Good &good, std::uint64_t held, int spreadBps, std::vector<Posting> &ladder)
{
	for (std::uint64_t at = held, level = 0; at > 0 && level < kLevels; ++level)
	{
		const std::uint64_t totes = std::min(Rung(good), at);
		ladder.push_back(
		    {Side::Sell, totes, CurvePrice(good, 2 * at - totes, spreadBps, Side::Sell), 0});
		at -= totes;
	}
}


// The good's bids at the stock it holds, after what the ladder has: up to kLevels so, falling as they
// buy it up, within the budget and its warehouse's room.
void Bids(
    const Rules &rules, const Good &good, std::uint64_t held, std::int64_t budget,
    std::vector<Posting> &ladder)
{
	const double unitsPerTote = good.unitsPerTote;
	for (std::uint64_t level = 0; level < kLevels && held < Core::kMaxWarehouseTotes; ++level)
	{
		const std::uint64_t totes = std::min(Rung(good), Core::kMaxWarehouseTotes - held);
		const std::int64_t limit = CurvePrice(good, 2 * held + totes, rules.spreadBps, Side::Buy);
		const std::optional<std::int64_t> perTote =
		    limit > 0 ? PerTote(limit, unitsPerTote, rules.feeBps) : std::nullopt;
		if (!perTote || *perTote <= 0)
			break;
		const std::uint64_t affordable = std::min(totes, static_cast<std::uint64_t>(budget / *perTote));
		if (affordable == 0)
			break;
		const std::int64_t escrow = *perTote * static_cast<std::int64_t>(affordable);
		ladder.push_back({Side::Buy, affordable, limit, escrow});
		budget -= escrow;
		held += totes;
	}
}


// Each broker's ladders (Asks, then Bids within its share of the funds, the goods in its market's
// order): a sale and its buy back meet at the same middles of the curve, so the round trip loses the
// spread. A good whose resting orders are already the ladder its stock and share give, none filled or
// lapsing before the next turn's requote, keeps them, as posting it again would leave them; the rest
// are retired and posted afresh, each at the share the goods before it leave.
void Requote(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Requote");
	entt::registry &registry = world.Registry();
	const std::uint64_t next = world.Clock().Step() + rules.turnSteps;
	using Resting = std::vector<std::pair<std::uint64_t, entt::entity>>;
	std::vector<Posting> ladder;
	for (const std::string &venueId : rules.venueOrder)
	{

		const entt::entity broker = rules.identities.Find(Kind::Broker, venueId);
		const entt::entity account = rules.identities.Find(Kind::BrokerAccount, venueId);
		JETTISON_VERIFY(broker != entt::null && account != entt::null);
		const Venue &venue = rules.venues.at(venueId);
		const std::vector<Good> &goods = venue.goods;
		// Its resting orders by good, from the venue's markets, and the escrow they hold.
		std::vector<Resting> resting(goods.size());
		std::int64_t funds = registry.get<const World::Account>(account).balance;
		for (std::size_t index = 0; index < goods.size(); ++index)
			for (const entt::entity entity : Listing(registry, venueId, goods[index].itemId))
				if (const auto &order = registry.get<const World::Order>(entity);
				    order.ownerKind == Kind::Broker)
				{
					resting[index].emplace_back(order.sequence, entity);
					funds += order.escrow;
				}
		// What it holds of each good, available and reserved, in one pass of its stock.
		std::vector<std::uint64_t> stocked(goods.size(), 0);
		for (const World::Holding &holding : registry.get<const World::Stock>(broker).holdings)
			if (const auto good = venue.goodIndex.find(holding.itemId); good != venue.goodIndex.end())
				stocked[good->second] = holding.available + holding.reserved;
		auto left = static_cast<std::int64_t>(
		    std::ranges::count_if(goods, [](const Good &good) { return good.native > Decimal{}; }));
		std::vector<std::pair<const Good *, std::int64_t>> due;
		for (std::size_t index = 0; index < goods.size(); ++index)
		{
			const Good &good = goods[index];
			if (good.native <= Decimal{})
				continue;
			const std::uint64_t held = stocked[index];
			const std::int64_t budget = funds / left--;
			ladder.clear();
			Asks(good, held, rules.spreadBps, ladder);
			Bids(rules, good, held, budget, ladder);
			for (const Posting &posting : ladder)
				funds -= posting.escrow;
			Resting &orders = resting[index];
			std::ranges::sort(orders);
			const bool kept = std::ranges::equal(
			    ladder, orders,
			    [&](const Posting &posting, const std::pair<std::uint64_t, entt::entity> &held)
			    {
				    const auto &order = registry.get<const World::Order>(held.second);
				    return order.side == posting.side && order.limit == posting.limit &&
				           order.totes == posting.totes && order.remaining == posting.totes &&
				           (order.expiresStep == 0 || order.expiresStep > next);
			    });
			if (kept)
				continue;
			RetireAll(registry, rules.identities, std::move(orders));
			due.emplace_back(&good, budget);
		}

		for (const auto &[pGood, budget] : due)
		{
			World::Order order;
			order.ownerKind = Kind::Broker;
			order.ownerId = venueId;
			order.venueId = venueId;
			order.itemId = pGood->itemId;
			// Its orders retired, all it holds is free; an ask may fill, so the bids are at what is
			// left.
			ladder.clear();
			Asks(*pGood, BrokerStock(registry, broker, pGood->itemId), rules.spreadBps, ladder);
			for (const Posting &posting : ladder)
			{
				order.side = posting.side;
				order.totes = posting.totes;
				order.limit = posting.limit;
				(void)Post(world, rules, order);
			}
			ladder.clear();
			Bids(rules, *pGood, BrokerStock(registry, broker, pGood->itemId), budget, ladder);
			for (const Posting &posting : ladder)
			{
				order.side = posting.side;
				order.totes = posting.totes;
				order.limit = posting.limit;
				if (Post(world, rules, order) != Result::Succeeded)
					break;
			}
		}
	}
}


// ─── The regional economy ───
// docs/economy-and-development-plan.md, "Aggregate households, wages and consumer demand" and what
// follows it; docs/stage-4-trading-plan.md, "The step, in order".

entt::entity Require(const Rules &rules, Kind kind, const std::string &id)
{
	const entt::entity entity = rules.identities.Find(kind, id);
	JETTISON_VERIFY(entity != entt::null);
	return entity;
}


// The totes of the item the owner has free at the venue.
std::uint64_t Available(
    const entt::registry &registry, entt::entity owner, const std::string &venueId,
    const std::string &itemId)
{
	for (const World::Holding &holding : registry.get<const World::Stock>(owner).holdings)
		if (holding.venueId == venueId && holding.itemId == itemId)
			return holding.available;
	return 0;
}


// A gross payment of the region's employer's wages: the households' share to their account and the
// withheld credits to the treasury, the carry moved on. The employer's account paid it as the batch
// was reserved.
void Pay(
    Ledger &ledger, const Rules &rules, const std::string &regionId, World::Production &production,
    std::int64_t gross)
{
	if (gross <= 0)
		return;
	const Withholding withheld = Withhold(gross, production.taxCarry, rules.perTaxCredit);
	production.taxCarry = withheld.carry;
	if (!ledger.Credit(Kind::HouseholdAccount, regionId, withheld.households) ||
	    !ledger.Credit(Kind::PublicAccount, regionId, withheld.treasury))
		throw std::overflow_error{std::format("'{}''s wages exceed its accounts", regionId)};
}


// A new batch of the row, not yet funded: none when it cannot be one. About batchSeconds of the
// row's rate, in whole totes, at least one.
std::optional<World::Batch> Plan(const Rules &rules, const Row &row)
{
	const double unitsPerTote = rules.unitsPerTote.at(row.itemId);
	const double perDayTotes = row.perDay / unitsPerTote;
	const double totes =
	    std::max(1.0, std::round(perDayTotes * rules.production.batchSeconds / 86'400.0));
	if (!std::isfinite(totes) || totes > static_cast<double>(Core::kMaxWarehouseTotes))
		return std::nullopt;
	World::Batch batch;
	batch.venueId = row.venueId;
	batch.itemId = row.itemId;
	batch.totes = static_cast<std::uint64_t>(totes);
	batch.duration = totes * 86'400.0 / perDayTotes;
	const std::optional<std::int64_t> wages =
	    Cost(rules.basePrices.at(row.itemId), rules.production.wageShare * totes * unitsPerTote, false);
	if (!wages)
		return std::nullopt;
	batch.wages = *wages;
	return batch;
}


// A new batch of the row, its wages reserved from the employer's account: none when it cannot fund
// them.
std::optional<World::Batch> Start(
    Ledger &ledger, const Rules &rules, const std::string &regionId, const Row &row)
{
	std::optional<World::Batch> batch = Plan(rules, row);
	if (!batch)
		return std::nullopt;
	World::Account &account = ledger.Account(Kind::EmployerAccount, regionId);
	if (account.balance < batch->wages)
		return std::nullopt;
	account.balance -= batch->wages;
	return batch;
}


// Each employer's batches worked for a step at its region's rate; a finished batch's totes go into the
// employer's holding at its venue and the rest of its wages are paid, and the next starts with the
// work left. A finished batch with no room in the warehouse waits, finished, for room.
void Produce(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Produce");
	entt::registry &registry = world.Registry();
	for (const Economy &economy : rules.economies)
	{
		if (economy.made.empty())
			continue;
		const World::Region &region =
		    registry.get<const World::Region>(Require(rules, Kind::Region, economy.regionId));
		const double rate = WorkRate(region.health, region.happiness, rules.condition.workRateFloor);
		const entt::entity employer = Require(rules, Kind::Employer, economy.regionId);
		World::Production production = registry.get<const World::Production>(employer);
		Ledger ledger{registry, rules.identities};
		for (const Row &row : economy.made)
		{
			const auto key = std::tie(row.venueId, row.itemId);
			const auto find = [&]
			{
				return std::ranges::find_if(
				    production.batches, [&](const World::Batch &batch)
				    { return std::tie(batch.venueId, batch.itemId) == key; });
			};
			double work = world.FixedStep() * rate;
			while (true)
			{
				auto batch = find();
				if (batch == production.batches.end())
				{
					std::optional<World::Batch> started = Start(ledger, rules, economy.regionId, row);
					if (!started)
						break;
					const auto at = std::ranges::lower_bound(
					    production.batches, key, std::less<>{},
					    [](const World::Batch &held) { return std::tie(held.venueId, held.itemId); });
					batch = production.batches.insert(at, *std::move(started));
				}
				const double left = batch->duration - batch->progress;
				if (work < left)
				{
					batch->progress += work;
					break;
				}
				batch->progress = batch->duration;
				work -= left;
				World::Holding &holding =
				    ledger.Holding(Kind::Employer, economy.regionId, row.venueId, row.itemId);
				if (holding.available + holding.reserved > Core::kMaxWarehouseTotes - batch->totes)
					break;
				holding.available += batch->totes;
				Pay(ledger, rules, economy.regionId, production, batch->wages - batch->paid);
				production.batches.erase(batch);
				if (work <= 0.0)
					break;
			}
		}
		ledger.Commit();
		registry.replace<World::Production>(employer, std::move(production));
	}
}


// The wages each batch has earned and not yet been paid, rounded down.
void Payroll(SimWorld &world, const Rules &rules)
{
	entt::registry &registry = world.Registry();
	for (const Economy &economy : rules.economies)
	{
		const entt::entity employer = Require(rules, Kind::Employer, economy.regionId);
		World::Production production = registry.get<const World::Production>(employer);
		if (production.batches.empty())
			continue;
		Ledger ledger{registry, rules.identities};
		for (World::Batch &batch : production.batches)
		{
			const std::int64_t earned = Earned(batch.wages, batch.progress, batch.duration);
			if (earned > batch.paid)
			{
				Pay(ledger, rules, economy.regionId, production, earned - batch.paid);
				batch.paid = earned;
			}
		}
		ledger.Commit();
		registry.replace<World::Production>(employer, std::move(production));
	}
}


// The owner's resting orders in the venue's market for the item, cancelled in placement order.
void CancelAt(
    SimWorld &world, const Rules &rules, Kind kind, const std::string &id, const std::string &venueId,
    const std::string &itemId)
{
	std::vector<std::pair<std::uint64_t, entt::entity>> resting;
	for (const entt::entity entity : Listing(world.Registry(), venueId, itemId))
		if (const auto &order = world.Registry().get<const World::Order>(entity);
		    order.ownerKind == kind && order.ownerId == id)
			resting.emplace_back(order.sequence, entity);
	RetireAll(world.Registry(), rules.identities, std::move(resting));
}


// The owner's resting orders, cancelled in placement order.
void CancelAll(SimWorld &world, const Rules &rules, Kind kind, const std::string &id)
{
	std::vector<std::pair<std::uint64_t, entt::entity>> resting;
	for (const auto [entity, order] : world.Registry().view<const World::Order>().each())
		if (order.ownerKind == kind && order.ownerId == id)
			resting.emplace_back(order.sequence, entity);
	RetireAll(world.Registry(), rules.identities, std::move(resting));
}


// Each employer sells everything it has made into its broker's bids, down to a floor, what a unit of
// it cost in wages (D-102): what does not sell at once is cancelled and kept in its warehouse, offered
// again next turn.
void Sell(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Sell");
	entt::registry &registry = world.Registry();
	for (const Economy &economy : rules.economies)
	{
		if (economy.made.empty())
			continue;
		CancelAll(world, rules, Kind::Employer, economy.regionId);
		const std::vector<World::Holding> holdings =
		    registry.get<const World::Stock>(Require(rules, Kind::Employer, economy.regionId)).holdings;
		for (const World::Holding &holding : holdings)
		{
			const double floor = std::ceil(
			    static_cast<double>(rules.basePrices.at(holding.itemId)) * rules.production.wageShare);
			if (holding.available == 0 || floor <= 0.0 || floor >= 0x1p63)
				continue;
			World::Order order;
			order.ownerKind = Kind::Employer;
			order.ownerId = economy.regionId;
			order.venueId = holding.venueId;
			order.itemId = holding.itemId;
			order.side = Side::Sell;
			order.limit = static_cast<std::int64_t>(floor);
			order.totes = holding.available;
			(void)Post(world, rules, std::move(order));
		}
		CancelAll(world, rules, Kind::Employer, economy.regionId);
	}
}


// The households buy the items, cheapest of the broker's asks first, at up to the markup over the ask
// each one's broker posts at its target stock (D-102), walking its asks: whole totes enough for the
// shortfall in the items' unit, within their funds and their warehouse's room. What does not fill at
// once is cancelled, so their bids never rest; an item whose best ask is over its limit is not bid for.
void Buy(
    SimWorld &world, const Rules &rules, const Economy &economy, std::span<const std::string> items,
    double shortfall, double markup)
{
	entt::registry &registry = world.Registry();
	const entt::entity household = Require(rules, Kind::Household, economy.regionId);
	const entt::entity account = Require(rules, Kind::HouseholdAccount, economy.regionId);
	std::vector<std::tuple<std::int64_t, std::int64_t, std::string>> asks;
	for (const std::string &itemId : items)
	{
		const auto quotes = Quotes(registry, rules, economy.venueId, itemId);
		if (!quotes)
			continue;
		const Good &good = *GoodAt(rules, economy.venueId, itemId);
		const std::int64_t reference =
		    CurvePrice(good, 2 * good.target - Rung(good), rules.spreadBps, Side::Sell);
		const double limit = std::floor(static_cast<double>(reference) * markup);
		const std::int64_t capped = limit < 0x1p63 ? static_cast<std::int64_t>(limit) : kMaxCredits;
		if (quotes->second <= capped)
			asks.emplace_back(quotes->second, capped, itemId);
	}
	std::ranges::stable_sort(asks, {}, [](const auto &ask) { return std::get<0>(ask); });
	for (const auto &[ask, limit, itemId] : asks)
	{
		if (shortfall <= 0.0)
			break;
		const double unitsPerTote = rules.unitsPerTote.at(itemId);
		const std::optional<std::int64_t> perTote = PerTote(limit, unitsPerTote, rules.feeBps);
		if (!perTote)
			continue;
		const std::int64_t balance = registry.get<const World::Account>(account).balance;
		const std::uint64_t held = Available(registry, household, economy.venueId, itemId);
		const double wanted = std::ceil(shortfall / unitsPerTote);
		const std::uint64_t affordable =
		    *perTote > 0 ? static_cast<std::uint64_t>(balance / *perTote) : Core::kMaxWarehouseTotes;
		const std::uint64_t totes = std::min(
		    {static_cast<std::uint64_t>(
		         std::min(wanted, static_cast<double>(Core::kMaxWarehouseTotes))),
		     affordable, Core::kMaxWarehouseTotes - held});
		if (totes == 0)
			continue;
		World::Order order;
		order.ownerKind = Kind::Household;
		order.ownerId = economy.regionId;
		order.venueId = economy.venueId;
		order.itemId = itemId;
		order.side = Side::Buy;
		order.limit = limit;
		order.totes = totes;
		(void)Post(world, rules, std::move(order));
		CancelAt(world, rules, Kind::Household, economy.regionId, economy.venueId, itemId);
		const std::uint64_t bought = Available(registry, household, economy.venueId, itemId) - held;
		shortfall -= static_cast<double>(bought) * unitsPerTote;
	}
}


// What the households consume of the quantity, in the items' unit, from what is opened and then whole
// totes opened in turn, in the items' order: all of it when they hold enough, exactly.
double Consume(
    Ledger &ledger, World::Pantry &pantry, const Rules &rules, const Economy &economy,
    std::span<const std::string> items, double quantity)
{
	double left = quantity;
	for (const std::string &itemId : items)
	{
		auto opened = std::ranges::find(pantry.opened, itemId, &World::Opened::itemId);
		World::Holding &holding =
		    ledger.Holding(Kind::Household, economy.regionId, economy.venueId, itemId);
		while (left > 0.0)
		{
			if (opened == pantry.opened.end() || opened->quantity <= 0.0)
			{
				if (holding.available == 0)
					break;
				--holding.available;
				if (opened == pantry.opened.end())
					opened = pantry.opened.insert(
					    std::ranges::lower_bound(pantry.opened, itemId, {}, &World::Opened::itemId),
					    {itemId, 0.0});
				opened->quantity += rules.unitsPerTote.at(itemId);
			}
			const double taken = std::min(left, opened->quantity);
			opened->quantity -= taken;
			left -= taken;
		}
	}
	std::erase_if(pantry.opened, [](const World::Opened &opened) { return opened.quantity <= 0.0; });
	return left <= 0.0 ? quantity : quantity - left;
}


// The span a need's pressure is its unmet share smoothed over (D-102): an hour, the user's, so a
// shortage raises what households pay within a few intervals.
constexpr double kPressureSeconds = 3'600.0;


// Each region's households' interval: every need's fresh requirement, scaled by the population and the
// interval, bought and consumed in the needs' order; then, every need met, the optional goods; then
// the region's health and happiness, from the interval's severity, which the work rate reads from the
// next step.
void Households(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Households");
	entt::registry &registry = world.Registry();
	for (const Economy &economy : rules.economies)
	{
		const entt::entity regionEntity = Require(rules, Kind::Region, economy.regionId);
		const double population = registry.get<const World::Region>(regionEntity).population;
		const entt::entity household = Require(rules, Kind::Household, economy.regionId);
		std::vector<NeedMet> met;
		World::Pantry pantry = registry.get<const World::Pantry>(household);
		if (population > 0.0 && !economy.venueId.empty())
		{
			const double days = rules.intervalSeconds / 86'400.0;
			const auto have = [&](std::span<const std::string> items)
			{
				double held = 0.0;
				for (const std::string &itemId : items)
				{
					const auto opened =
					    std::ranges::find(pantry.opened, itemId, &World::Opened::itemId);
					held +=
					    (opened == pantry.opened.end() ? 0.0 : opened->quantity) +
					    static_cast<double>(Available(registry, household, economy.venueId, itemId)) *
					        rules.unitsPerTote.at(itemId);
				}
				return held;
			};
			const auto consume = [&](std::span<const std::string> items, double required, double markup)
			{
				if (const double held = have(items); held < required)
					Buy(world, rules, economy, items, required - held, markup);
				Ledger ledger{registry, rules.identities};
				const double consumed = Consume(ledger, pantry, rules, economy, items, required);
				ledger.Commit();
				return consumed;
			};
			const double smoothing = std::min(1.0, rules.intervalSeconds / kPressureSeconds);
			for (const Need &need : rules.needs)
			{
				const double required = need.perThousandPerDay * population / 1'000.0 * days;
				auto unmet = std::ranges::find(pantry.unmet, need.needId, &World::Unmet::needId);
				const double pressure = unmet == pantry.unmet.end() ? 0.0 : unmet->pressure;
				// From the ask at the target when fed to the curve's ceiling when starving.
				const double consumed = consume(
				    need.items, required, 1.0 + static_cast<double>(kCurveCeiling - 1) * pressure);
				met.push_back({need.weight, required, consumed});
				if (consumed < required && unmet == pantry.unmet.end())
					unmet = pantry.unmet.insert(
					    std::ranges::lower_bound(pantry.unmet, need.needId, {}, &World::Unmet::needId),
					    {need.needId, 0.0, 0.0});
				if (unmet == pantry.unmet.end())
					continue;
				unmet->quantity += required - consumed;
				unmet->pressure = std::clamp(
				    pressure + smoothing * ((required - consumed) / required - pressure), 0.0, 1.0);
			}
			if (std::ranges::all_of(
			        met, [](const NeedMet &need) { return need.satisfied >= need.required; }))
				for (const Row &row : economy.wanted)
					(void)consume(std::span{&row.itemId, 1}, row.perDay * days, 1.0 + row.premium);
		}
		pantry.severity = Severity(met);
		if (pantry.severity > 0.0)
			++pantry.shortIntervals;
		registry.replace<World::Pantry>(household, std::move(pantry));

		World::Region region = registry.get<const World::Region>(regionEntity);
		const Condition condition = Updated(
		    {region.health, region.happiness}, Severity(met), rules.intervalSeconds / 3'600.0,
		    rules.condition);
		region.health = condition.health;
		region.happiness = condition.happiness;
		registry.replace<World::Region>(regionEntity, std::move(region));
	}
}


// The step's economy: production, then on the turn's last step the payroll, the employers' sales,
// the households' interval and the condition, in that order.
void Step(SimWorld &world, const Rules &rules)
{
	Produce(world, rules);
	if ((world.Clock().Step() + 1) % rules.turnSteps != 0)
		return;
	Payroll(world, rules);
	Sell(world, rules);
	Households(world, rules);
}


// The last step of the turn the step is in, when the payroll, sales and brokers run.
std::uint64_t TurnEnd(std::uint64_t step, std::uint64_t turnSteps)
{
	return step + (turnSteps - 1 - step % turnSteps);
}


// What each step of the economy works on, by its employer: each row's batch under way, and the work
// a step adds to it, which the turn's condition alone changes.
template <typename Visit> void EachUnderWay(const SimWorld &world, const Rules &rules, Visit visit)
{
	const entt::registry &registry = world.Registry();
	for (const Economy &economy : rules.economies)
	{
		if (economy.made.empty())
			continue;
		const World::Region &region =
		    registry.get<const World::Region>(Require(rules, Kind::Region, economy.regionId));
		const double work = world.FixedStep() *
		                    WorkRate(region.health, region.happiness, rules.condition.workRateFloor);
		visit(economy, Require(rules, Kind::Employer, economy.regionId), work);
	}
}


// A bound on the rounding of one addition of a step's work to a batch's progress, relative to the
// batch's duration and work: a half unit in the last place, doubled for safety.
constexpr double kAdditionError = 0x1p-52;


// The first step the economy has to run in (Jettison::Sim::Quiet): the turn's last, or sooner a batch
// finishing, a batch that can start, or a finished one with room for its totes. Until then a step
// only adds each batch's work to it, the same addition every step.
std::uint64_t EconomyNext(const SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.EconomyNext");
	const entt::registry &registry = world.Registry();
	const std::uint64_t now = world.Clock().Step();
	std::uint64_t next = TurnEnd(now, rules.turnSteps);
	bool due = false;
	EachUnderWay(
	    world, rules,
	    [&](const Economy &economy, entt::entity employer, double work)
	    {
		    const World::Production &production = registry.get<const World::Production>(employer);
		    for (const Row &row : economy.made)
		    {
			    if (due)
				    return;
			    const auto batch = std::ranges::find_if(
			        production.batches, [&](const World::Batch &held)
			        { return held.venueId == row.venueId && held.itemId == row.itemId; });
			    if (batch == production.batches.end())
			    {
				    const std::optional<World::Batch> planned = Plan(rules, row);
				    due =
				        planned && registry.get<const World::Account>(
				                               Require(rules, Kind::EmployerAccount, economy.regionId))
				                           .balance >= planned->wages;
				    continue;
			    }
			    if (batch->progress >= batch->duration)
			    {
				    // Finished and waiting for room, which only a turn's sales or a trade makes.
				    const auto &holdings = registry.get<const World::Stock>(employer).holdings;
				    const auto held = std::ranges::find_if(
				        holdings, [&](const World::Holding &holding)
				        { return holding.venueId == row.venueId && holding.itemId == row.itemId; });
				    const std::uint64_t stocked =
				        held == holdings.end() ? 0 : held->available + held->reserved;
				    due = stocked <= Core::kMaxWarehouseTotes - batch->totes;
				    continue;
			    }
			    // Short of its end by more than the steps to next can add, rounding included, it is not
			    // due.
			    const auto span = static_cast<double>(next - now) + 1.0;
			    if (batch->duration - batch->progress - work * span >
			        span * kAdditionError * (batch->duration + work))
				    continue;
			    double progress = batch->progress;
			    for (std::uint64_t step = now; step < next; ++step)
			    {
				    if (!(work < batch->duration - progress))
				    {
					    next = step;
					    break;
				    }
				    progress += work;
			    }
		    }
	    });
	return due ? now : next;
}


// The work the steps passed over added to each batch under way, one addition a step, as Produce
// adds it.
void EconomySkip(SimWorld &world, const Rules &rules, std::uint64_t passed)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.EconomySkip");
	entt::registry &registry = world.Registry();
	EachUnderWay(
	    world, rules,
	    [&](const Economy &economy, entt::entity employer, double work)
	    {
		    std::vector<World::Batch> &batches = registry.get<World::Production>(employer).batches;
		    for (const Row &row : economy.made)
			    if (const auto batch = std::ranges::find_if(
			            batches, [&](const World::Batch &held)
			            { return held.venueId == row.venueId && held.itemId == row.itemId; });
			        batch != batches.end() && batch->progress < batch->duration)
				    batch->progress = Added(batch->progress, work, passed);
	    });
}


// ─── Observation ───
// docs/market-information-plan.md, "Selected information rules".

const Content::VenueDefinition &DefinitionOf(const Rules &rules, std::string_view venueId)
{
	const auto definition = rules.definitions.find(venueId);
	JETTISON_VERIFY(definition != rules.definitions.end());
	return *definition->second;
}


// The report in the player's reports, in place of the one of its venue they had.
void Remember(World::Reports &reports, World::Report report)
{
	const auto at =
	    std::ranges::lower_bound(reports.reports, report.venueId, {}, &World::Report::venueId);
	if (at != reports.reports.end() && at->venueId == report.venueId)
		*at = std::move(report);
	else
		reports.reports.insert(at, std::move(report));
}


// Each docked fleet's venue reported to its principals as it is when the step observed begins.
void Observe(SimWorld &world, const Rules &rules, std::uint64_t observed)
{
	JETTISON_PROFILE_ZONE("Manifest.Trade.Observe");
	entt::registry &registry = world.Registry();
	// Each venue a fleet is docked at, and the principals whose fleets they are.
	std::map<std::string, std::set<std::string>, std::less<>> covered;
	for (const auto [entity, ship] : registry.view<const World::Ship>().each())
	{
		if (ship.dockedAt.empty())
			continue;
		const entt::entity company = rules.identities.Find(Kind::Company, ship.companyId);
		JETTISON_VERIFY(company != entt::null);
		covered[ship.dockedAt].insert(registry.get<const World::Company>(company).principalId);
	}
	// shortcut: each covered venue's report is made afresh every step, a scan of the book each; a
	// cheaper refresh (only what changed) when a profile shows it: at L26 all of Manifest's systems
	// together were under 5% of a cruising step.
	for (const auto &[venueId, principals] : covered)
	{
		const World::Report report = Capture(
		    registry, rules.identities, DefinitionOf(rules, venueId), observed, rules.windowSteps);
		for (const std::string &principalId : principals)
		{
			const entt::entity principal = rules.identities.Find(Kind::Principal, principalId);
			JETTISON_VERIFY(principal != entt::null);
			Remember(registry.get_or_emplace<World::Reports>(principal), report);
		}
	}
}
} // namespace


std::uint64_t SalesWindowSteps(const SimWorld &world, const Content::ContentSet &content)
{
	const double seconds = content.tuning.market.salesWindowSeconds;
	const std::optional<std::uint64_t> window = WholeSteps(seconds, world.FixedStep());
	if (!window || *window == 0)
		throw std::invalid_argument{std::format(
		    "market.salesWindowSeconds, {}, is not a whole number of {} s steps above 0", seconds,
		    world.FixedStep())};
	return *window;
}


std::vector<std::string> StartingVenues(
    const Content::ScenarioDefinition &scenario, std::string_view venueId)
{
	const auto starter =
	    std::ranges::find(scenario.venues, venueId, &Content::VenueDefinition::venueId);
	if (starter == scenario.venues.end())
		return {};
	const std::string &home = starter->systemId;
	const auto connects = [](const Content::SystemDefinition &system, std::string_view to)
	{
		return std::ranges::find(system.connections, to, &Content::SystemSelection::systemId) !=
		       system.connections.end();
	};
	std::set<std::string, std::less<>> systems{home};
	for (const Content::SystemDefinition &system : scenario.systems)
		if (system.systemId == home)
			for (const Content::SystemSelection &connection : system.connections)
				systems.insert(connection.systemId);
		else if (connects(system, home))
			systems.insert(system.systemId);

	std::vector<std::string> venues;
	for (const Content::VenueDefinition &venue : scenario.venues)
		if (venue.venueId != venueId && systems.contains(venue.systemId))
			venues.push_back(venue.venueId);
	return venues;
}


World::Report Capture(
    const entt::registry &registry, const World::Identities &identities,
    const Content::VenueDefinition &venue, std::uint64_t observed, std::uint64_t windowSteps)
{
	World::Report report{
	    venue.venueId,
	    World::Source::Docked,
	    observed,
	    observed > windowSteps ? observed - windowSteps : 0,
	    {}};
	report.lines.reserve(venue.market.size());
	for (const Content::MarketEntry &entry : venue.market)
		report.lines.emplace_back().itemId = entry.itemId;
	const auto lineOf = [&](std::string_view itemId) -> World::Line *
	{
		const auto line = std::ranges::find(report.lines, itemId, &World::Line::itemId);
		return line == report.lines.end() ? nullptr : &*line;
	};

	for (const auto [entity, order] : registry.view<const World::Order>().each())
	{
		World::Line *pLine = order.venueId == venue.venueId ? lineOf(order.itemId) : nullptr;
		// An order that no longer matches after the observed step is not in the book it shows.
		if (!pLine || (order.expiresStep != 0 && order.expiresStep <= observed))
			continue;
		std::vector<World::Level> &side = order.side == Side::Buy ? pLine->bids : pLine->asks;
		const auto level = std::ranges::find(side, order.limit, &World::Level::price);
		if (level == side.end())
			side.push_back({order.limit, order.remaining});
		else
			level->totes += order.remaining;
	}
	for (World::Line &line : report.lines)
	{
		std::ranges::sort(line.bids, std::greater{}, &World::Level::price);
		std::ranges::sort(line.asks, {}, &World::Level::price);
		line.bids.resize(std::min(line.bids.size(), World::kReportLevels));
		line.asks.resize(std::min(line.asks.size(), World::kReportLevels));
	}

	const entt::entity market = identities.Find(Kind::Market, venue.venueId);
	JETTISON_VERIFY(market != entt::null);
	const World::Sales &sales = registry.get<const World::Sales>(market);
	for (const World::Sold &sold : sales.recent)
		if (World::Line *pLine = sold.step >= report.since ? lineOf(sold.itemId) : nullptr)
			pLine->sold += sold.totes;
	for (const World::LastSale &last : sales.last)
		if (World::Line *pLine = lineOf(last.itemId))
		{
			pLine->lastStep = last.step;
			pLine->lastPrice = last.price;
		}
	return report;
}


void Seed(
    SimWorld &world, const World::Identities &identities, const Content::ContentSet &content,
    entt::entity principal, std::uint64_t windowSteps)
{
	const Content::ScenarioDefinition &scenario = content.scenarios.at(content.scenario);
	entt::registry &registry = world.Registry();
	World::Reports &reports = registry.get_or_emplace<World::Reports>(principal);
	for (const std::string &venueId : StartingVenues(scenario, scenario.starterVenue))
	{
		World::Report report = Capture(
		    registry, identities,
		    *std::ranges::find(scenario.venues, venueId, &Content::VenueDefinition::venueId),
		    world.Clock().Step(), windowSteps);
		report.source = World::Source::Starting;
		Remember(reports, std::move(report));
	}
}


void RegisterTypes()
{
	static const bool registered = []
	{
		Command::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<Place>("Manifest.Trade.Place")
		    .Field<&Place::venueId>("venueId", kSaved)
		    .Field<&Place::itemId>("itemId", kSaved)
		    .Field<&Place::side>("side", kSaved)
		    .Field<&Place::totes>("totes", kSaved)
		    .Field<&Place::limit>("limit", kSaved);
		RegisterType<Cancel>("Manifest.Trade.Cancel").Field<&Cancel::orderId>("orderId", kSaved);
		return true;
	}();
	(void)registered;
}


void Register(
    SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content)
{
	std::shared_ptr<const Rules> rules = MakeRules(world, identities, content);
	(void)MarketsOf(world.Registry());
	requests.Register<Place>(
	    std::string{kPlace}, kVersion, Command::ActorKind::Principal,
	    [rules, &requests](SimWorld &on, const Actor &actor, const Place &place)
	    { return Placed(on, *rules, requests, actor, place); });
	requests.Register<Cancel>(
	    std::string{kCancel}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Cancel &cancel)
	    { return Cancelled(on, *rules, actor, cancel); });

	using Jettison::Ecs::Access;
	const Access writes = Access::Of<World::Order>()
	                          .Writing<
	                              World::Order, World::Account, World::Stock, World::Identity,
	                              World::Sales, World::Paid>();
	// Ahead of the movement Travel::Register attaches (docs/stage-4-trading-plan.md, "The step, in
	// order").
	const std::vector<std::string> beforeMovement{
	    std::string{Jettison::Spatial::SystemIndices::kSystemName}};
	world.RegisterSystem(
	    {.name = std::string{kExpiry},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = writes,
	     .after = {},
	     .run = [rules, &world](entt::registry &, double) { Expire(world, *rules); },
	     .before = beforeMovement},
	    // An order lapses in the step its expiry begins.
	    {.next = [](const SimWorld &target)
	     {
		     std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
		     for (const auto [entity, order] : target.Registry().view<const World::Order>().each())
			     if (order.expiresStep != 0)
				     next = std::min(next, order.expiresStep);
		     return next;
	     }});
	world.RegisterSystem(
	    {.name = std::string{kEconomy},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access =
	         Access::Of<World::Order>()
	             .Writing<
	                 World::Order, World::Account, World::Stock, World::Identity, World::Production,
	                 World::Pantry, World::Region, World::Sales, World::Paid>(),
	     .after = {std::string{kExpiry}},
	     .run = [rules, &world](entt::registry &, double) { Step(world, *rules); },
	     .before = beforeMovement},
	    {.next = [rules](const SimWorld &target) { return EconomyNext(target, *rules); },
	     .skip = [rules](SimWorld &target, std::uint64_t passed, double)
	     { EconomySkip(target, *rules, passed); }});
	world.RegisterSystem(
	    {.name = std::string{kBrokers},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = writes,
	     .after = {std::string{kExpiry}, std::string{kEconomy}},
	     .run =
	         [rules, &world](entt::registry &, double)
	     {
		     if ((world.Clock().Step() + 1) % rules->turnSteps == 0)
			     Requote(world, *rules);
	     },
	     .before = beforeMovement},
	    {.next = [rules](const SimWorld &target)
	     { return TurnEnd(target.Clock().Step(), rules->turnSteps); }});
	world.RegisterSystem(
	    {.name = std::string{kObserve},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access =
	         Access::Of<World::Ship, World::Company, World::Order, World::Sales, World::Identity>()
	             .Writing<World::Reports>(),
	     .after = {std::string{Travel::kSystem}},
	     .run = [rules, &world](entt::registry &, double)
	     { Observe(world, *rules, world.Clock().Step() + 1); }},
	    // A report is read off its venue's book and sales and the fleets docked there, which only a
	    // step that runs changes, at the step observed, which moves its window's edge and an order's
	    // lapse: captured once at the last step passed over, it is what that step's own would be.
	    {.next = [](const SimWorld &) { return std::numeric_limits<std::uint64_t>::max(); },
	     .skip = [rules](SimWorld &target, std::uint64_t, double)
	     { Observe(target, *rules, target.Clock().Step()); }});
}


void Quote(SimWorld &world, World::Identities &identities, const Content::ContentSet &content)
{
	Requote(world, *MakeRules(world, identities, content));
}


namespace
{
std::optional<std::int64_t> PriceForTender(
    entt::registry &registry, const Rules &rules, const std::string &venueId, const std::string &itemId)
{
	const Good *pGood = GoodAt(rules, venueId, itemId);
	if (!pGood)
		return std::nullopt;
	std::optional<std::int64_t> lowest;
	for (const entt::entity entity : Listing(registry, venueId, itemId))
		if (const World::Order &order = registry.get<const World::Order>(entity);
		    order.side == Side::Sell && (!lowest || order.limit < *lowest))
			lowest = order.limit;
	return lowest ? lowest : pGood->native.Raw();
}
} // namespace


std::optional<std::int64_t> Budget(std::int64_t price, double units, double urgency)
{
	const std::optional<std::int64_t> cost = Cost(price, units, true);
	if (!cost || !(urgency >= 0.0))
		return std::nullopt;
	const double budget = std::ceil(static_cast<double>(*cost) * (1.0 + urgency));
	if (!(budget < 0x1p63))
		return std::nullopt;
	return static_cast<std::int64_t>(budget);
}


// shortcut: each call works out the rules from the content again, as Quote does; W12's issuer keeps
// them when a profile shows its calls.
std::optional<std::int64_t> TenderPrice(
    SimWorld &world, World::Identities &identities, const Content::ContentSet &content,
    std::string_view venueId, std::string_view itemId)
{
	return PriceForTender(
	    world.Registry(), *MakeRules(world, identities, content), std::string{venueId},
	    std::string{itemId});
}


std::vector<Relief> Reliefs(
    SimWorld &world, World::Identities &identities, const Content::ContentSet &content)
{
	const std::shared_ptr<const Rules> rules = MakeRules(world, identities, content);
	entt::registry &registry = world.Registry();
	std::vector<Relief> reliefs;
	for (const Economy &economy : rules->economies)
	{
		if (economy.venueId.empty())
			continue;
		const double population =
		    registry.get<const World::Region>(Require(*rules, Kind::Region, economy.regionId))
		        .population;
		const World::Pantry &pantry =
		    registry.get<const World::Pantry>(Require(*rules, Kind::Household, economy.regionId));
		for (const Need &need : rules->needs)
		{
			const auto unmet = std::ranges::find(pantry.unmet, need.needId, &World::Unmet::needId);
			if (unmet == pantry.unmet.end() || !(unmet->pressure > kReliefPressure))
				continue;
			std::optional<std::int64_t> cheapest;
			const std::string *pItem = nullptr;
			for (const std::string &itemId : need.items)
				if (const std::optional<std::int64_t> price =
				        PriceForTender(registry, *rules, economy.venueId, itemId);
				    price && (!cheapest || *price < *cheapest))
				{
					cheapest = price;
					pItem = &itemId;
				}
			if (!cheapest)
				continue;
			const double unitsPerTote = rules->unitsPerTote.at(*pItem);
			const double shortfall = need.perThousandPerDay * population / 1'000.0 * unmet->pressure;
			const double totes = std::ceil(shortfall / unitsPerTote);
			const std::optional<std::int64_t> budget =
			    totes < 0x1p63
			        ? Budget(*cheapest, totes * unitsPerTote, kReliefUrgency * unmet->pressure)
			        : std::nullopt;
			if (!budget)
				continue;
			reliefs.push_back(
			    {economy.regionId, need.needId, economy.venueId, *pItem,
			     static_cast<std::uint64_t>(totes), *budget});
		}
	}
	return reliefs;
}
} // namespace Manifest::Trade
