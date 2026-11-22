#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <string>
#include <string_view>

#include <jettison/sim/Controller.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/space/Cruise.h>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/content/Definitions.h>
#include <manifest/world/Identities.h>
#include <manifest/world/State.h>

namespace Jettison::Dock
{
class Docks;
}

namespace Jettison::Physics
{
class PhysicsWorld;
}

namespace Manifest::Travel
{
// A journey's burn (D-107): the share of its cruise's time the engines thrust, half at each end, in
// basis points, 100% thrusting to the midpoint; and its acceleration, in thousandths of standard
// gravity, from 0.1 g to the sustained acceleration of its hull's mode (D-090). Never seconds.
inline constexpr std::uint32_t kFullThrustBps = 10000;
inline constexpr std::uint32_t kMilliGPerG = 1000;
inline constexpr std::uint32_t kLeastAccelerationMilliG = 100;
inline constexpr double kStandardGravity = 9.80665;

// A player's command that sends their company's ship to a venue (D-086): a saved World::Journey on
// the ship, which manifest.travel drives through the pad let go of, the flight out of the station's
// reach, the cruise and the pad asked for at the destination, one leg a step, to the ship docked and
// its Ship::dockedAt the venue. It writes no engine state. Refused, writing nothing, for a ship or a
// venue the world does not have (UnknownTarget), a ship of another company (NotAuthorised), one whose
// journey is under way (Travelling), one docked at the venue already (AlreadyThere), and a company
// whose account lacks the venue's docking fee for the ship's fee class (World::Fees, Content::FeeClass)
// and the ship's World::Arrears together
// (InsufficientFunds), or a venue whose account cannot hold what it is paid (InvalidQuantity). A ship
// whose drive has a profile flies the burn (Profile), refused InvalidQuantity for a thrust not above 0
// or past kFullThrustBps or an acceleration outside its bounds, and InsufficientFuel for a leg its
// World::Fuel cannot fly (Burned, at the distance to the venue's station now and everything aboard);
// one with none, the power law, burns nothing. As it starts it pays the fee to the venue's account and
// the arrears to the account of the venue it leaves (D-097), and takes the leg's fuel from the tanks.
struct Travel
{
	std::string shipId;
	std::string venueId;
	std::uint32_t thrustBps{kFullThrustBps};
	std::uint32_t accelerationMilliG{kMilliGPerG};
};

inline constexpr std::string_view kTravel = "Manifest.Travel.Travel";
// Version 2 added the burn; a version-1 command is refused as one of no registered version is.
inline constexpr std::uint32_t kVersion = 2;

// The most a drive of the mode's profile can sustain, in thousandths of g, to the nearest.
std::uint32_t MostMilliG(const Jettison::Space::CruiseProfile &mode);

// The profile a journey flies at the burn on its hull's mode profile: the mode's own at its sustained
// acceleration, its boost kept; below it, that acceleration throughout; and a thrust below 100% as the
// profile's thrust share. The burn must be in bounds.
Jettison::Space::CruiseProfile Profile(
    const Jettison::Space::CruiseProfile &mode, std::uint32_t thrustBps,
    std::uint32_t accelerationMilliG);

// The fuel, in kilograms, a cruise of that many metres on the profile burns for that many tonnes
// aboard (D-108): each phase's speed change, its acceleration times its seconds at each end
// (Space::CruiseThrust), burns reaction mass by the rocket equation to first order and pellets by the
// tuning's rate times the acceleration's factor, 1 up to 1 g and its g to the curve's power above;
// each rounded up to the gram.
World::Fuel Burned(
    const Content::FuelTuning &tuning, const Jettison::Space::CruiseProfile &profile, double tonnes,
    double metres);

// Everything the ship carries as a leg starts, in tonnes: its hull's mass, its tanks' and the fuel in
// them, its occupants and its cargo, opened totes included, each unit at its item's density and pack
// factor over its units to the cubic metre.
double TonnesAboard(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity ship);

// The system, by the name the step's table gives it.
inline constexpr std::string_view kSystem = "manifest.travel";

// The payload, with Reflect, and the engine's movement types. Safe to call again.
void RegisterTypes();

// The engine controller a company's ships are commanded by: a player's, by the n of its "Company<n>",
// which is never handed out twice. Throws std::invalid_argument for an ID that is not a company's of
// that form, or an n past a slot.
Jettison::Sim::Controller ControllerOf(std::string_view companyId);

// The venue's station, or null for a venue the content does not have or that has none.
entt::entity StationOf(
    const entt::registry &registry, const Content::ContentSet &content, std::string_view venueId);

// The world's ships in space, as Register attaches them: what provisioning parks a new ship through.
class Fleet
{
  public:
	Fleet(
	    Jettison::Dock::Docks &docks, Jettison::Physics::PhysicsWorld &physics,
	    const Content::ContentSet &content)
	    : m_docks{docks}, m_physics{physics}, m_content{content}
	{
	}

	// The venue's station (Travel::StationOf).
	entt::entity StationOf(const entt::registry &registry, std::string_view venueId) const
	{
		return ::Manifest::Travel::StationOf(registry, m_content, venueId);
	}

	// The smallest free pad of the venue's station, not a display one, that the hull's dock hull fits,
	// the lowest numbered of those, as the docks give one (Dock::RequestPad); the hull a Manifest.Hull
	// by ID. Null for none.
	entt::entity FreePad(
	    const entt::registry &registry, std::string_view venueId, std::string_view hullId) const;

	// Makes the ship a body of its hull in space, of the hull's mass, with its cruise drive, commanded
	// by its company (ControllerOf), and parks it on the pad, docked. The pad is FreePad's for the
	// hull.
	void Park(
	    Jettison::Sim::SimWorld &world, entt::entity ship, entt::entity pad, std::string_view hullId,
	    std::string_view companyId) const;

	Jettison::Dock::Docks &Docks() const
	{
		return m_docks;
	}

	Jettison::Physics::PhysicsWorld &Physics() const
	{
		return m_physics;
	}

	const Content::ContentSet &Content() const
	{
		return m_content;
	}

  private:
	Jettison::Dock::Docks &m_docks;
	Jettison::Physics::PhysicsWorld &m_physics;
	const Content::ContentSet &m_content;
};

// Attaches Jettison's physics, its spatial index, steering, the docks, the lanes and cruising, as the
// step's table orders them; defines the content's pad classes, dock hulls, station layouts and the
// scenario's star systems in them; and registers the Travel command and the manifest.travel system,
// after dock and cruise. The world, the identities and the content must outlive the world's last
// step, and the returned fleet lives as long as the world. Throws std::invalid_argument for a content
// set with no scenario selected.
Fleet &Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);

// The scenario's stations, venue by venue in its order, each built to its layout on its body's rails.
// The initial state's; throws std::invalid_argument for a world that already has one.
void OpenStations(Jettison::Sim::SimWorld &world, const Fleet &fleet);
} // namespace Manifest::Travel
