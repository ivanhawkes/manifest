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
#include <jettison/spatial/SystemIndices.h>

#include <manifest/sim/Simulation.h>
#include <manifest/trade/Economy.h>
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


// A market row a region's employer makes, or its households want, in the item's unit per day.
struct Row
{
	std::string venueId;
	std::string itemId;
	double perDay{0.0};
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
					economy.wanted.push_back({home->venueId, entry.itemId, entry.demand});
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
		World::Account &account = ledger.Account(World::AccountOf(order.ownerKind), order.ownerId);
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


// A broker's bid and ask about a native price: half the spread either side, the ask rounded up and the
// bid down.
std::pair<std::int64_t, std::int64_t> Around(Decimal native, int spreadBps)
{
	const __int128 mid = native.Raw();
	return {
	    static_cast<std::int64_t>(mid * (20'000 - spreadBps) / 20'000),
	    static_cast<std::int64_t>((mid * (20'000 + spreadBps) + 19'999) / 20'000)};
}


// The bid and ask a venue's broker quotes the item at; none for a good its market does not price.
std::optional<std::pair<std::int64_t, std::int64_t>> Quotes(
    const Rules &rules, const std::string &venueId, const std::string &itemId)
{
	const auto venue = rules.venues.find(venueId);
	if (venue == rules.venues.end())
		return std::nullopt;
	const auto quote =
	    std::ranges::find(venue->second.quotes, itemId, &std::pair<std::string, Decimal>::first);
	if (quote == venue->second.quotes.end() || quote->second <= Decimal{})
		return std::nullopt;
	return Around(quote->second, rules.spreadBps);
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
			const auto [bid, ask] = Around(native, rules.spreadBps);
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


// A new batch of the row, its wages reserved from the employer's account: none when it cannot fund
// them. About batchSeconds of the row's rate, in whole totes, at least one.
std::optional<World::Batch> Start(
    Ledger &ledger, const Rules &rules, const std::string &regionId, const Row &row)
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
	World::Account &account = ledger.Account(Kind::EmployerAccount, regionId);
	if (account.balance < batch.wages)
		return std::nullopt;
	account.balance -= batch.wages;
	return batch;
}


// Each employer's batches worked for a step at its region's rate; a finished batch's totes go into the
// employer's holding at its venue and the rest of its wages are paid, and the next starts with the
// work left. A finished batch with no room in the warehouse waits, finished, for room.
void Produce(SimWorld &world, const Rules &rules)
{
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


// The owner's resting orders, cancelled in placement order.
void CancelAll(SimWorld &world, const Rules &rules, Kind kind, const std::string &id)
{
	std::vector<std::pair<std::uint64_t, entt::entity>> resting;
	for (const auto [entity, order] : world.Registry().view<const World::Order>().each())
		if (order.ownerKind == kind && order.ownerId == id)
			resting.emplace_back(order.sequence, entity);
	RetireAll(world.Registry(), rules.identities, std::move(resting));
}


// Each employer offers everything it has made at its broker's bid, its last turn's offers cancelled.
void Sell(SimWorld &world, const Rules &rules)
{
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
			const auto quotes = Quotes(rules, holding.venueId, holding.itemId);
			if (holding.available == 0 || !quotes || quotes->first <= 0)
				continue;
			World::Order order;
			order.ownerKind = Kind::Employer;
			order.ownerId = economy.regionId;
			order.venueId = holding.venueId;
			order.itemId = holding.itemId;
			order.side = Side::Sell;
			order.limit = quotes->first;
			order.totes = holding.available;
			(void)Post(world, rules, std::move(order));
		}
	}
}


// The households buy at the broker's ask, cheapest of the items first, whole totes enough for the
// shortfall in the items' unit, within their funds and their warehouse's room; what does not fill at
// once is cancelled, so their bids never rest.
void Buy(
    SimWorld &world, const Rules &rules, const Economy &economy, std::span<const std::string> items,
    double shortfall)
{
	entt::registry &registry = world.Registry();
	const entt::entity household = Require(rules, Kind::Household, economy.regionId);
	const entt::entity account = Require(rules, Kind::HouseholdAccount, economy.regionId);
	std::vector<std::pair<std::int64_t, std::string>> asks;
	for (const std::string &itemId : items)
		if (const auto quotes = Quotes(rules, economy.venueId, itemId))
			asks.emplace_back(quotes->second, itemId);
	std::ranges::stable_sort(asks, {}, &std::pair<std::int64_t, std::string>::first);
	for (const auto &[ask, itemId] : asks)
	{
		if (shortfall <= 0.0)
			break;
		const double unitsPerTote = rules.unitsPerTote.at(itemId);
		const std::optional<std::int64_t> perTote = PerTote(ask, unitsPerTote, rules.feeBps);
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
		order.limit = ask;
		order.totes = totes;
		(void)Post(world, rules, std::move(order));
		CancelAll(world, rules, Kind::Household, economy.regionId);
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


// Each region's households' interval: every need's fresh requirement, scaled by the population and the
// interval, bought and consumed in the needs' order; then, every need met, the optional goods; then
// the region's health and happiness, from the interval's severity, which the work rate reads from the
// next step.
void Households(SimWorld &world, const Rules &rules)
{
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
			const auto consume = [&](std::span<const std::string> items, double required)
			{
				if (const double held = have(items); held < required)
					Buy(world, rules, economy, items, required - held);
				Ledger ledger{registry, rules.identities};
				const double consumed = Consume(ledger, pantry, rules, economy, items, required);
				ledger.Commit();
				return consumed;
			};
			for (const Need &need : rules.needs)
			{
				const double required = need.perThousandPerDay * population / 1'000.0 * days;
				const double consumed = consume(need.items, required);
				met.push_back({need.weight, required, consumed});
				if (consumed < required)
				{
					auto unmet = std::ranges::find(pantry.unmet, need.needId, &World::Unmet::needId);
					if (unmet == pantry.unmet.end())
						unmet = pantry.unmet.insert(
						    std::ranges::lower_bound(
						        pantry.unmet, need.needId, {}, &World::Unmet::needId),
						    {need.needId, 0.0});
					unmet->quantity += required - consumed;
				}
			}
			if (std::ranges::all_of(
			        met, [](const NeedMet &need) { return need.satisfied >= need.required; }))
				for (const Row &row : economy.wanted)
					(void)consume(std::span{&row.itemId, 1}, row.perDay * days);
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
	     .before = beforeMovement});
	world.RegisterSystem(
	    {.name = std::string{kEconomy},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = Access::Of<World::Order>()
	                   .Writing<
	                       World::Order, World::Account, World::Stock, World::Identity,
	                       World::Production, World::Pantry, World::Region>(),
	     .after = {std::string{kExpiry}},
	     .run = [rules, &world](entt::registry &, double) { Step(world, *rules); },
	     .before = beforeMovement});
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
	     .before = beforeMovement});
}


void Quote(SimWorld &world, World::Identities &identities, const Content::ContentSet &content)
{
	Requote(world, *MakeRules(world, identities, content));
}
} // namespace Manifest::Trade
