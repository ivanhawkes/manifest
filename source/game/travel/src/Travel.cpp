#include <manifest/travel/Travel.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include <jettison/ai/Steering.h>
#include <jettison/celestial/StarSystem.h>
#include <jettison/core/Assert.h>
#include <jettison/dock/Docks.h>
#include <jettison/physics/Components.h>
#include <jettison/physics/Flight.h>
#include <jettison/physics/PhysicsWorld.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/space/Cruise.h>
#include <jettison/space/Lanes.h>
#include <jettison/space/StarSystemFrame.h>
#include <jettison/space/Types.h>
#include <jettison/spatial/SystemIndices.h>

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
namespace Celestial = Jettison::Celestial;
namespace Dock = Jettison::Dock;
namespace Physics = Jettison::Physics;
namespace Sim = Jettison::Sim;
namespace Space = Jettison::Space;

constexpr std::string_view kCompanyPrefix = "Company";

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
const Celestial::StarSystem *SystemOf(
    const entt::registry &registry, const Content::ContentSet &content, entt::entity ship)
{
	const auto *inSystem = registry.try_get<Sim::InFrame>(ship);
	if (!inSystem)
		return nullptr;
	const auto system = std::ranges::find(
	    content.systems, inSystem->frame,
	    [](const Celestial::StarSystem &each) { return Sim::FrameId(each.name); });
	return system == content.systems.end() ? nullptr : &*system;
}


// The world's entity of the kind and ID, which the world must hold.
entt::entity Required(const Rules &rules, Kind kind, const std::string &id)
{
	const entt::entity entity = rules.identities.Find(kind, id);
	JETTISON_VERIFY(entity != entt::null);
	return entity;
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

	// The destination's docking fee for the ship's fee class and the ship's arrears where it is, paid
	// as it starts (D-097, D-100).
	const World::ClassFees *pFees = World::Find(
	    registry.get<World::Fees>(Required(rules, Kind::Market, travel.venueId)),
	    Content::FeeClass(rules.content, record.hullId));
	JETTISON_VERIFY(pFees != nullptr);
	const std::int64_t fee = pFees->docking;
	const auto *pArrears = registry.try_get<World::Arrears>(ship);
	const std::int64_t owed = pArrears ? pArrears->owed : 0;
	World::Account &account =
	    registry.get<World::Account>(Required(rules, Kind::CompanyAccount, record.companyId));
	if (account.balance < fee || account.balance - fee < owed)
		return Result::InsufficientFunds;
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
	registry.emplace_or_replace<World::Journey>(
	    ship, travel.venueId, Leg::Departing, Failure::None, world.Clock().Step());
	return Result::Succeeded;
}


void Fail(World::Journey &journey, Failure failure)
{
	journey.leg = Leg::Failed;
	journey.failure = failure;
}


// The pad asked for at the destination, its station flying the ship in.
void Ask(
    SimWorld &world, World::Journey &journey, entt::entity ship, entt::entity station,
    const Sim::Controller &sender)
{
	world.Apply(Dock::RequestPad{sender, ship, station, true});
	const auto *docking = world.Registry().try_get<Dock::Docking>(ship);
	if (docking && docking->station == station)
		journey.leg = Leg::Arriving;
	else
		Fail(journey, Failure::PadRefused);
}


// Whether the ship is where a cruise to the station would bring it, or nearer.
bool Near(
    const entt::registry &registry, const Celestial::StarSystem &system, entt::entity ship,
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
// (docs/stage-4-trading-plan.md, "Transaction boundaries"): the next engine command, applied now and
// acted on by the engine's systems from the next step, and the leg it leads to.
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
				world.Apply(Dock::ReleasePad{sender, ship, true});
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
			const Celestial::StarSystem *system = SystemOf(registry, rules.content, ship);
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
			if (!docking || docking->station != station)
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
		    docking->station != rules.fleet.StationOf(registry, journey.venueId) ||
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
		    .Field<&Travel::venueId>("venueId", Attribute::Saved);
		return true;
	}();
	(void)registered;
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


entt::entity Fleet::StationOf(const entt::registry &registry, std::string_view venueId) const
{
	const Content::VenueDefinition *pVenue = FindVenue(m_content, venueId);
	if (!pVenue)
		return entt::null;
	const std::uint64_t frame = Sim::FrameId(pVenue->systemId);
	for (const auto [station, held, rails, inSystem] :
	     registry.view<const Dock::Station, const Physics::OnRails, const Sim::InFrame>().each())
		if (inSystem.frame == frame && rails.body == venueId)
			return station;
	return entt::null;
}


entt::entity Fleet::FreePad(
    const entt::registry &registry, std::string_view venueId, std::string_view hullId) const
{
	const entt::entity station = StationOf(registry, venueId);
	const Dock::HullDefinition *pHull = m_docks.FindHull(Content::RegistryName(hullId));
	if (station == entt::null || !pHull)
		return entt::null;
	entt::entity found = entt::null;
	for (const entt::entity pad : registry.get<Dock::Station>(station).pads)
		if (const Dock::Pad &held = registry.get<Dock::Pad>(pad);
		    held.state == Dock::PadState::Free && !held.display &&
		    m_docks.Fits(*pHull, held.padClass) &&
		    (found == entt::null ||
		     m_docks.FindPadClass(held.padClass)->rank <
		         m_docks.FindPadClass(registry.get<Dock::Pad>(found).padClass)->rank))
			found = pad;
	return found;
}


void Fleet::Park(
    SimWorld &world, entt::entity ship, entt::entity pad, std::string_view hullId,
    std::string_view companyId) const
{
	entt::registry &registry = world.Registry();
	const entt::entity station = registry.get<Dock::Pad>(pad).station;
	const std::string hull = Content::RegistryName(hullId);
	// Content holds a scenario's starter hull to having a mass.
	const auto spec = std::ranges::find(m_content.hulls, hullId, &Core::ShipClassSpec::hullId);
	JETTISON_VERIFY(spec != m_content.hulls.end() && spec->mass);
	registry.emplace<Sim::Transform>(ship, registry.get<Sim::Transform>(station));
	registry.emplace<Sim::InFrame>(ship, registry.get<Sim::InFrame>(station).frame);
	registry.emplace<Physics::LocalFrame>(ship, registry.get<Physics::OnRails>(station).body);
	Physics::Body body{
	    Physics::BodyKind::Dynamic, Dock::Docks::HullShape(hull), static_cast<float>(*spec->mass)};
	body.continuous = true;
	registry.emplace<Physics::Body>(ship, body);
	registry.emplace<Physics::Velocity>(ship);
	registry.emplace<Physics::Handling>(ship);
	registry.emplace<Physics::FlightControls>(ship, Physics::FlightControls{{}, {}, true, station});
	registry.emplace<Dock::Hull>(ship, hull);
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
	Ai::Steering::Attach(world, physics, Jettison::Spatial::SystemIndices::Attach(world));
	Dock::Docks &docks = Dock::Docks::Attach(world, physics);
	Space::LaneTravel &lanes = Space::LaneTravel::Attach(world, physics);
	Space::CruiseTravel &cruises = Space::CruiseTravel::Attach(world, physics);
	for (const auto &[name, padClass] : content.padClasses)
		docks.DefinePadClass(name, padClass);
	for (const auto &[name, hull] : content.dockHulls)
		docks.DefineHull(name, hull);
	for (const auto &[name, layout] : content.layouts)
		docks.DefineLayout(name, layout);
	for (const Celestial::StarSystem &system : content.systems)
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
	         Access::Of<World::Journey, World::Ship, Dock::Docking, Space::Cruising, Ai::Destination>()
	             .Writing<
	                 World::Journey, World::Ship, Dock::Docking, Dock::Pad, Dock::Station,
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
		fleet.Docks().MakeStation(world, Sim::FrameId(venue.systemId), venue.venueId, venue.layout);
	}
}
} // namespace Manifest::Travel
