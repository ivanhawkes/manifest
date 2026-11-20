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
#include <stdexcept>
#include <utility>
#include <vector>

#include <jettison/core/Assert.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/reflect/Reflect.h>

#include <manifest/sim/Simulation.h>
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


struct Venue
{
	// The venue's market's goods in its order, each at its native price.
	std::vector<std::pair<std::string, Decimal>> quotes;
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
	int feeBps{0};
	int spreadBps{0};
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
	}

	// The legacy model's prices (Simulation::GetLocalPrice), over the scenario's venues as its
	// economies: the whole market's scarcity, then each venue's own making and needing.
	std::map<std::string, double, std::less<>> supply;
	std::map<std::string, double, std::less<>> demand;
	for (const Content::VenueDefinition &venue : scenario.venues)
		for (const Content::MarketEntry &entry : venue.market)
		{
			supply[entry.itemId] += entry.production;
			demand[entry.itemId] += entry.demand;
		}
	const auto economies = static_cast<double>(std::max<std::size_t>(scenario.venues.size(), 1));
	for (const Content::VenueDefinition &venue : scenario.venues)
	{
		Venue &hosted = rules->venues[venue.venueId];
		rules->venueOrder.push_back(venue.venueId);
		for (const Content::MarketEntry &entry : venue.market)
		{
			const double made = supply[entry.itemId];
			const double needed = demand[entry.itemId];
			const Decimal market = Core::Simulation::MarketPrice(
			    Core::Simulation::BasePrice(*items.at(entry.itemId)), made, needed);
			hosted.quotes.emplace_back(
			    entry.itemId, Core::Simulation::LocalPrice(
			                      market, entry.production, entry.demand, (made + needed) / economies));
		}
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
	rules->feeBps = content.tuning.market.listingFeeBps;
	rules->spreadBps = content.tuning.brokers.spreadBps;
	return rules;
}


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


Kind AccountKind(Kind owner)
{
	return owner == Kind::Broker ? Kind::BrokerAccount : Kind::CompanyAccount;
}


// Copies of what one unit of work changes, written back together by Commit: everything a command or
// a system decides is planned here, so an expected refusal leaves the world as it was. An account
// or an owner reached twice, by a self-trade or a broker's own venue, is the one copy.
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

	// The owner's holding of the item at the venue, made empty if it has none: used at once, since
	// the next call may move it.
	World::Holding &Holding(
	    Kind owner, const std::string &ownerId, const std::string &venueId, const std::string &itemId)
	{
		const entt::entity entity = Require(owner, ownerId);
		std::vector<World::Holding> &holdings =
		    m_stocks.try_emplace(entity, m_registry.get<const World::Stock>(entity))
		        .first->second.holdings;
		const auto key = std::tie(venueId, itemId);
		const auto at = std::ranges::lower_bound(
		    holdings, key, std::less<>{},
		    [](const World::Holding &held) { return std::tie(held.venueId, held.itemId); });
		if (at != holdings.end() && std::tie(at->venueId, at->itemId) == key)
			return *at;
		return *holdings.insert(at, {venueId, itemId, 0, 0});
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
		for (auto &[entity, stock] : m_stocks)
		{
			std::erase_if(
			    stock.holdings, [](const World::Holding &holding)
			    { return holding.available == 0 && holding.reserved == 0; });
			m_registry.replace<World::Stock>(entity, std::move(stock));
		}
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
	std::map<entt::entity, World::Stock> m_stocks;
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
	else if (!ledger.Credit(AccountKind(order.ownerKind), order.ownerId, order.escrow))
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


// The order reserved, then matched against the book, the whole planned in one ledger and written only
// when every check holds; what is left rests under a new ID.
Result Post(SimWorld &world, const Rules &rules, World::Order order)
{
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
		World::Account &account = ledger.Account(AccountKind(order.ownerKind), order.ownerId);
		if (account.balance < reservation)
			return Result::InsufficientFunds;
		account.balance -= reservation;
		order.escrow = reservation;

		// Room for everything it and the owner's resting buys there may still fill.
		const World::Holding &holding =
		    ledger.Holding(order.ownerKind, order.ownerId, order.venueId, order.itemId);
		std::uint64_t held = holding.available + holding.reserved;
		for (const auto [entity, resting] : registry.view<const World::Order>().each())
			if (resting.side == Side::Buy && resting.ownerKind == order.ownerKind &&
			    resting.ownerId == order.ownerId && resting.venueId == order.venueId &&
			    resting.itemId == order.itemId)
				held += resting.remaining;
		if (held > Core::kMaxWarehouseTotes || order.totes > Core::kMaxWarehouseTotes - held)
			return Result::CapacityExceeded;
	}

	// The makers it crosses, best price first, then in time priority; an expired one no longer trades.
	std::vector<std::pair<entt::entity, const World::Order *>> makers;
	std::size_t restingHere = 0;
	for (const auto [entity, resting] : registry.view<const World::Order>().each())
	{
		if (resting.venueId != order.venueId || resting.itemId != order.itemId)
			continue;
		if (resting.side == order.side)
			++restingHere;
		else if ((resting.expiresStep == 0 || step < resting.expiresStep) && Crosses(order, resting))
			makers.emplace_back(entity, &resting);
	}
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
		if (!ledger.Credit(AccountKind(sell.ownerKind), sell.ownerId, *gross - *sellerFee) ||
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
	}

	if (order.remaining == 0)
	{
		if (!ledger.Credit(AccountKind(order.ownerKind), order.ownerId, order.escrow))
			return Result::InvalidQuantity;
		order.escrow = 0;
	}
	else if (restingHere >= kMaxRestingOrders)
		return Result::BookFull;

	ledger.Commit();
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
	const std::uint64_t step = world.Clock().Step();
	std::vector<std::pair<std::uint64_t, entt::entity>> expired;
	for (const auto [entity, order] : world.Registry().view<const World::Order>().each())
		if (order.expiresStep != 0 && step >= order.expiresStep)
			expired.emplace_back(order.sequence, entity);
	RetireAll(world.Registry(), rules.identities, std::move(expired));
}


void Requote(SimWorld &world, const Rules &rules)
{
	entt::registry &registry = world.Registry();
	for (const std::string &venueId : rules.venueOrder)
	{
		std::vector<std::pair<std::uint64_t, entt::entity>> resting;
		for (const auto [entity, order] : registry.view<const World::Order>().each())
			if (order.ownerKind == Kind::Broker && order.ownerId == venueId)
				resting.emplace_back(order.sequence, entity);
		RetireAll(registry, rules.identities, std::move(resting));

		const entt::entity broker = rules.identities.Find(Kind::Broker, venueId);
		const entt::entity account = rules.identities.Find(Kind::BrokerAccount, venueId);
		JETTISON_VERIFY(broker != entt::null && account != entt::null);
		const std::vector<std::pair<std::string, Decimal>> &quotes = rules.venues.at(venueId).quotes;
		auto left = static_cast<std::int64_t>(
		    std::ranges::count_if(quotes, [](const auto &quote) { return quote.second > Decimal{}; }));
		for (const auto &[itemId, native] : quotes)
		{
			if (native <= Decimal{})
				continue;
			const __int128 mid = native.Raw();
			const auto ask =
			    static_cast<std::int64_t>((mid * (20'000 + rules.spreadBps) + 19'999) / 20'000);
			const auto bid = static_cast<std::int64_t>(mid * (20'000 - rules.spreadBps) / 20'000);
			World::Order order;
			order.ownerKind = Kind::Broker;
			order.ownerId = venueId;
			order.venueId = venueId;
			order.itemId = itemId;

			const std::vector<World::Holding> &holdings =
			    registry.get<const World::Stock>(broker).holdings;
			const auto held = std::ranges::find_if(
			    holdings, [&](const World::Holding &holding) { return holding.itemId == itemId; });
			if (held != holdings.end() && held->available > 0)
			{
				order.side = Side::Sell;
				order.limit = ask;
				order.totes = held->available;
				(void)Post(world, rules, order);
			}

			// Its share of the funds, as far as its warehouse has room; the ask may have filled.
			const std::int64_t budget = registry.get<const World::Account>(account).balance / left--;
			const std::optional<std::int64_t> perTote =
			    bid > 0 ? PerTote(bid, rules.unitsPerTote.at(itemId), rules.feeBps) : std::nullopt;
			std::uint64_t room = Core::kMaxWarehouseTotes;
			for (const World::Holding &holding : registry.get<const World::Stock>(broker).holdings)
				if (holding.itemId == itemId)
					room -= holding.available + holding.reserved;
			if (perTote && *perTote > 0 && budget / *perTote > 0 && room > 0)
			{
				order.side = Side::Buy;
				order.limit = bid;
				order.totes = std::min(static_cast<std::uint64_t>(budget / *perTote), room);
				(void)Post(world, rules, order);
			}
		}
	}
}
} // namespace


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
	                          .Writing<World::Order, World::Account, World::Stock, World::Identity>();
	world.RegisterSystem(
	    {.name = std::string{kExpiry},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = writes,
	     .after = {},
	     .run = [rules, &world](entt::registry &, double) { Expire(world, *rules); }});
	world.RegisterSystem(
	    {.name = std::string{kBrokers},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = writes,
	     // W04 orders it after manifest.economy, once that system is registered.
	     .after = {std::string{kExpiry}},
	     .run = [rules, &world](entt::registry &, double)
	     {
		     if ((world.Clock().Step() + 1) % rules->turnSteps == 0)
			     Requote(world, *rules);
	     }});
}


void Quote(SimWorld &world, World::Identities &identities, const Content::ContentSet &content)
{
	Requote(world, *MakeRules(world, identities, content));
}
} // namespace Manifest::Trade
