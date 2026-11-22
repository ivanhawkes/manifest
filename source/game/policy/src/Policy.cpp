#include <manifest/policy/Policy.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>

#include <jettison/core/Assert.h>
#include <jettison/dock/Docks.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/physics/Components.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/space/Cruise.h>
#include <jettison/spatial/FrameIndices.h>

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
constexpr double kSecondsPerDay = 86400.0;
constexpr double kKilogramsPerTonne = 1000.0;
constexpr double kMicrocreditsPerCredit = 1e6;
// The fuel a leg is estimated to leave in its tanks at the least, a gram, so that Travel::Burned's
// rounding up of its own sums never refuses a thrust chosen as the most the tanks allow.
constexpr double kFuelMarginKilograms = 0.001;
// A job's decisions in one step: choosing leads at once to buying where its ship is docked, or to
// selling, and a blocked job's retry to choosing.
constexpr int kMaxDecisions = 4;


// What the system runs on, worked out once from the content.
struct Rules
{
	Command::Requests &requests;
	World::Identities &identities;
	const Content::ContentSet &content;
	// The scenario's venues and items, in its order, each item with the count of its units a tote holds
	// and a tote's tonnes, packed.
	struct Item
	{
		std::string itemId;
		double unitsPerTote{0.0};
		double tonnesPerTote{0.0};
	};
	std::vector<std::string> venues;
	std::vector<Item> items;
	int feeBps{0};
	std::uint64_t turnSteps{0};
	std::uint64_t daySteps{0};
	// How long arrears stand before their ship's cargo is seized, in steps.
	std::uint64_t graceSteps{0};
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
	if (set.targetProfit < 0 || set.lossAllowance < 0 || set.thrustBps > World::kFullThrustBps)
		return Result::InvalidQuantity;
	registry.emplace_or_replace<World::Policy>(
	    ship, set.enabled, set.targetProfit, set.lossAllowance, set.thrustBps);
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


// The venue's docking fee as posted now for the ship's fee class (D-100).
std::int64_t DockingFee(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, const World::Ship &ship, std::string_view venueId)
{
	const World::ClassFees *pFees = World::Find(
	    registry.get<const World::Fees>(Required(identities, Kind::Market, std::string{venueId})),
	    Content::FeeClass(content, ship.hullId));
	JETTISON_VERIFY(pFees != nullptr);
	return pFees->docking;
}


std::int64_t Added(std::int64_t left, std::int64_t right)
{
	return left + std::min(right, kMaxCredits - left);
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


// The company's last price for the item (World::Paid), raw microcredits per its unit; 0 for a good
// it never bought (D-100).
std::int64_t LastPaid(
    const entt::registry &registry, const World::Identities &identities, const std::string &companyId,
    std::string_view itemId)
{
	const auto *pPaid =
	    registry.try_get<const World::Paid>(Required(identities, Kind::Company, companyId));
	const World::PaidPrice *pPrice = pPaid ? World::Find(*pPaid, itemId) : nullptr;
	return pPrice ? pPrice->price : 0;
}


// What the quantities come to at their prices, raw microcredits per unit, in whole credits rounded up.
std::int64_t Worth(std::initializer_list<std::pair<double, std::int64_t>> amounts)
{
	double micro = 0.0;
	for (const auto &[quantity, price] : amounts)
		micro += quantity * static_cast<double>(price);
	const double credits = std::ceil(micro / kMicrocreditsPerCredit);
	return credits >= 0x1p63 ? kMaxCredits : static_cast<std::int64_t>(credits);
}


// What the life support is worth at the company's last prices for its goods: the oxygen in
// kilograms, its unit, and the cartridges' occupant-days, a cartridge for each cartridgeOccupantDays.
std::int64_t Upkeep(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, const std::string &companyId, double oxygenKilograms,
    double cartridgeDays)
{
	const Content::LifeSupportTuning &tuning = content.tuning.lifeSupport;
	return Worth(
	    {{oxygenKilograms, LastPaid(registry, identities, companyId, tuning.oxygenItemId)},
	     {cartridgeDays / tuning.cartridgeOccupantDays,
	      LastPaid(registry, identities, companyId, tuning.cartridgeItemId)}});
}


// A leg of a route: the venue it leaves, empty from space, its metres, the docking fee of the venue it
// flies to, and whether a trade's lot is aboard.
struct Leg
{
	std::string fromId;
	double metres{0.0};
	std::int64_t fee{0};
	bool laden{false};
};


// The legs from where the ship is to the destination, by the source
// where there is one and the ship is not docked there; none docked at the destination.
std::vector<Leg> Legs(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity ship, std::string_view sourceId,
    std::string_view destinationId)
{
	const World::Ship &record = registry.get<const World::Ship>(ship);
	const auto at = [&](std::string_view venueId)
	{
		const entt::entity station = Travel::StationOf(registry, content, venueId);
		JETTISON_VERIFY(station != entt::null);
		return registry.get<const Jettison::Sim::Transform>(station).position;
	};
	// Where the ship is, its pad's place while docked, as Travel::Travel measures a leg from.
	const glm::dvec3 here = registry.get<const Jettison::Sim::Transform>(ship).position;
	const bool atSource = sourceId.empty() || record.dockedAt == sourceId;
	if (atSource && record.dockedAt == destinationId)
		return {};
	std::vector<Leg> legs;
	if (!atSource)
		legs.push_back(
		    {record.dockedAt, glm::distance(here, at(sourceId)),
		     DockingFee(registry, identities, content, record, sourceId), false});
	legs.push_back(
	    {atSource ? record.dockedAt : std::string{sourceId},
	     glm::distance(atSource ? here : at(sourceId), at(destinationId)),
	     DockingFee(registry, identities, content, record, destinationId), true});
	return legs;
}


// The route's legs flown at the thrust, as EstimateRoute is.
Route Priced(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity ship, const std::vector<Leg> &legs,
    std::uint32_t thrustBps, double lotTonnes)
{
	const World::Ship &record = registry.get<const World::Ship>(ship);
	const Content::FuelTuning &tuning = content.tuning.fuel;
	const Content::LifeSupportTuning &lifeSupport = content.tuning.lifeSupport;
	const auto hull = std::ranges::find(content.hulls, record.hullId, &Core::ShipClassSpec::hullId);
	const int occupants = hull == content.hulls.end() ? 0 : hull->occupants.value_or(0);

	// The drive as Travel::Travel flies it: the authored one, on a profile at the thrust where the
	// ship's drive has one.
	const auto authored = content.drives.find(Content::RegistryName(record.hullId));
	const auto *pDrive = registry.try_get<const Jettison::Space::CruiseDrive>(ship);
	std::optional<Jettison::Space::CruiseDrive> flown;
	if (authored != content.drives.end())
		flown = authored->second;
	else if (pDrive)
		flown = *pDrive;
	const bool burns = flown && pDrive && pDrive->profile;
	if (burns)
		flown->profile = Travel::Profile(
		    flown->profile ? *flown->profile : *pDrive->profile, thrustBps, Travel::kMilliGPerG);

	const auto *pFuel = registry.try_get<const World::Fuel>(ship);
	const World::Fuel current = pFuel ? *pFuel : World::Fuel{};
	const std::optional<World::Fuel> tanks =
	    identities.FuelTanks(record, registry.try_get<const World::Systems>(ship));
	const double dry = Travel::TonnesAboard(registry, identities, content, ship) -
	                   (current.reactionMass + current.pellets) / kKilogramsPerTonne;
	const auto *pRefuel = registry.try_get<const World::Refuel>(ship);
	const bool refuels = !pRefuel || pRefuel->enabled;
	const std::string &principalId =
	    registry.get<const World::Company>(Required(identities, Kind::Company, record.companyId))
	        .principalId;
	const auto *pReports =
	    registry.try_get<const World::Reports>(Required(identities, Kind::Principal, principalId));

	Route route;
	World::Fuel left = current;
	for (const Leg &leg : legs)
	{
		const double seconds = flown ? Jettison::Space::CruiseSeconds(*flown, leg.metres) : 0.0;
		route.seconds += seconds + tuning.legAllowanceSeconds;
		const double occupantDays = occupants * seconds / kSecondsPerDay;
		route.charges = Added(
		    route.charges,
		    Added(
		        leg.fee, Upkeep(
		                     registry, identities, content, record.companyId,
		                     occupantDays * lifeSupport.oxygenKilogramsPerOccupantDay, occupantDays)));
		if (!burns)
			continue;

		// Each fuel the venue the leg leaves sells, where the ship refuels, at its ask and filling its
		// tanks; at the last price paid with what is left in them where not.
		const auto sold = [&](const std::string &itemId) -> const World::Line *
		{
			const World::Line *pLine = refuels && pReports && !leg.fromId.empty()
			                               ? LineOf(*pReports, leg.fromId, itemId)
			                               : nullptr;
			return pLine && !pLine->asks.empty() ? pLine : nullptr;
		};
		const World::Line *pMass = sold(tuning.reactionMassItemId);
		const World::Line *pPellets = sold(tuning.pelletItemId);
		// shortcut: a venue selling pellets is taken to fill the magazine, which takes a tote in the
		// hold; refine when a policy's estimate is seen refused for its pellets.
		const World::Fuel start{
		    pMass && tanks ? tanks->reactionMass : left.reactionMass,
		    pPellets && tanks ? tanks->pellets : left.pellets};
		const World::Fuel burned = Travel::Burned(
		    tuning, *flown->profile,
		    dry + (start.reactionMass + start.pellets) / kKilogramsPerTonne +
		        (leg.laden ? lotTonnes : 0.0),
		    leg.metres);
		route.fuelled = route.fuelled &&
		                burned.reactionMass + kFuelMarginKilograms <= start.reactionMass &&
		                burned.pellets + kFuelMarginKilograms <= start.pellets;
		left = {
		    std::max(0.0, start.reactionMass - burned.reactionMass),
		    std::max(0.0, start.pellets - burned.pellets)};
		const auto price = [&](const World::Line *pLine, const std::string &itemId)
		{ return pLine ? pLine->Ask() : LastPaid(registry, identities, record.companyId, itemId); };
		route.charges = Added(
		    route.charges, Worth(
		                       {{burned.reactionMass, price(pMass, tuning.reactionMassItemId)},
		                        {burned.pellets, price(pPellets, tuning.pelletItemId)}}));
	}
	return route;
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

	// The thrust the job's legs fly: its policy's own, or the one chosen with its trade.
	std::uint32_t Thrust() const
	{
		return Policy().thrustBps != 0 ? Policy().thrustBps : Job().thrustBps;
	}

	// What the route from where the ship is comes to at the thrust, with that many tonnes of lot.
	Route Priced(const std::vector<Leg> &legs, std::uint32_t thrustBps, double lotTonnes) const
	{
		return ::Manifest::Policy::Priced(
		    Registry(), rules.identities, rules.content, ship, legs, thrustBps, lotTonnes);
	}

	std::vector<Leg> Legs(std::string_view sourceId, std::string_view destinationId) const
	{
		return ::Manifest::Policy::Legs(
		    Registry(), rules.identities, rules.content, ship, sourceId, destinationId);
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

	const Rules::Item &Item(const std::string &itemId) const
	{
		const auto item = std::ranges::find(rules.items, itemId, &Rules::Item::itemId);
		JETTISON_VERIFY(item != rules.items.end());
		return *item;
	}

	double UnitsPerTote(const std::string &itemId) const
	{
		return Item(itemId).unitsPerTote;
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
		    DockingFee(Registry(), rules.identities, rules.content, Record(), venueId);
		if (Act(Travel::kTravel, Travel::kVersion,
		        Travel::Travel{shipId, venueId, Thrust(), Travel::kMilliGPerG}) != Result::Succeeded)
			return Block(World::Blockage::Refused);
		World::Job &job = Job();
		if (job.totes > 0)
			job.charges = Added(job.charges, fee);
		job.phase = phase;
	}
};


// A trade a job may make: its item, from the source to the destination, the totes, what it is
// estimated to make after charges, the limit its buy walks the asks to, and the thrust it flies.
struct Deal
{
	std::string itemId;
	std::string sourceId;
	std::string destinationId;
	std::uint64_t totes{0};
	std::int64_t estimate{0};
	std::int64_t limit{0};
	std::uint32_t thrustBps{World::kFullThrustBps};
};


// A route's charges with that many totes of the lot aboard, none where its tanks cannot fly it.
using ChargesOf = std::function<std::optional<std::int64_t>(std::uint64_t totes)>;


std::uint64_t Totes(const std::vector<World::Level> &side)
{
	std::uint64_t totes = 0;
	for (const World::Level &level : side)
		totes += level.totes;
	return totes;
}


// What a side's levels, best first, come to for the totes, each level estimated as one fill: a buy's
// outlay or a sale's proceeds, and the price the last tote fills at. None past the levels' totes or
// for an amount the ledger cannot hold.
struct Walked
{
	std::int64_t total{0};
	std::int64_t limit{0};
};
std::optional<Walked> Walk(
    const std::vector<World::Level> &side, std::uint64_t totes, double units, int feeBps, bool buying)
{
	Walked walked;
	for (const World::Level &level : side)
	{
		if (totes == 0)
			break;
		const std::uint64_t taken = std::min(totes, level.totes);
		const double traded = units * static_cast<double>(taken);
		const std::optional<std::int64_t> each =
		    buying ? Outlay(level.price, traded, feeBps) : Proceeds(level.price, traded, feeBps);
		if (!each || *each > kMaxCredits - walked.total)
			return std::nullopt;
		walked.total += *each;
		walked.limit = level.price;
		totes -= taken;
	}
	return totes == 0 ? std::optional{walked} : std::nullopt;
}


// The trade between a source's asks and a destination's bids, walking both (D-102): of the totes both
// sides hold, the hold's free totes and what the account covers after the charges with them aboard at
// the deepest ask's escrow, the count estimated to make the most, the fewest of equals. None for no
// totes, or none the tanks can fly.
std::optional<Deal> Estimate(
    const Context &context, const std::string &itemId, const World::Line &ask, const World::Line &bid,
    const ChargesOf &chargesOf, std::int64_t balance, std::uint64_t free)
{
	const std::optional<std::int64_t> unladen = chargesOf(0);
	if (!unladen || balance <= *unladen)
		return std::nullopt;
	const double units = context.UnitsPerTote(itemId);
	const int feeBps = context.rules.feeBps;
	const std::uint64_t most = std::min({Totes(ask.asks), Totes(bid.bids), free});
	// Each tote further fills at a worse price on both sides, so the best count is at a level's edge
	// or a cap: what the hold, either side or the funds at a level's escrow allow, the lot's fuel
	// rising with its mass.
	std::vector<std::uint64_t> counts{free};
	std::uint64_t asked = 0;
	for (const World::Level &level : ask.asks)
	{
		counts.push_back(asked += level.totes);
		const std::optional<std::int64_t> perTote = Trade::PerTote(level.price, units, feeBps);
		if (!perTote || *perTote <= 0)
			continue;
		const auto covered = [&](std::int64_t charges)
		{ return static_cast<std::uint64_t>((balance - charges) / *perTote); };
		const std::uint64_t cap = covered(*unladen);
		counts.push_back(cap);
		if (const std::optional<std::int64_t> laden = chargesOf(std::min(cap, most));
		    laden && balance > *laden)
			counts.push_back(covered(*laden));
	}
	std::uint64_t bidden = 0;
	for (const World::Level &level : bid.bids)
		counts.push_back(bidden += level.totes);

	std::optional<Deal> best;
	for (std::uint64_t totes : counts)
	{
		totes = std::min(totes, most);
		const std::optional<std::int64_t> charges = totes > 0 ? chargesOf(totes) : std::nullopt;
		const std::optional<Walked> buy =
		    charges && balance > *charges ? Walk(ask.asks, totes, units, feeBps, true) : std::nullopt;
		const std::optional<Walked> sale =
		    buy ? Walk(bid.bids, totes, units, feeBps, false) : std::nullopt;
		const std::optional<std::int64_t> perTote =
		    sale ? Trade::PerTote(buy->limit, units, feeBps) : std::nullopt;
		if (!perTote || *perTote <= 0 ||
		    totes > static_cast<std::uint64_t>((balance - *charges) / *perTote))
			continue;
		const std::int64_t estimate = sale->total - buy->total - *charges;
		if (best && (estimate < best->estimate || (estimate == best->estimate && totes >= best->totes)))
			continue;
		best = Deal{itemId, {}, {}, totes, estimate, buy->limit};
	}
	return best;
}


// The most thrust, in basis points, at which the route is fuelled; none for not even the least.
// Fuel rises with the thrust, so a halving search finds it.
std::optional<std::uint32_t> MostThrust(const std::function<bool(std::uint32_t)> &fuelled)
{
	if (!fuelled(1))
		return std::nullopt;
	std::uint32_t low = 1;
	std::uint32_t high = World::kFullThrustBps;
	while (low < high)
	{
		const std::uint32_t middle = low + ((high - low + 1) / 2);
		if (fuelled(middle))
			low = middle;
		else
			high = middle - 1;
	}
	return low;
}


// What a sale must recover of the lot: its cost, its charges and the target, less the allowance it has
// left, and the whole without the allowance.
std::pair<std::int64_t, std::int64_t> Floor(const World::Job &job, const World::Policy &policy)
{
	const std::int64_t whole = Added(Added(job.cost, job.charges), policy.targetProfit);
	const std::int64_t left = std::max<std::int64_t>(0, policy.lossAllowance - job.allowanceSpent);
	return {whole - left, whole};
}


// What rounding can take of a fill, in half credits: a credit and a half, as SellLimit allows for.
constexpr std::int64_t kRoundingHalvesPerFill = 3;


// The limit a sale of the lot walks the bids to (D-102): the lowest bid level the sale can reach with
// what it sells to there recovering its share of the least, and never above the uniform limit at
// which every tote does. None where that is none.
std::optional<std::int64_t> WalkLimit(
    const World::Line *pBid, std::int64_t least, std::uint64_t totes, double units, int feeBps)
{
	std::optional<std::int64_t> limit = SellLimit(least, totes, units, feeBps);
	if (!limit || !pBid)
		return limit;
	std::uint64_t sold = 0;
	std::int64_t proceeds = 0;
	for (const World::Level &level : pBid->bids)
	{
		if (sold == totes)
			break;
		const std::uint64_t taken = std::min(totes - sold, level.totes);
		const std::optional<std::int64_t> each =
		    Proceeds(level.price, units * static_cast<double>(taken), feeBps);
		if (!each || *each > kMaxCredits - proceeds)
			break;
		sold += taken;
		proceeds += *each;
		const auto fills = static_cast<std::int64_t>(std::min<std::uint64_t>(sold, Trade::kMaxFills));
		if (level.price < *limit &&
		    2 * proceeds - kRoundingHalvesPerFill * fills >= 2 * Share(least, sold, totes))
			limit = level.price;
	}
	return limit;
}


// The best trade from the owner's reports above the target: the most an hour, its estimate over its
// route's seconds, at the policy's thrust or, choosing, at the best of each of fuel.thrustSharesBps and
// the most the tanks can fly below one they cannot; then a source the ship is docked at, then the
// scenario's order of source, item, destination and thrust.
std::optional<Deal> ChooseTrade(const Context &context)
{
	const World::Reports &reports = context.Reports();
	const World::Ship &record = context.Record();
	const std::int64_t balance = context.Balance();
	const std::uint64_t free = context.FreeHold();
	const std::vector<std::string> &venues = context.rules.venues;
	const std::uint32_t fixed = context.Policy().thrustBps;
	const std::vector<std::uint32_t> listed =
	    fixed != 0 ? std::vector{fixed} : context.rules.content.tuning.fuel.thrustSharesBps;
	// Each route's legs, by source and destination.
	std::vector<std::vector<Leg>> routes(venues.size() * venues.size());
	for (std::size_t source = 0; source < venues.size(); ++source)
		for (std::size_t destination = 0; destination < venues.size(); ++destination)
			if (source != destination)
				routes[(source * venues.size()) + destination] =
				    context.Legs(venues[source], venues[destination]);

	std::optional<Deal> best;
	double bestRate = 0.0;
	for (std::size_t source = 0; source < venues.size(); ++source)
		for (const Rules::Item &item : context.rules.items)
		{
			const World::Line *pAsk = LineOf(reports, venues[source], item.itemId);
			if (!pAsk || pAsk->asks.empty())
				continue;
			for (std::size_t destination = 0; destination < venues.size(); ++destination)
			{
				const World::Line *pBid = LineOf(reports, venues[destination], item.itemId);
				if (destination == source || !pBid || pBid->bids.empty())
					continue;
				const std::vector<Leg> &legs = routes[(source * venues.size()) + destination];
				const auto priced = [&](std::uint32_t thrustBps, std::uint64_t totes)
				{
					return context.Priced(
					    legs, thrustBps, static_cast<double>(totes) * item.tonnesPerTote);
				};
				std::vector<std::uint32_t> thrusts = listed;
				const std::uint64_t most = std::min({Totes(pAsk->asks), Totes(pBid->bids), free});
				if (fixed == 0 &&
				    std::ranges::any_of(
				        listed, [&](std::uint32_t bps) { return !priced(bps, most).fuelled; }))
					if (const std::optional<std::uint32_t> thrust =
					        MostThrust([&](std::uint32_t bps) { return priced(bps, most).fuelled; });
					    thrust && std::ranges::find(thrusts, *thrust) == thrusts.end())
						thrusts.push_back(*thrust);
				for (const std::uint32_t thrustBps : thrusts)
				{
					const ChargesOf chargesOf = [&](std::uint64_t totes)
					{
						const Route route = priced(thrustBps, totes);
						return route.fuelled ? std::optional{route.charges} : std::nullopt;
					};
					std::optional<Deal> trade =
					    Estimate(context, item.itemId, *pAsk, *pBid, chargesOf, balance, free);
					if (!trade || trade->estimate <= context.Policy().targetProfit)
						continue;
					const double rate =
					    static_cast<double>(trade->estimate) / priced(thrustBps, 0).seconds;
					const bool docked = record.dockedAt == venues[source];
					if (best && (rate < bestRate ||
					             (rate == bestRate && (!docked || record.dockedAt == best->sourceId))))
						continue;
					trade->sourceId = venues[source];
					trade->destinationId = venues[destination];
					trade->thrustBps = thrustBps;
					best = std::move(trade);
					bestRate = rate;
				}
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
		if (!pBid || pBid->bids.empty())
			continue;
		const Route route = context.Priced(context.Legs({}, venueId), context.Thrust(), 0.0);
		if (!route.fuelled)
			continue;
		const std::int64_t charges = route.charges;
		const std::optional<std::int64_t> limit = WalkLimit(
		    pBid, Added(Floor(job, context.Policy()).first, charges), job.totes, units,
		    context.rules.feeBps);
		if (!limit || pBid->Bid() < *limit)
			continue;
		// The bids at the limit or above, as far as the lot goes.
		std::uint64_t sellable = 0;
		for (const World::Level &level : pBid->bids)
			sellable += level.price >= *limit ? level.totes : 0;
		const std::optional<Walked> sale =
		    Walk(pBid->bids, std::min(sellable, job.totes), units, context.rules.feeBps, false);
		if (!sale)
			continue;
		const std::int64_t estimate = sale->total - charges;
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
	job.thrustBps = trade->thrustBps;
	if (docked == job.sourceId)
	{
		job.phase = Phase::Buying;
		return true;
	}
	context.Go(job.sourceId, Phase::ToSource);
	return false;
}


// Docked at the source: the trade estimated again on the live asks, and if it is still above the
// target, a buy walking them to its limit whose rest is cancelled, everything bought loaded, and the
// journey on.
void Buy(const Context &context)
{
	World::Job &job = context.Job();
	const World::Reports &reports = context.Reports();
	const World::Line *pAsk = LineOf(reports, job.sourceId, job.itemId);
	const World::Line *pBid = LineOf(reports, job.destinationId, job.itemId);
	const std::vector<Leg> legs = context.Legs(job.sourceId, job.destinationId);
	const double tonnesPerTote = context.Item(job.itemId).tonnesPerTote;
	const std::uint32_t thrustBps = context.Thrust();
	const ChargesOf chargesOf = [&](std::uint64_t totes)
	{
		const Route route = context.Priced(legs, thrustBps, static_cast<double>(totes) * tonnesPerTote);
		return route.fuelled ? std::optional{route.charges} : std::nullopt;
	};
	const std::int64_t before = context.Balance();
	const std::optional<Deal> trade =
	    pAsk && pBid
	        ? Estimate(context, job.itemId, *pAsk, *pBid, chargesOf, before, context.FreeHold())
	        : std::nullopt;
	if (!trade || trade->estimate <= context.Policy().targetProfit)
	{
		job = World::Job{};
		return;
	}

	const std::uint64_t held = context.Available(job.sourceId, job.itemId);
	(void)context.Act(
	    Trade::kPlace, Trade::kVersion,
	    Trade::Place{job.sourceId, job.itemId, World::Side::Buy, trade->totes, trade->limit});
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


// Docked at the destination: the lot unloaded and offered at its walk limit, the rest cancelled and
// loaded back, and what was sold taking its share of the lot's cost and charges, any shortfall below
// its share of the whole floor spending the allowance.
void Sell(const Context &context)
{
	World::Job &job = context.Job();
	const World::Policy &policy = context.Policy();
	const std::string venueId = context.Record().dockedAt;
	const auto [least, whole] = Floor(job, policy);
	const std::optional<std::int64_t> limit = WalkLimit(
	    LineOf(context.Reports(), venueId, job.itemId), least, job.totes,
	    context.UnitsPerTote(job.itemId), context.rules.feeBps);
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
			// The life support drawn and the fuel burned on the way, charged to the lot at the
			// company's last prices as the ship docks (D-100, W21).
			if (!record.dockedAt.empty() &&
			    (job.oxygenKilograms > 0.0 || job.cartridgeDays > 0.0 ||
			     job.reactionMassKilograms > 0.0 || job.pelletKilograms > 0.0))
			{
				const entt::registry &registry = context.Registry();
				const World::Identities &identities = context.rules.identities;
				const Content::FuelTuning &fuel = context.rules.content.tuning.fuel;
				job.charges = Added(
				    Added(
				        job.charges, Upkeep(
				                         registry, identities, context.rules.content, record.companyId,
				                         job.oxygenKilograms, job.cartridgeDays)),
				    Worth(
				        {{job.reactionMassKilograms,
				          LastPaid(registry, identities, record.companyId, fuel.reactionMassItemId)},
				         {job.pelletKilograms,
				          LastPaid(registry, identities, record.companyId, fuel.pelletItemId)}}));
				job.oxygenKilograms = 0.0;
				job.cartridgeDays = 0.0;
				job.reactionMassKilograms = 0.0;
				job.pelletKilograms = 0.0;
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


// The seized totes of the item leaving the ship's job's lot, as a sale's do, with their shares of its
// cost and charges; a job left with no lot chooses again.
void Seized(World::Job &job, const std::string &itemId, std::uint64_t seized)
{
	if (job.totes == 0 || job.itemId != itemId || seized == 0)
		return;
	const std::uint64_t totes = std::min(seized, job.totes);
	job.cost -= Share(job.cost, totes, job.totes);
	job.charges -= Share(job.charges, totes, job.totes);
	job.totes -= totes;
	if (job.totes == 0)
		job = World::Job{};
}


// As the day's last step ends, each ship whose arrears have stood the grace, in entity order, has its
// cargo sold for them through its owner's commands (kSeizureSystem): lot by lot in hold order, a tote
// at a time at the venue's live bids, until the proceeds cover what is owed or the bids run out, the
// rest loaded back; then its company's account pays the arrears as far as it can.
void Seize(SimWorld &world, const Rules &rules)
{
	const std::uint64_t now = world.Clock().Step();
	if ((now + 1) % rules.daySteps != 0)
		return;
	entt::registry &registry = world.Registry();
	std::vector<entt::entity> ships;
	for (const auto [ship, arrears] : registry.view<const World::Arrears>().each())
		if (now - arrears.since >= rules.graceSteps)
			ships.push_back(ship);
	std::ranges::sort(ships, {}, [](entt::entity ship) { return entt::to_integral(ship); });
	for (const entt::entity ship : ships)
	{
		const World::Ship &record = registry.get<const World::Ship>(ship);
		const std::string companyId = record.companyId;
		const std::string venueId = record.dockedAt;
		const Context context{
		    world,
		    rules,
		    ship,
		    registry.get<const World::Identity>(ship).id,
		    {Command::ActorKind::Principal,
		     registry.get<const World::Company>(Required(rules.identities, Kind::Company, companyId))
		         .principalId}};
		const std::int64_t owed = registry.get<const World::Arrears>(ship).owed;
		const std::int64_t before = context.Balance();
		const std::vector<World::Lot> lots = registry.get<const World::Cargo>(ship).lots;
		for (const World::Lot &lot : lots)
		{
			if (context.Balance() - before >= owed)
				break;
			if (context.Act(
			        Company::kUnload, Company::kVersion,
			        Company::Move{context.shipId, venueId, lot.itemId, lot.totes}) != Result::Succeeded)
				continue;
			const std::uint64_t held = context.Available(venueId, lot.itemId);
			std::uint64_t sold = 0;
			while (sold < lot.totes && context.Balance() - before < owed)
			{
				// At the least a price can be, so the tote takes the best bid there is.
				(void)context.Act(
				    Trade::kPlace, Trade::kVersion,
				    Trade::Place{venueId, lot.itemId, World::Side::Sell, 1, 1});
				context.CancelRest(venueId, lot.itemId);
				if (context.Available(venueId, lot.itemId) + sold == held)
					break;
				++sold;
			}
			if (sold < lot.totes)
				JETTISON_VERIFY(
				    context.Act(
				        Company::kLoad, Company::kVersion,
				        Company::Move{context.shipId, venueId, lot.itemId, lot.totes - sold}) ==
				    Result::Succeeded);
			if (auto *pJob = registry.try_get<World::Job>(ship))
				Seized(*pJob, lot.itemId, sold);
		}

		World::Arrears &arrears = registry.get<World::Arrears>(ship);
		World::Account &account =
		    registry.get<World::Account>(Required(rules.identities, Kind::CompanyAccount, companyId));
		World::Account &venue =
		    registry.get<World::Account>(Required(rules.identities, Kind::VenueAccount, venueId));
		const std::int64_t paid =
		    std::min({arrears.owed, account.balance, kMaxCredits - venue.balance});
		account.balance -= paid;
		venue.balance += paid;
		arrears.owed -= paid;
		if (arrears.owed == 0)
			registry.remove<World::Arrears>(ship);
	}
}


// The first step the seizure has to run in (Jettison::Sim::Quiet): the day's last while any ship owes
// arrears; otherwise never.
std::uint64_t SeizeNext(const SimWorld &world, const Rules &rules)
{
	if (world.Registry().view<const World::Arrears>().empty())
		return std::numeric_limits<std::uint64_t>::max();
	const std::uint64_t now = world.Clock().Step();
	return now + (rules.daySteps - 1 - now % rules.daySteps);
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


Route EstimateRoute(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity ship, std::string_view sourceId,
    std::string_view destinationId, std::uint32_t thrustBps, double lotTonnes)
{
	return Priced(
	    registry, identities, content, ship,
	    Legs(registry, identities, content, ship, sourceId, destinationId), thrustBps, lotTonnes);
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
		const auto fills = static_cast<double>(std::min<std::uint64_t>(n, Trade::kMaxFills));
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
		    .Field<&Set::lossAllowance>("lossAllowance", kSaved)
		    .Field<&Set::thrustBps>("thrustBps", kSaved);
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
	auto rules = std::make_shared<Rules>(requests, identities, content);
	for (const Content::VenueDefinition &venue : selected->second.venues)
		rules->venues.push_back(venue.venueId);
	for (const Content::ItemSelection &selection : selected->second.items)
	{
		const auto item =
		    std::ranges::find(content.gameMode.items, selection.itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(item != content.gameMode.items.end());
		const auto totes = static_cast<double>(Core::kTotesPerIbc);
		rules->items.push_back(
		    {selection.itemId, item->unitsPerCubicMetre / totes,
		     item->density * item->packFactor / totes});
	}
	rules->feeBps = content.tuning.market.listingFeeBps;
	const double turn = std::round(content.tuning.economy.intervalSeconds / world.FixedStep());
	if (!(turn >= 1.0))
		throw std::invalid_argument{"economy.intervalSeconds is less than one of the world's steps"};
	rules->turnSteps = static_cast<std::uint64_t>(turn);
	// A day as the berth fees count it, in whole hours of steps.
	rules->daySteps = static_cast<std::uint64_t>(std::round(3600.0 / world.FixedStep())) * 24;
	rules->graceSteps =
	    static_cast<std::uint64_t>(content.tuning.charges.seizureGraceDays) * rules->daySteps;

	requests.Register<Set>(
	    std::string{kSet}, kVersion, Command::ActorKind::Principal,
	    [&identities](SimWorld &on, const Actor &actor, const Set &set)
	    { return Sets(on, identities, actor, set); });

	using Jettison::Ecs::Access;
	world.RegisterSystem(
	    {.name = std::string{kSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     // What the commands it sends write, besides its own.
	     .access =
	         Access::Of<
	             World::Policy, World::Job, World::Ship, World::Company, World::Reports, World::Fees,
	             World::Journey, World::Account, World::Stock, World::Order, World::Identity,
	             World::Cargo, World::Systems, World::Paid, Jettison::Sim::Transform,
	             Jettison::Dock::Platform, Jettison::Physics::OnRails, Jettison::Sim::InFrame>()
	             .Writing<
	                 World::Job, World::Order, World::Account, World::Stock, World::Identity,
	                 World::Sales, World::Cargo, World::Journey, World::Arrears, World::Paid>(),
	     .after = {std::string{Trade::kBrokers}},
	     .run = [rules, &world](entt::registry &, double) { Run(world, *rules); },
	     .before = {std::string{Jettison::Spatial::FrameIndices::kSystemName}}},
	    {.next = [](const SimWorld &target) { return RunNext(target); }});
	world.RegisterSystem(
	    {.name = std::string{kSeizureSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     // What the commands it sends write, besides its own.
	     .access = Access::Of<
	                   World::Arrears, World::Job, World::Ship, World::Company, World::Account,
	                   World::Stock, World::Order, World::Identity, World::Cargo, World::Systems>()
	                   .Writing<
	                       World::Arrears, World::Job, World::Order, World::Account, World::Stock,
	                       World::Identity, World::Sales, World::Cargo, World::Paid>(),
	     .after = {std::string{Company::kBerthSystem}},
	     .run = [rules, &world](entt::registry &, double) { Seize(world, *rules); },
	     .before = {std::string{Trade::kObserve}}},
	    {.next = [rules](const SimWorld &target) { return SeizeNext(target, *rules); }});
}
} // namespace Manifest::Policy
