#include <manifest/travel/Travel.h>

#include <algorithm>
#include <charconv>
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

#include <glm/geometric.hpp>

#include <jettison/ai/Steering.h>
#include <jettison/core/Assert.h>
#include <jettison/dock/Docks.h>
#include <jettison/physics/Components.h>
#include <jettison/physics/Flight.h>
#include <jettison/physics/PhysicsWorld.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/spatial/FrameIndices.h>
#include <jettison/transit/Profile.h>
#include <manifest/space/Cruise.h>
#include <manifest/space/Lanes.h>
#include <manifest/space/StarSystem.h>
#include <manifest/space/StarSystemFrame.h>
#include <manifest/space/Types.h>

#include <manifest/config/SimulationConfig.h>
#include <manifest/content/Content.h>
#include <manifest/content/Definitions.h>
#include <manifest/world/State.h>

namespace Manifest::Travel
{
namespace
{
using Command::Actor;
using Command::Result;
using Jettison::Sim::SimWorld;
using World::Failure;
using World::Kind;
using World::Leg;
namespace Ai = Jettison::Ai;
namespace Dock = Jettison::Dock;
namespace Physics = Jettison::Physics;
namespace Sim = Jettison::Sim;

constexpr std::string_view kCompanyPrefix = "Company";
constexpr double kKilogramsPerTonne = 1000.0;
constexpr double kGramsPerKilogram = 1000.0;
constexpr double kMilligramsPerKilogram = 1.0e6;
constexpr double kMetresPerKilometre = 1000.0;

// The fuel a cruise's start must leave aboard past what its settled course burns beyond the
// admission's, a gram, as a policy's estimate keeps.
constexpr double kFuelMarginKilograms = 0.001;

// What the command and the system run on.
struct Rules
{
	World::Identities &identities;
	const Content::ContentSet &content;
	Fleet fleet;
};


const Content::VenueDefinition *FindVenue(const Content::ContentSet &content, std::string_view venueId)
{
	const Content::ScenarioDefinition &scenario = content.scenarios.find(content.scenario)->second;
	const auto venue = std::ranges::find(scenario.venues, venueId, &Content::VenueDefinition::venueId);
	return venue == scenario.venues.end() ? nullptr : &*venue;
}


// The star system a ship is in, or null.
const Space::StarSystem *SystemOf(
    const entt::registry &registry, const Content::ContentSet &content, entt::entity ship)
{
	const auto *inSystem = registry.try_get<Sim::InFrame>(ship);
	if (!inSystem)
		return nullptr;
	const auto system = std::ranges::find(
	    content.systems, inSystem->frame,
	    [](const Space::StarSystem &each) { return Sim::FrameId(each.name); });
	return system == content.systems.end() ? nullptr : &*system;
}


// The world's entity of the kind and ID, which the world must hold.
entt::entity Required(const Rules &rules, Kind kind, const std::string &id)
{
	const entt::entity entity = rules.identities.Find(kind, id);
	JETTISON_VERIFY(entity != entt::null);
	return entity;
}


// Whether the drive crosses at either end, so that its cruise is settled as flown.
bool Crosses(const Space::CruiseDrive &drive)
{
	return drive.profile && drive.crossingAcceleration > 0.0f &&
	       (drive.departureClearance > 0.0 || drive.arrivalClearance > 0.0);
}


// The cruise to the venue's station settled from where the ship is now, as the cruise's start settles
// it (Space::SettleCruise), for a drive that crosses; none for one that does not, the result left
// Succeeded, and none for one refused, the result its reason.
std::optional<Space::SettledCruise> Settle(
    const SimWorld &world, const Space::StarSystem &system, const Space::CruiseDrive &drive,
    entt::entity ship, std::string_view venueId, Result &result)
{
	result = Result::Succeeded;
	if (!Crosses(drive))
		return std::nullopt;
	const entt::registry &registry = world.Registry();
	const auto *pFrame = registry.try_get<Physics::LocalFrame>(ship);
	Space::Settlement settlement = Space::SettleCruise(
	    system, drive, registry.get<Sim::Transform>(ship).position,
	    pFrame ? std::string_view{pFrame->body} : std::string_view{}, venueId, world.Clock().Now(),
	    world.FixedStep());
	if (settlement.cruise)
		return std::move(settlement.cruise);
	result = settlement.refusal == Space::Reason::InsideRadius ? Result::InsideRadius : Result::NoCourse;
	return std::nullopt;
}


Result Started(SimWorld &world, const Rules &rules, const Actor &actor, const Travel &travel)
{
	entt::registry &registry = world.Registry();
	const entt::entity ship = rules.identities.Find(Kind::Ship, travel.shipId);
	if (ship == entt::null)
		return Result::UnknownTarget;
	const World::Ship &record = registry.get<World::Ship>(ship);
	const entt::entity company = rules.identities.Find(Kind::Company, record.companyId);
	JETTISON_VERIFY(company != entt::null);
	if (registry.get<World::Company>(company).principalId != actor.id)
		return Result::NotAuthorised;
	if (rules.fleet.StationOf(registry, travel.venueId) == entt::null)
		return Result::UnknownTarget;
	if (const auto *journey = registry.try_get<World::Journey>(ship); journey && !journey->Ended())
		return Result::Travelling;
	if (record.dockedAt == travel.venueId)
		return Result::AlreadyThere;
	if (travel.thrustBps == 0 || travel.thrustBps > kFullThrustBps ||
	    travel.accelerationMilliG < kLeastAccelerationMilliG)
		return Result::InvalidQuantity;
	if (!Content::PadFits(rules.content, travel.venueId, record.hullId))
		return Result::NoPadFits;

	// The destination's docking fee for the ship's fee class and the ship's arrears where it is, paid
	// as it starts (D-097, D-100).
	const World::ClassFees *pFees = World::Find(
	    registry.get<World::Fees>(Required(rules, Kind::Market, travel.venueId)),
	    Content::FeeClass(rules.content, record.hullId));
	JETTISON_VERIFY(pFees != nullptr);
	const std::int64_t fee = pFees->docking;
	const auto *pArrears = registry.try_get<World::Arrears>(ship);
	const std::int64_t owed = pArrears ? pArrears->owed : 0;
	const entt::entity payer = Required(rules, Kind::CompanyAccount, record.companyId);
	if (const std::int64_t balance = registry.get<World::Account>(payer).balance;
	    balance < fee || balance - fee < owed)
		return Result::InsufficientFunds;

	// The burn and the leg's fuel, for a drive with a profile (D-107, D-108), refuelled first at the
	// venue it leaves, keeping back what it pays as it starts.
	auto *pDrive = registry.try_get<Space::CruiseDrive>(ship);
	std::optional<Jettison::Transit::MotionProfile> flown;
	Space::CruiseDrive drive;
	World::Fuel burned{};
	Result result = Result::Succeeded;
	if (pDrive && pDrive->profile)
	{
		const auto authored = rules.content.drives.find(Content::RegistryName(record.hullId));
		const Jettison::Transit::MotionProfile &mode =
		    authored != rules.content.drives.end() && authored->second.profile
		        ? *authored->second.profile
		        : *pDrive->profile;
		if (travel.accelerationMilliG > MostMilliG(mode))
			return Result::InvalidQuantity;
		if (!record.dockedAt.empty())
			rules.fleet.Refuel(world, ship, fee + owed);
		flown = Profile(mode, travel.thrustBps, travel.accelerationMilliG);
		const Content::DriveDefinition *pClass = Content::DriveOf(rules.content, record.hullId);
		JETTISON_VERIFY(pClass != nullptr);

		// The journey's drive: the burn, and the crossings and spheres of its two ends. A drive that
		// crosses is settled as flown from here, its fuel its course's own metres; any other burns
		// over the straight line to the station.
		drive = *pDrive;
		drive.profile = flown;
		const auto *pFrame = registry.try_get<Physics::LocalFrame>(ship);
		const Space::StarSystem *pSystem = SystemOf(registry, rules.content, ship);
		double driven = Space::DriveMetres(
		    *pDrive, glm::distance(
		                 registry.get<Sim::Transform>(ship).position,
		                 registry.get<Sim::Transform>(rules.fleet.StationOf(registry, travel.venueId))
		                     .position));
		if (pSystem && pFrame)
		{
			Exclusion exclusion =
			    ExclusionOf(rules.content, *pSystem, *pClass, pFrame->body, travel.venueId);
			drive.departureClearance = exclusion.departureClearance;
			drive.arrivalClearance = exclusion.arrivalClearance;
			drive.keepouts = std::move(exclusion.keepouts);
			if (const std::optional<Space::SettledCruise> settled =
			        Settle(world, *pSystem, drive, ship, travel.venueId, result))
				driven = settled->DriveMetres();
			else if (result != Result::Succeeded)
				return result;
		}
		burned = Burned(
		    *pClass, *flown, TonnesAboard(registry, rules.identities, rules.content, ship), driven,
		    Content::KilogramsPerUnit(Content::PelletItem(rules.content, *pClass)));
		const auto *pFuel = registry.try_get<World::Fuel>(ship);
		const World::Fuel tanks = pFuel ? *pFuel : World::Fuel{};
		if (tanks.reactionMass < burned.reactionMass || tanks.pellets < burned.pellets)
			return Result::InsufficientFuel;
	}

	World::Account &account = registry.get<World::Account>(payer);
	World::Account &destination =
	    registry.get<World::Account>(Required(rules, Kind::VenueAccount, travel.venueId));
	World::Account *pLeft =
	    owed > 0 ? &registry.get<World::Account>(Required(rules, Kind::VenueAccount, record.dockedAt))
	             : nullptr;
	constexpr std::int64_t kMaxCredits = std::numeric_limits<std::int64_t>::max();
	if (fee > kMaxCredits - destination.balance || (pLeft && owed > kMaxCredits - pLeft->balance))
		return Result::InvalidQuantity;
	account.balance -= fee + owed;
	destination.balance += fee;
	if (pLeft)
	{
		pLeft->balance += owed;
		registry.remove<World::Arrears>(ship);
	}
	if (flown)
	{
		*pDrive = std::move(drive);
		if (auto *pFuel = registry.try_get<World::Fuel>(ship))
		{
			pFuel->reactionMass -= burned.reactionMass;
			pFuel->pellets -= burned.pellets;
		}
		if (auto *pJob = registry.try_get<World::Job>(ship); pJob && pJob->totes > 0)
		{
			pJob->reactionMassKilograms += burned.reactionMass;
			pJob->pelletKilograms += burned.pellets;
		}
	}
	registry.emplace_or_replace<World::Journey>(
	    ship, travel.venueId, Leg::Departing, Failure::None, world.Clock().Step(), burned);
	return Result::Succeeded;
}


void Fail(World::Journey &journey, Failure failure)
{
	journey.leg = Leg::Failed;
	journey.failure = failure;
}


// The cruise of a drive that crosses settled again as it starts, from where the ship has cleared to,
// and the fuel its admission burnt settled against that course's own metres on the mass admission
// weighed, in whole units: more burnt from the fuel aboard, its job's share with it, and less returned
// to the tanks and magazine. False for a course refused, or fuel aboard short of more and a gram.
bool Resettled(SimWorld &world, const Rules &rules, entt::entity ship, World::Journey &journey)
{
	entt::registry &registry = world.Registry();
	const auto *pDrive = registry.try_get<Space::CruiseDrive>(ship);
	const Space::StarSystem *pSystem = SystemOf(registry, rules.content, ship);
	if (!pDrive || !pSystem)
		return true;
	Result result = Result::Succeeded;
	const std::optional<Space::SettledCruise> settled =
	    Settle(world, *pSystem, *pDrive, ship, journey.venueId, result);
	if (!settled)
		return result == Result::Succeeded;
	const Content::DriveDefinition *pClass =
	    Content::DriveOf(rules.content, registry.get<World::Ship>(ship).hullId);
	JETTISON_VERIFY(pClass != nullptr);
	const World::Fuel &paid = journey.paid;
	const World::Fuel burned = Burned(
	    *pClass, *pDrive->profile,
	    TonnesAboard(registry, rules.identities, rules.content, ship) +
	        ((paid.reactionMass + paid.pellets) / kKilogramsPerTonne),
	    settled->DriveMetres(), Content::KilogramsPerUnit(Content::PelletItem(rules.content, *pClass)));
	const World::Fuel more{burned.reactionMass - paid.reactionMass, burned.pellets - paid.pellets};
	auto *pFuel = registry.try_get<World::Fuel>(ship);
	const World::Fuel held = pFuel ? *pFuel : World::Fuel{};
	if ((more.reactionMass > 0.0 && held.reactionMass < more.reactionMass + kFuelMarginKilograms) ||
	    (more.pellets > 0.0 && held.pellets < more.pellets + kFuelMarginKilograms))
		return false;
	if (pFuel)
		*pFuel = {
		    std::max(0.0, held.reactionMass - more.reactionMass),
		    std::max(0.0, held.pellets - more.pellets)};
	if (auto *pJob = registry.try_get<World::Job>(ship); pJob && pJob->totes > 0)
	{
		pJob->reactionMassKilograms = std::max(0.0, pJob->reactionMassKilograms + more.reactionMass);
		pJob->pelletKilograms = std::max(0.0, pJob->pelletKilograms + more.pellets);
	}
	journey.paid = burned;
	return true;
}


// The pad asked for at the destination, its station flying the ship in.
void Ask(
    SimWorld &world, World::Journey &journey, entt::entity ship, entt::entity station,
    const Sim::Controller &sender)
{
	world.Apply(Dock::RequestBerth{sender, ship, station, true});
	const auto *docking = world.Registry().try_get<Dock::Docking>(ship);
	if (docking && docking->platform == station)
		journey.leg = Leg::Arriving;
	else
		Fail(journey, Failure::PadRefused);
}


// Whether the ship is where a cruise to the station would bring it, or nearer.
bool Near(
    const entt::registry &registry, const Space::StarSystem &system, entt::entity ship,
    entt::entity station)
{
	const int body = system.Find(registry.get<Physics::OnRails>(station).body);
	JETTISON_VERIFY(body >= 0);
	return glm::distance(
	           registry.get<Sim::Transform>(ship).position,
	           registry.get<Sim::Transform>(station).position) <
	       Space::CruiseTravel::ArrivalDistance(system.bodies[static_cast<std::size_t>(body)]) +
	           Space::CruiseTravel::kStationReach;
}


// One transition of the ship's journey, from the engine's state as the step's movement left it
// (docs/world.md, "Transaction boundaries"): the next engine command, applied now and acted on by the
// engine's systems from the next step, and the leg it leads to.
void Advance(SimWorld &world, const Rules &rules, entt::entity ship)
{
	entt::registry &registry = world.Registry();
	World::Journey &journey = registry.get<World::Journey>(ship);
	World::Ship &record = registry.get<World::Ship>(ship);
	const Sim::Controller sender = ControllerOf(record.companyId);
	const entt::entity station = rules.fleet.StationOf(registry, journey.venueId);
	JETTISON_VERIFY(station != entt::null);

	switch (journey.leg)
	{
		case Leg::Departing:
		{
			const auto *docking = registry.try_get<Dock::Docking>(ship);
			if (!docking)
				journey.leg = Leg::Clearing;
			else if (docking->stage != Dock::Stage::Leaving)
			{
				world.Apply(Dock::ReleaseBerth{sender, ship, true});
				docking = registry.try_get<Dock::Docking>(ship);
				if (docking && docking->stage != Dock::Stage::Leaving)
					Fail(journey, Failure::DepartureRefused);
				else
					record.dockedAt.clear();
			}
			return;
		}
		case Leg::Clearing:
		{
			const Space::StarSystem *system = SystemOf(registry, rules.content, ship);
			if (!system)
				return Fail(journey, Failure::CruiseRefused);
			if (Near(registry, *system, ship, station))
				return Ask(world, journey, ship, station, sender);

			// Within a station's reach: flown out, or still flying out, its flight planned again if it
			// lost its way. Steering keeps a destination it has reached, so it is out once it is past.
			const Content::ScenarioDefinition &scenario =
			    rules.content.scenarios.find(rules.content.scenario)->second;
			for (const Content::VenueDefinition &venue : scenario.venues)
				if (std::optional<Ai::Destination> flight = Space::ClearOf(
				        world, rules.fleet.Physics(), *system, ship,
				        rules.fleet.StationOf(registry, venue.venueId), journey.venueId))
				{
					if (registry.all_of<Ai::Destination>(ship))
						return;
					world.Apply(Ai::SetDestination{sender, ship, *std::move(flight)});
					if (!registry.all_of<Ai::Destination>(ship))
						Fail(journey, Failure::CruiseRefused);
					return;
				}
			if (!Resettled(world, rules, ship, journey))
				return Fail(journey, Failure::CruiseRefused);
			world.Apply(Space::Cruise{sender, ship, journey.venueId});
			if (registry.all_of<Space::Cruising>(ship))
				journey.leg = Leg::Cruising;
			else
				Fail(journey, Failure::CruiseRefused);
			return;
		}
		case Leg::Cruising:
			if (!registry.all_of<Space::Cruising>(ship))
				Ask(world, journey, ship, station, sender);
			return;
		case Leg::Arriving:
		{
			const auto *docking = registry.try_get<Dock::Docking>(ship);
			if (!docking || docking->platform != station)
				Fail(journey, Failure::PadLost);
			else if (docking->stage == Dock::Stage::Docked)
			{
				record.dockedAt = journey.venueId;
				journey.leg = Leg::Arrived;
			}
			return;
		}
		case Leg::Arrived:
		case Leg::Failed: return;
	}
}


// The first step a journey under way has to advance in (Jettison::Sim::Quiet): the step its cruise
// arrives in, as the cruise's system runs before this one; none while its ship waits on the docks;
// the next for any other leg.
std::uint64_t AdvanceNext(const SimWorld &world, const Rules &rules)
{
	const entt::registry &registry = world.Registry();
	const std::uint64_t now = world.Clock().Step();
	std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
	for (const auto [ship, journey] : registry.view<const World::Journey>().each())
	{
		if (journey.Ended())
			continue;
		if (journey.leg == Leg::Cruising)
		{
			const auto *cruising = registry.try_get<Space::Cruising>(ship);
			if (!cruising)
				return now;
			next = std::min(
			    next, cruising->arrivalStep - std::min<std::uint64_t>(cruising->arrivalStep, 1));
			continue;
		}
		const auto *docking = registry.try_get<Dock::Docking>(ship);
		if (journey.leg != Leg::Arriving || !docking ||
		    docking->platform != rules.fleet.StationOf(registry, journey.venueId) ||
		    docking->stage == Dock::Stage::Docked)
			return now;
	}
	return next;
}


// Every journey under way, one transition each, in entity order.
void AdvanceAll(SimWorld &world, const Rules &rules)
{
	std::vector<entt::entity> ships;
	for (const auto [ship, journey] : world.Registry().view<const World::Journey>().each())
		if (!journey.Ended())
			ships.push_back(ship);
	std::ranges::sort(ships, {}, [](entt::entity ship) { return entt::to_integral(ship); });
	for (const entt::entity ship : ships)
		Advance(world, rules, ship);
}
} // namespace


void RegisterTypes()
{
	static const bool registered = []
	{
		World::RegisterTypes();
		Command::RegisterTypes();
		Sim::RegisterTypes();
		Dock::RegisterTypes();
		Space::RegisterTypes();
		using namespace Jettison::Reflect;
		RegisterType<Travel>("Manifest.Travel.Travel")
		    .Field<&Travel::shipId>("shipId", Attribute::Saved)
		    .Field<&Travel::venueId>("venueId", Attribute::Saved)
		    .Field<&Travel::thrustBps>("thrustBps", Attribute::Saved)
		    .Field<&Travel::accelerationMilliG>("accelerationMilliG", Attribute::Saved);
		return true;
	}();
	(void)registered;
}


std::uint32_t MostMilliG(const Jettison::Transit::MotionProfile &mode)
{
	const double milliG = std::clamp(
	    static_cast<double>(mode.sustainedAcceleration) / kStandardGravity * kMilliGPerG, 0.0,
	    static_cast<double>(std::numeric_limits<std::uint32_t>::max()));
	return static_cast<std::uint32_t>(std::lround(milliG));
}


Jettison::Transit::MotionProfile Profile(
    const Jettison::Transit::MotionProfile &mode, std::uint32_t thrustBps,
    std::uint32_t accelerationMilliG)
{
	const auto acceleration = static_cast<float>(accelerationMilliG * kStandardGravity / kMilliGPerG);
	Jettison::Transit::MotionProfile profile =
	    accelerationMilliG >= MostMilliG(mode)
	        ? mode
	        : Jettison::Transit::MotionProfile{acceleration, 0.0f, acceleration, {}};
	profile.thrustShare =
	    thrustBps < kFullThrustBps
	        ? std::optional{static_cast<float>(thrustBps) / static_cast<float>(kFullThrustBps)}
	        : std::nullopt;
	return profile;
}


World::Fuel Burned(
    const Content::DriveDefinition &drive, const Jettison::Transit::MotionProfile &profile,
    double tonnes, double metres, double pelletKilogramsPerUnit)
{
	const Jettison::Transit::ThrustSeconds thrust = Jettison::Transit::Thrust(profile, metres);
	double kms = 0.0;
	double pelletKms = 0.0;
	for (const auto &[acceleration, seconds] :
	     {std::pair{static_cast<double>(profile.boostAcceleration), thrust.boost},
	      std::pair{static_cast<double>(profile.sustainedAcceleration), thrust.sustained}})
	{
		// Speeding up at one end and slowing down at the other.
		const double change = 2.0 * acceleration * seconds / kMetresPerKilometre;
		const double g = acceleration / kStandardGravity;
		double factor = 1.0;
		for (int power = 0; g > 1.0 && power < drive.curveExponent; ++power)
			factor *= g;
		kms += change;
		pelletKms += change * factor;
	}
	const auto grams = [](double kilograms)
	{ return std::ceil(kilograms * kGramsPerKilogram) / kGramsPerKilogram; };
	const double pellets =
	    grams(tonnes * pelletKms * drive.pelletMilligramsPerTonneKms / kMilligramsPerKilogram);
	return {
	    drive.exhaustKms ? grams(tonnes * kms * kKilogramsPerTonne / *drive.exhaustKms) : 0.0,
	    pelletKilogramsPerUnit > 1.0
	        ? std::ceil(pellets / pelletKilogramsPerUnit) * pelletKilogramsPerUnit
	        : pellets};
}


Exclusion ExclusionOf(
    const Content::ContentSet &content, const Space::StarSystem &system,
    const Content::DriveDefinition &drive, std::string_view from, std::string_view to)
{
	const double reach = Space::CruiseTravel::kStationReach;
	if (!drive.bodyExclusionRadius && !(drive.exclusionRadius > reach))
		return {};
	const Content::ScenarioDefinition &scenario = content.scenarios.find(content.scenario)->second;
	const auto populated = [&](const Space::Body &body)
	{
		return (body.kind == Space::BodyKind::Planet || body.kind == Space::BodyKind::Moon) &&
		       std::ranges::any_of(
		           scenario.regions, [&](const Content::RegionDefinition &region)
		           { return region.planetId == body.name && region.population > 0.0; });
	};
	const auto farthest = [](const Space::Body &body)
	{ return body.orbit.semiMajorAxis * (1.0 + body.orbit.eccentricity); };

	Exclusion exclusion;
	const auto keep = [&](const std::string &body, double radius)
	{
		if (std::ranges::find(exclusion.keepouts, body, &Space::Keepout::body) == exclusion.keepouts.end())
			exclusion.keepouts.push_back({body, radius});
	};

	// Each end's body, the ball about it and its spheres, and the clearance out of the ball.
	const auto end = [&](std::string_view name)
	{
		int at = system.Find(name);
		if (at < 0)
			return 0.0;
		const Space::Body &station = system.bodies[static_cast<std::size_t>(at)];
		const bool orbiting = station.kind == Space::BodyKind::Station && station.parentIndex >= 0;
		if (orbiting)
			at = station.parentIndex;
		const Space::Body &body = system.bodies[static_cast<std::size_t>(at)];
		double ball = 0.0;
		if (drive.bodyExclusionRadius && populated(body))
		{
			ball = body.radius + *drive.bodyExclusionRadius;
			keep(body.name, ball);
		}
		for (const Space::Body &other : system.bodies)
			if (other.kind == Space::BodyKind::Station && other.parentIndex == at)
			{
				ball = std::max(ball, farthest(other) + drive.exclusionRadius);
				keep(other.name, drive.exclusionRadius);
			}
		if (!drive.bodyExclusionRadius)
			return drive.exclusionRadius - reach;
		return ball + (orbiting ? farthest(station) : 0.0);
	};
	exclusion.departureClearance = end(from);
	exclusion.arrivalClearance = end(to);
	return exclusion;
}


double TonnesAboard(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity ship)
{
	const World::Ship &record = registry.get<World::Ship>(ship);
	const Content::FuelTuning &tuning = content.tuning.fuel;
	const auto hull = std::ranges::find(content.hulls, record.hullId, &Core::ShipClassSpec::hullId);
	JETTISON_VERIFY(hull != content.hulls.end());
	double kilograms =
	    hull->mass.value_or(0.0) + (hull->occupants.value_or(0) * tuning.occupantKilograms);
	if (const auto *pFuel = registry.try_get<World::Fuel>(ship))
		kilograms += pFuel->reactionMass + pFuel->pellets;
	if (const std::optional<World::Fuel> tanks =
	        identities.FuelTanks(record, registry.try_get<World::Systems>(ship)))
		kilograms += tanks->reactionMass * tuning.tankTonnesPerTonne;
	const auto item = [&content](std::string_view itemId) -> const Core::ItemNode &
	{
		const auto found = std::ranges::find(content.gameMode.items, itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(found != content.gameMode.items.end());
		return *found;
	};

	// A cubic metre of the item as packed, in kilograms: its density, water's 1 a tonne to it.
	const auto packed = [](const Core::ItemNode &each)
	{ return each.density * each.packFactor * kKilogramsPerTonne; };
	if (const auto *pCargo = registry.try_get<World::Cargo>(ship))
	{
		for (const World::Lot &lot : pCargo->lots)
			kilograms += static_cast<double>(lot.totes) * packed(item(lot.itemId)) /
			             static_cast<double>(Core::kTotesPerIbc);
		for (const World::Opened &opened : pCargo->opened)
		{
			const Core::ItemNode &each = item(opened.itemId);
			kilograms += opened.quantity * packed(each) / each.unitsPerCubicMetre;
		}
	}
	return kilograms / kKilogramsPerTonne;
}


Sim::Controller ControllerOf(std::string_view companyId)
{
	std::uint32_t slot = 0;
	const std::string_view number =
	    companyId.starts_with(kCompanyPrefix) ? companyId.substr(kCompanyPrefix.size()) : "";
	const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), slot);
	if (number.empty() || error != std::errc{} || end != number.data() + number.size() || slot == 0)
		throw std::invalid_argument{
		    std::format("'{}' is not a company's ID, Company<n> with n a slot", companyId)};
	return {Sim::ControllerKind::Player, slot};
}


entt::entity StationOf(
    const entt::registry &registry, const Content::ContentSet &content, std::string_view venueId)
{
	const Content::VenueDefinition *pVenue = FindVenue(content, venueId);
	if (!pVenue)
		return entt::null;
	const std::uint64_t frame = Sim::FrameId(pVenue->systemId);
	for (const auto [station, held, rails, inSystem] :
	     registry.view<const Dock::Platform, const Physics::OnRails, const Sim::InFrame>().each())
		if (inSystem.frame == frame && rails.body == venueId)
			return station;
	return entt::null;
}


entt::entity Fleet::FreePad(
    const entt::registry &registry, std::string_view venueId, std::string_view hullId) const
{
	const entt::entity station = StationOf(registry, venueId);
	const Dock::VehicleDefinition *pHull = m_docks.FindVehicle(Content::RegistryName(hullId));
	if (station == entt::null || !pHull)
		return entt::null;
	entt::entity found = entt::null;
	for (const entt::entity pad : registry.get<Dock::Platform>(station).berths)
		if (const Dock::Berth &held = registry.get<Dock::Berth>(pad);
		    held.state == Dock::BerthState::Free && !held.display &&
		    m_docks.Fits(*pHull, held.berthClass) &&
		    (found == entt::null ||
		     m_docks.FindBerthClass(held.berthClass)->rank <
		         m_docks.FindBerthClass(registry.get<Dock::Berth>(found).berthClass)->rank))
			found = pad;
	return found;
}


void Fleet::Park(
    SimWorld &world, entt::entity ship, entt::entity pad, std::string_view hullId,
    std::string_view companyId) const
{
	entt::registry &registry = world.Registry();
	const entt::entity station = registry.get<Dock::Berth>(pad).platform;
	const std::string hull = Content::RegistryName(hullId);

	// Content holds a scenario's starter hull to having a mass.
	const auto spec = std::ranges::find(m_content.hulls, hullId, &Core::ShipClassSpec::hullId);
	JETTISON_VERIFY(spec != m_content.hulls.end() && spec->mass);
	registry.emplace<Sim::Transform>(ship, registry.get<Sim::Transform>(station));
	registry.emplace<Sim::InFrame>(ship, registry.get<Sim::InFrame>(station).frame);
	registry.emplace<Physics::LocalFrame>(ship, registry.get<Physics::OnRails>(station).body);
	Physics::Body body{
	    Physics::BodyKind::Dynamic, Dock::Docks::VehicleShape(hull), static_cast<float>(*spec->mass)};
	body.continuous = true;
	registry.emplace<Physics::Body>(ship, body);
	registry.emplace<Physics::Velocity>(ship);
	registry.emplace<Physics::Handling>(ship);
	registry.emplace<Physics::FlightControls>(ship, Physics::FlightControls{{}, {}, true, station});
	registry.emplace<Dock::Vehicle>(ship, hull);
	registry.emplace<Space::CruiseDrive>(ship, m_content.drives.at(hull));
	registry.emplace<Ai::Steered>(ship);
	registry.emplace<Sim::ControlledBy>(ship, ControllerOf(companyId));
	m_docks.Park(world, ship, pad);
}


Fleet &Register(
    SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content)
{
	if (!content.scenarios.contains(content.scenario))
		throw std::invalid_argument{"the content set has no scenario selected"};
	RegisterTypes();
	world.RegisterComponent<Sim::Transform>();
	world.RegisterComponent<Sim::InFrame>();
	world.RegisterComponent<Sim::ControlledBy>();
	Physics::PhysicsWorld &physics = Physics::PhysicsWorld::Attach(world);
	Ai::Steering::Attach(world, physics, Jettison::Spatial::FrameIndices::Attach(world));
	Dock::Docks &docks = Dock::Docks::Attach(world, physics);
	Space::LaneTravel &lanes = Space::LaneTravel::Attach(world, physics);
	Space::CruiseTravel &cruises = Space::CruiseTravel::Attach(world, physics);
	for (const auto &[name, padClass] : content.padClasses)
		docks.DefineBerthClass(name, padClass);
	for (const auto &[name, hull] : content.dockHulls)
		docks.DefineVehicle(name, hull);
	for (const auto &[name, layout] : content.layouts)
		docks.DefineLayout(name, layout);
	for (const Space::StarSystem &system : content.systems)
	{
		Space::DefineStarSystem(physics, system);
		lanes.DefineStarSystem(system);
		cruises.DefineStarSystem(system);
	}

	auto rules = std::make_shared<Rules>(identities, content, Fleet{docks, physics, content});
	requests.Register<Travel>(
	    std::string{kTravel}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Travel &travel)
	    { return Started(target, *rules, actor, travel); });
	using Jettison::Ecs::Access;
	world.RegisterSystem(
	    {.name = std::string{kSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     // What its engine commands write, besides its own.
	     .access =
	         Access::Of<
	             World::Journey, World::Ship, Dock::Docking, Space::Cruising, Space::CruiseDrive,
	             Ai::Destination, World::Cargo, World::Systems>()
	             .Writing<
	                 World::Journey, World::Ship, World::Fuel, World::Job, Dock::Docking, Dock::Berth,
	                 Dock::Platform,
	                 Ai::Destination, Space::Cruising, Sim::Transform, Physics::Body, Physics::Velocity,
	                 Physics::LocalFrame, Physics::OnRails>(),
	     .after =
	         {std::string{Dock::Docks::kSystemName}, std::string{Space::CruiseTravel::kSystemName}},
	     .run = [rules, &world](entt::registry &, double) { AdvanceAll(world, *rules); }},
	    {.next = [rules](const SimWorld &target) { return AdvanceNext(target, *rules); }});
	return rules->fleet;
}


void OpenStations(SimWorld &world, const Fleet &fleet)
{
	const Content::ContentSet &content = fleet.Content();
	const Content::ScenarioDefinition &scenario = content.scenarios.find(content.scenario)->second;
	for (const Content::VenueDefinition &venue : scenario.venues)
	{
		if (fleet.StationOf(world.Registry(), venue.venueId) != entt::null)
			throw std::invalid_argument{std::format("the world already has '{}'", venue.venueId)};
		fleet.Docks().MakePlatform(world, Sim::FrameId(venue.systemId), venue.venueId, venue.layout);
	}
}
} // namespace Manifest::Travel
