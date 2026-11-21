#include <manifest/policy/Policy.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <jettison/core/Assert.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/spatial/SystemIndices.h>

#include <manifest/company/Company.h>
#include <manifest/config/SimulationConfig.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/State.h>

namespace Manifest::Policy
{
namespace
{
using Command::Actor;
using Command::Result;
using Jettison::Sim::SimWorld;
using World::Kind;
using World::Phase;

constexpr std::int64_t kMaxCredits = std::numeric_limits<std::int64_t>::max();
// A job's decisions in one step: choosing leads at once to buying where its ship is docked, or to
// selling, and a blocked job's retry to choosing.
constexpr int kMaxDecisions = 4;


// What the system runs on, worked out once from the content.
struct Rules
{
	Command::Requests &requests;
	World::Identities &identities;
	// The scenario's venues and items, in its order, each item the count of its units a tote holds.
	std::vector<std::string> venues;
	std::vector<std::pair<std::string, double>> items;
	int feeBps{0};
	std::uint64_t turnSteps{0};
};


Result Sets(SimWorld &world, const World::Identities &identities, const Actor &actor, const Set &set)
{
	entt::registry &registry = world.Registry();
	const entt::entity ship = identities.Find(Kind::Ship, set.shipId);
	if (ship == entt::null)
		return Result::UnknownTarget;
	const entt::entity company =
	    identities.Find(Kind::Company, registry.get<const World::Ship>(ship).companyId);
	JETTISON_VERIFY(company != entt::null);
	if (registry.get<const World::Company>(company).principalId != actor.id)
		return Result::NotAuthorised;
	if (set.targetProfit < 0 || set.lossAllowance < 0)
		return Result::InvalidQuantity;
	registry.emplace_or_replace<World::Policy>(ship, set.enabled, set.targetProfit, set.lossAllowance);
	if (!registry.all_of<World::Job>(ship))
		registry.emplace<World::Job>(ship);
	return Result::Succeeded;
}


entt::entity Required(const World::Identities &identities, Kind kind, const std::string &id)
{
	const entt::entity entity = identities.Find(kind, id);
	JETTISON_VERIFY(entity != entt::null);
	return entity;
}


std::int64_t Added(std::int64_t left, std::int64_t right)
{
	return left + std::min(right, kMaxCredits - left);
}


// The part of a whole for n of all t, rounded up; the whole for all of them.
std::int64_t Share(std::int64_t whole, std::uint64_t n, std::uint64_t t)
{
	if (n >= t || whole <= 0)
		return n >= t ? whole : 0;
	return static_cast<std::int64_t>(
	    (static_cast<__int128>(whole) * n + static_cast<__int128>(t) - 1) / t);
}


// a * n / t rounded up, for an a of either sign.
std::int64_t CeilDiv(std::int64_t a, std::uint64_t n, std::uint64_t t)
{
	const __int128 product = static_cast<__int128>(a) * n;
	const __int128 divisor = t;
	// Division truncates toward 0, which rounds a negative quotient up already.
	return static_cast<std::int64_t>(
	    product / divisor + (product > 0 && product % divisor != 0 ? 1 : 0));
}


// What a fill of the units at the price nets a seller, or costs a buyer, by one fill's rules.
std::optional<std::int64_t> Proceeds(std::int64_t price, double units, int feeBps)
{
	const std::optional<std::int64_t> gross = Trade::Cost(price, units, false);
	const std::optional<std::int64_t> fee = gross ? Trade::Fee(*gross, feeBps) : std::nullopt;
	return fee ? std::optional{*gross - *fee} : std::nullopt;
}


std::optional<std::int64_t> Outlay(std::int64_t price, double units, int feeBps)
{
	const std::optional<std::int64_t> gross = Trade::Cost(price, units, false);
	const std::optional<std::int64_t> fee = gross ? Trade::Fee(*gross, feeBps) : std::nullopt;
	if (!fee || *gross > kMaxCredits - *fee)
		return std::nullopt;
	return *gross + *fee;
}


const World::Line *LineOf(
    const World::Reports &reports, std::string_view venueId, std::string_view itemId)
{
	const auto report = std::ranges::find(reports.reports, venueId, &World::Report::venueId);
	if (report == reports.reports.end())
		return nullptr;
	const auto line = std::ranges::find(report->lines, itemId, &World::Line::itemId);
	return line == report->lines.end() ? nullptr : &*line;
}


// One ship's job, with what its decisions read and send.
struct Context
{
	SimWorld &world;
	const Rules &rules;
	entt::entity ship;
	std::string shipId;
	Actor owner;
	// Its blockage before this step's retry, so that blocking again for the same reason keeps the step
	// it first blocked at.
	World::Blockage was{World::Blockage::None};
	std::uint64_t wasSince{0};

	entt::registry &Registry() const
	{
		return world.Registry();
	}

	World::Job &Job() const
	{
		return Registry().get<World::Job>(ship);
	}

	const World::Policy &Policy() const
	{
		return Registry().get<const World::Policy>(ship);
	}

	const World::Ship &Record() const
	{
		return Registry().get<const World::Ship>(ship);
	}

	const World::Reports &Reports() const
	{
		return Registry().get<const World::Reports>(
		    Required(rules.identities, Kind::Principal, owner.id));
	}

	std::int64_t Balance() const
	{
		return Registry()
		    .get<const World::Account>(
		        Required(rules.identities, Kind::CompanyAccount, Record().companyId))
		    .balance;
	}

	// The company's totes of the item available at the venue.
	std::uint64_t Available(const std::string &venueId, const std::string &itemId) const
	{
		const World::Stock &stock = Registry().get<const World::Stock>(
		    Required(rules.identities, Kind::Company, Record().companyId));
		const auto holding = std::ranges::find_if(
		    stock.holdings, [&](const World::Holding &held)
		    { return held.venueId == venueId && held.itemId == itemId; });
		return holding == stock.holdings.end() ? 0 : holding->available;
	}

	std::uint64_t FreeHold() const
	{
		const World::Ship &record = Record();
		std::uint64_t free =
		    rules.identities.Hold(record, Registry().try_get<const World::Systems>(ship)).value_or(0);
		const World::Cargo &cargo = Registry().get<const World::Cargo>(ship);
		for (const World::Lot &lot : cargo.lots)
			free -= std::min(free, lot.totes);
		return free - std::min<std::uint64_t>(free, cargo.opened.size());
	}

	double UnitsPerTote(const std::string &itemId) const
	{
		const auto item =
		    std::ranges::find(rules.items, itemId, &std::pair<std::string, double>::first);
		JETTISON_VERIFY(item != rules.items.end());
		return item->second;
	}

	template <typename T>
	Result Act(std::string_view command, std::uint32_t version, const T &payload) const
	{
		return rules.requests.Act(world, owner, command, version, payload);
	}

	// Cancels what is left of the orders the job placed at the venue, the world's own (request 0).
	void CancelRest(const std::string &venueId, const std::string &itemId) const
	{
		std::vector<std::string> rest;
		for (const auto [entity, identity, order] :
		     Registry().view<const World::Identity, const World::Order>().each())
			if (order.ownerKind == Kind::Company && order.ownerId == Record().companyId &&
			    order.venueId == venueId && order.itemId == itemId && order.request == 0)
				rest.push_back(identity.id);
		for (const std::string &orderId : rest)
			JETTISON_VERIFY(
			    Act(Trade::kCancel, Trade::kVersion, Trade::Cancel{orderId}) == Result::Succeeded);
	}

	void Block(World::Blockage blockage) const
	{
		World::Job &job = Job();
		const std::uint64_t now = world.Clock().Step();
		job.phase = Phase::Blocked;
		job.blockedSince = blockage == was ? wasSince : now;
		job.blockage = blockage;
		// The next economic turn's last step, after the brokers' requote; never this one.
		const std::uint64_t after = now + 1;
		job.retry = after + (rules.turnSteps - 1 - after % rules.turnSteps);
	}

	// Starts the ship's journey to the venue, its docking fee a charge of the lot aboard; blocked when
	// the journey is refused.
	void Go(const std::string &venueId, Phase phase) const
	{
		const std::int64_t fee =
		    Registry()
		        .get<const World::Fees>(Required(rules.identities, Kind::Market, venueId))
		        .docking;
		if (Act(Travel::kTravel, Travel::kVersion, Travel::Travel{shipId, venueId}) !=
		    Result::Succeeded)
			return Block(World::Blockage::Refused);
		World::Job &job = Job();
		if (job.totes > 0)
			job.charges = Added(job.charges, fee);
		job.phase = phase;
	}
};


// A trade a job may make: its item, from the source to the destination, the totes, and what it is
// estimated to make after charges.
struct Deal
{
	std::string itemId;
	std::string sourceId;
	std::string destinationId;
	std::uint64_t totes{0};
	std::int64_t estimate{0};
};


// The trade between an ask and a bid: the least of the ask's and the bid's totes, the hold's free
// totes and what the account covers after the charges at the ask's escrow, estimated as one fill at
// each price. None for no totes.
std::optional<Deal> Estimate(
    const Context &context, const std::string &itemId, const World::Line &ask, const World::Line &bid,
    std::int64_t charges, std::int64_t balance, std::uint64_t free)
{
	const double units = context.UnitsPerTote(itemId);
	const int feeBps = context.rules.feeBps;
	const std::optional<std::int64_t> perTote = Trade::PerTote(ask.ask, units, feeBps);
	if (!perTote || *perTote <= 0 || balance <= charges)
		return std::nullopt;
	const std::uint64_t totes = std::min(
	    {ask.askTotes, bid.bidTotes, free, static_cast<std::uint64_t>((balance - charges) / *perTote)});
	if (totes == 0)
		return std::nullopt;
	const double traded = units * static_cast<double>(totes);
	const std::optional<std::int64_t> proceeds = Proceeds(bid.bid, traded, feeBps);
	const std::optional<std::int64_t> outlay = Outlay(ask.ask, traded, feeBps);
	if (!proceeds || !outlay)
		return std::nullopt;
	return Deal{itemId, {}, {}, totes, *proceeds - *outlay - charges};
}


// What a sale must recover of the lot: its cost, its charges and the target, less the allowance it has
// left, and the whole without the allowance.
std::pair<std::int64_t, std::int64_t> Floor(const World::Job &job, const World::Policy &policy)
{
	const std::int64_t whole = Added(Added(job.cost, job.charges), policy.targetProfit);
	const std::int64_t left = std::max<std::int64_t>(0, policy.lossAllowance - job.allowanceSpent);
	return {whole - left, whole};
}


// The best trade from the owner's reports above the target: the highest estimate, then a source the
// ship is docked at, then the scenario's order of source, destination and item.
std::optional<Deal> ChooseTrade(const Context &context)
{
	const World::Reports &reports = context.Reports();
	const World::Ship &record = context.Record();
	const std::int64_t balance = context.Balance();
	const std::uint64_t free = context.FreeHold();
	std::optional<Deal> best;
	for (const std::string &sourceId : context.rules.venues)
		for (const auto &[itemId, units] : context.rules.items)
		{
			const World::Line *pAsk = LineOf(reports, sourceId, itemId);
			if (!pAsk || pAsk->ask <= 0 || pAsk->askTotes == 0)
				continue;
			for (const std::string &destinationId : context.rules.venues)
			{
				const World::Line *pBid = LineOf(reports, destinationId, itemId);
				if (destinationId == sourceId || !pBid || pBid->bid <= 0 || pBid->bidTotes == 0)
					continue;
				const std::int64_t charges = Charges(
				    context.Registry(), context.rules.identities, record, sourceId, destinationId);
				std::optional<Deal> trade =
				    Estimate(context, itemId, *pAsk, *pBid, charges, balance, free);
				if (!trade || trade->estimate <= context.Policy().targetProfit)
					continue;
				const bool docked = record.dockedAt == sourceId;
				if (best && (trade->estimate < best->estimate ||
				             (trade->estimate == best->estimate &&
				              (!docked || record.dockedAt == best->sourceId))))
					continue;
				trade->sourceId = sourceId;
				trade->destinationId = destinationId;
				best = std::move(trade);
			}
		}
	return best;
}


// Where the lot aboard sells best from the owner's reports: a bid at least the lot's sell limit there,
// its docking fee in the floor, the highest net of the fee, then the venue the ship is docked at,
// then the scenario's order.
std::optional<std::string> ChooseDestination(const Context &context)
{
	const World::Job &job = context.Job();
	const World::Ship &record = context.Record();
	const double units = context.UnitsPerTote(job.itemId);
	std::optional<std::string> best;
	std::int64_t bestEstimate = 0;
	for (const std::string &venueId : context.rules.venues)
	{
		const World::Line *pBid = LineOf(context.Reports(), venueId, job.itemId);
		if (!pBid || pBid->bid <= 0 || pBid->bidTotes == 0)
			continue;
		const std::int64_t charges =
		    Charges(context.Registry(), context.rules.identities, record, {}, venueId);
		const std::optional<std::int64_t> limit = SellLimit(
		    Added(Floor(job, context.Policy()).first, charges), job.totes, units, context.rules.feeBps);
		const std::optional<std::int64_t> proceeds = Proceeds(
		    pBid->bid, units * static_cast<double>(std::min(pBid->bidTotes, job.totes)),
		    context.rules.feeBps);
		if (!limit || pBid->bid < *limit || !proceeds)
			continue;
		const std::int64_t estimate = *proceeds - charges;
		if (best &&
		    (estimate < bestEstimate ||
		     (estimate == bestEstimate && (record.dockedAt != venueId || record.dockedAt == *best))))
			continue;
		best = venueId;
		bestEstimate = estimate;
	}
	return best;
}


// A decision of a choosing job: a trade, or with a lot aboard a destination for it, and the journey or
// the trading it leads to. Whether the job can go on at once.
bool Choose(const Context &context)
{
	World::Job &job = context.Job();
	const std::string &docked = context.Record().dockedAt;
	if (job.totes > 0)
	{
		const std::optional<std::string> destination = ChooseDestination(context);
		if (!destination)
		{
			context.Block(World::Blockage::NoTrade);
			return false;
		}
		job.destinationId = *destination;
		if (docked == job.destinationId)
		{
			job.phase = Phase::Selling;
			return true;
		}
		context.Go(job.destinationId, Phase::ToDestination);
		return false;
	}
	const std::optional<Deal> trade = ChooseTrade(context);
	if (!trade)
	{
		context.Block(World::Blockage::NoTrade);
		return false;
	}
	job.itemId = trade->itemId;
	job.sourceId = trade->sourceId;
	job.destinationId = trade->destinationId;
	job.estimate = trade->estimate;
	if (docked == job.sourceId)
	{
		job.phase = Phase::Buying;
		return true;
	}
	context.Go(job.sourceId, Phase::ToSource);
	return false;
}


// Docked at the source: the trade estimated again on the live ask, and if it is still above the
// target, a buy at that ask whose rest is cancelled, everything bought loaded, and the journey on.
void Buy(const Context &context)
{
	World::Job &job = context.Job();
	const World::Reports &reports = context.Reports();
	const World::Line *pAsk = LineOf(reports, job.sourceId, job.itemId);
	const World::Line *pBid = LineOf(reports, job.destinationId, job.itemId);
	const std::int64_t charges = Charges(
	    context.Registry(), context.rules.identities, context.Record(), job.sourceId,
	    job.destinationId);
	const std::int64_t before = context.Balance();
	const std::optional<Deal> trade =
	    pAsk && pBid && pAsk->ask > 0 && pBid->bid > 0
	        ? Estimate(context, job.itemId, *pAsk, *pBid, charges, before, context.FreeHold())
	        : std::nullopt;
	if (!trade || trade->estimate <= context.Policy().targetProfit)
	{
		job = World::Job{};
		return;
	}

	const std::uint64_t held = context.Available(job.sourceId, job.itemId);
	(void)context.Act(
	    Trade::kPlace, Trade::kVersion,
	    Trade::Place{job.sourceId, job.itemId, World::Side::Buy, trade->totes, pAsk->ask});
	context.CancelRest(job.sourceId, job.itemId);
	const std::uint64_t bought = context.Available(job.sourceId, job.itemId) - held;
	if (bought == 0)
	{
		job = World::Job{};
		return;
	}
	if (context.Act(
	        Company::kLoad, Company::kVersion,
	        Company::Move{context.shipId, job.sourceId, job.itemId, bought}) != Result::Succeeded)
		return context.Block(World::Blockage::Refused);
	job.totes = bought;
	job.cost = before - context.Balance();
	job.charges = 0;
	job.allowanceSpent = 0;
	context.Go(job.destinationId, Phase::ToDestination);
}


// Docked at the destination: the lot unloaded and offered at its sell limit, the rest cancelled and
// loaded back, and what was sold taking its share of the lot's cost and charges, any shortfall below
// its share of the whole floor spending the allowance.
void Sell(const Context &context)
{
	World::Job &job = context.Job();
	const World::Policy &policy = context.Policy();
	const std::string venueId = context.Record().dockedAt;
	const auto [least, whole] = Floor(job, policy);
	const std::optional<std::int64_t> limit =
	    SellLimit(least, job.totes, context.UnitsPerTote(job.itemId), context.rules.feeBps);
	if (!limit)
		return context.Block(World::Blockage::NoTrade);
	if (context.Act(
	        Company::kUnload, Company::kVersion,
	        Company::Move{context.shipId, venueId, job.itemId, job.totes}) != Result::Succeeded)
		return context.Block(World::Blockage::Refused);

	const std::int64_t before = context.Balance();
	const std::uint64_t held = context.Available(venueId, job.itemId);
	(void)context.Act(
	    Trade::kPlace, Trade::kVersion,
	    Trade::Place{venueId, job.itemId, World::Side::Sell, job.totes, *limit});
	context.CancelRest(venueId, job.itemId);
	const std::uint64_t sold = held - context.Available(venueId, job.itemId);
	const std::int64_t net = context.Balance() - before;

	const std::int64_t left = whole - least;
	const std::int64_t shortfall = std::max<std::int64_t>(0, Share(whole, sold, job.totes) - net);
	job.allowanceSpent += std::min(shortfall, left);
	job.cost -= Share(job.cost, sold, job.totes);
	job.charges -= Share(job.charges, sold, job.totes);
	job.totes -= sold;
	if (job.totes == 0)
	{
		job = World::Job{};
		return;
	}
	if (context.Act(
	        Company::kLoad, Company::kVersion,
	        Company::Move{context.shipId, venueId, job.itemId, job.totes}) != Result::Succeeded)
		return context.Block(World::Blockage::Refused);
	job.phase = Phase::Choosing;
}


// One decision of the job. Whether it can go on to the next at once.
bool Decide(Context &context)
{
	World::Job &job = context.Job();
	const World::Ship &record = context.Record();
	switch (job.phase)
	{
		case Phase::Blocked:
			if (context.world.Clock().Step() < job.retry)
				return false;
			context.was = job.blockage;
			context.wasSince = job.blockedSince;
			job.phase = Phase::Choosing;
			job.blockage = World::Blockage::None;
			job.blockedSince = 0;
			job.retry = 0;
			return true;
		case Phase::ToSource:
		case Phase::ToDestination:
		{
			const auto *pJourney = context.Registry().try_get<const World::Journey>(context.ship);
			if (pJourney && !pJourney->Ended())
				return false;
			const bool toSource = job.phase == Phase::ToSource;
			if (pJourney && pJourney->leg == World::Leg::Failed)
			{
				context.Block(World::Blockage::JourneyFailed);
				return false;
			}
			const std::string &target = toSource ? job.sourceId : job.destinationId;
			if (record.dockedAt != target)
				job.phase = Phase::Choosing;
			else
				job.phase = toSource ? Phase::Buying : Phase::Selling;
			return true;
		}
		case Phase::Choosing: return Choose(context);
		case Phase::Buying:
			if (record.dockedAt != job.sourceId)
				job.phase = Phase::Choosing;
			else
				Buy(context);
			return false;
		case Phase::Selling:
			if (record.dockedAt != job.destinationId)
				job.phase = Phase::Choosing;
			else
				Sell(context);
			return false;
	}
	return false;
}


// Every enabled job's decisions this step, ship by ship in entity order.
void Run(SimWorld &world, const Rules &rules)
{
	entt::registry &registry = world.Registry();
	std::vector<entt::entity> ships;
	for (const auto [ship, policy, job] : registry.view<const World::Policy, const World::Job>().each())
		if (policy.enabled)
			ships.push_back(ship);
	std::ranges::sort(ships, {}, [](entt::entity ship) { return entt::to_integral(ship); });
	for (const entt::entity ship : ships)
	{
		const World::Ship &record = registry.get<const World::Ship>(ship);
		const entt::entity company = Required(rules.identities, Kind::Company, record.companyId);
		Context context{
		    world,
		    rules,
		    ship,
		    registry.get<const World::Identity>(ship).id,
		    {Command::ActorKind::Principal, registry.get<const World::Company>(company).principalId}};
		int decisions = 0;
		while (decisions < kMaxDecisions && Decide(context))
			++decisions;
	}
}


// The first step a job has a decision in (Jettison::Sim::Quiet): the next while one is choosing,
// trading or has its journey ended, a blocked one's retry, and never while every enabled job is on a
// journey under way, which travel's quiet wakes the world for.
std::uint64_t RunNext(const SimWorld &world)
{
	const entt::registry &registry = world.Registry();
	const std::uint64_t now = world.Clock().Step();
	std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
	for (const auto [ship, policy, job] : registry.view<const World::Policy, const World::Job>().each())
	{
		if (!policy.enabled)
			continue;
		if (job.phase == Phase::Blocked)
			next = std::min(next, std::max(job.retry, now));
		else if (job.phase == Phase::ToSource || job.phase == Phase::ToDestination)
		{
			const auto *pJourney = registry.try_get<const World::Journey>(ship);
			if (!pJourney || pJourney->Ended())
				return now;
		}
		else
			return now;
	}
	return next;
}
} // namespace


std::int64_t Charges(
    const entt::registry &registry, const World::Identities &identities, const World::Ship &ship,
    std::string_view sourceId, std::string_view destinationId)
{
	const auto docking = [&](std::string_view venueId)
	{
		return registry.get<const World::Fees>(Required(identities, Kind::Market, std::string{venueId}))
		    .docking;
	};
	std::int64_t charges = 0;
	const bool atSource = sourceId.empty() || ship.dockedAt == sourceId;
	if (!atSource)
		charges = docking(sourceId);
	if (atSource && ship.dockedAt == destinationId)
		return charges;
	return Added(charges, docking(destinationId));
}


std::optional<std::int64_t> SellLimit(
    std::int64_t floor, std::uint64_t totes, double unitsPerTote, int feeBps)
{
	const double keep = 1.0 - static_cast<double>(feeBps) / 10'000.0;
	if (totes == 0 || keep <= 0.0 || unitsPerTote <= 0.0)
		return std::nullopt;
	double most = 0.0;
	for (std::uint64_t n = 1; n <= totes; ++n)
	{
		const auto share = static_cast<double>(n == totes ? floor : CeilDiv(floor, n, totes));
		const double fills = static_cast<double>(std::min<std::uint64_t>(n, Trade::kMaxFills));
		most = std::max(most, (share + 1.5 * fills) / (static_cast<double>(n) * unitsPerTote * keep));
	}
	// A part in a billion over and a microcredit more, for the doubles the price rules compute in.
	const double micro = std::ceil(most * 1e6 * (1.0 + 1e-9)) + 1.0;
	if (!std::isfinite(micro) || micro >= 0x1p63)
		return std::nullopt;
	return std::max<std::int64_t>(1, static_cast<std::int64_t>(micro));
}


void ProvisionTraders(
    SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	entt::registry &registry = world.Registry();
	const Actor host{Command::ActorKind::Administrator, "Host"};
	for (const Content::TraderDefinition &trader : selected->second.traders)
	{
		if (requests.Act(
		        world, host, Company::kProvision, Company::kVersion,
		        Company::Provision{trader.principalId, trader.displayName, trader.companyName}) !=
		    Result::Succeeded)
			throw std::runtime_error{
			    std::format("the starter venue cannot take the trader '{}'", trader.principalId)};
		registry.get<World::Principal>(Required(identities, Kind::Principal, trader.principalId)).npc =
		    true;
		std::vector<std::string> ships;
		for (const auto [entity, ship, identity] :
		     registry.view<const World::Ship, const World::Identity>().each())
			if (registry.get<const World::Company>(Required(identities, Kind::Company, ship.companyId))
			        .principalId == trader.principalId)
				ships.push_back(identity.id);
		const Actor npc{Command::ActorKind::Principal, trader.principalId};
		for (const std::string &shipId : ships)
			JETTISON_VERIFY(
			    requests.Act(world, npc, kSet, kVersion, Set{shipId, true, 0, 0}) == Result::Succeeded);
	}
}


void RegisterTypes()
{
	static const bool registered = []
	{
		World::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<Set>("Manifest.Policy.Set")
		    .Field<&Set::shipId>("shipId", kSaved)
		    .Field<&Set::enabled>("enabled", kSaved)
		    .Field<&Set::targetProfit>("targetProfit", kSaved)
		    .Field<&Set::lossAllowance>("lossAllowance", kSaved);
		return true;
	}();
	(void)registered;
}


void Register(
    SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	auto rules = std::make_shared<Rules>(requests, identities);
	for (const Content::VenueDefinition &venue : selected->second.venues)
		rules->venues.push_back(venue.venueId);
	for (const Content::ItemSelection &selection : selected->second.items)
	{
		const auto item =
		    std::ranges::find(content.gameMode.items, selection.itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(item != content.gameMode.items.end());
		rules->items.emplace_back(
		    selection.itemId, item->unitsPerCubicMetre / static_cast<double>(Core::kTotesPerIbc));
	}
	rules->feeBps = content.tuning.market.listingFeeBps;
	const double turn = std::round(content.tuning.economy.intervalSeconds / world.FixedStep());
	if (!(turn >= 1.0))
		throw std::invalid_argument{"economy.intervalSeconds is less than one of the world's steps"};
	rules->turnSteps = static_cast<std::uint64_t>(turn);

	requests.Register<Set>(
	    std::string{kSet}, kVersion, Command::ActorKind::Principal,
	    [&identities](SimWorld &on, const Actor &actor, const Set &set)
	    { return Sets(on, identities, actor, set); });

	using Jettison::Ecs::Access;
	world.RegisterSystem(
	    {.name = std::string{kSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     // What the commands it sends write, besides its own.
	     .access = Access::Of<
	                   World::Policy, World::Job, World::Ship, World::Company, World::Reports,
	                   World::Fees, World::Journey, World::Account, World::Stock, World::Order,
	                   World::Identity, World::Cargo, World::Systems>()
	                   .Writing<
	                       World::Job, World::Order, World::Account, World::Stock, World::Identity,
	                       World::Sales, World::Cargo, World::Journey, World::Arrears>(),
	     .after = {std::string{Trade::kBrokers}},
	     .run = [rules, &world](entt::registry &, double) { Run(world, *rules); },
	     .before = {std::string{Jettison::Spatial::SystemIndices::kSystemName}}},
	    {.next = [](const SimWorld &target) { return RunNext(target); }});
}
} // namespace Manifest::Policy
