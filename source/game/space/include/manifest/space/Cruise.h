#pragma once

#include <entt/entt.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/ai/Steering.h>
#include <jettison/physics/Components.h>
#include <jettison/sim/Controller.h>
#include <jettison/sim/Reason.h>
#include <jettison/transit/Profile.h>
#include <manifest/space/StarSystem.h>
#include <manifest/space/Types.h>

namespace Jettison::Physics
{
class PhysicsWorld;
}

namespace Jettison::Sim
{
class SimWorld;
}

namespace Manifest::Space
{
// A sphere a drive may not thrust within (D-108's exclusion radius): that many metres about a body of
// the ship's system, a station or a populated planet. Registered with Reflect as "Space.Keepout",
// every field Saved and Moddable, by RegisterTypes.
struct Keepout
{
	std::string body;
	double radius{0.0};

	bool operator==(const Keepout &) const = default;
};

// The cruise (docs/space.md, "Cruising": a ship crosses a system by cruising): a ship taken across its
// system to a body, out of physics and its frame as on a lane but staying in its system, placed each
// step along its course, and brought out at rest a few kilometres off the body, in its frame. The
// bodies are on rails, so where the target will be on arrival is solved exactly rather than chased.

// How long a ship's cruises take: the seconds a cruise of the reference distance takes, rising with
// the distance to the exponent, slower than it, so one planet's stations are about a minute apart
// and two planets a few; or, where it has a profile (Transit::MotionProfile), the profile's time,
// the power law unused. The game puts one on each ship that cruises, from its own data (Manifest's
// drives, by hull). Registered with Reflect as "Space.CruiseDrive", every field Saved and Moddable, by
// RegisterTypes; a drive with no profile saves as one did before profiles.
struct CruiseDrive
{
	float seconds{45.0f};

	// In metres.
	double reference{1.0e8};
	float exponent{0.25f};
	std::optional<Jettison::Transit::MotionProfile> profile;

	// With a profile, the metres at the start and at the end of a course its drive may not thrust over,
	// which the ship crosses on its thrusters from rest to rest at the crossing acceleration, in m/s²
	// (D-108's exclusion radius, beyond a station's reach); none crosses nothing. A game writes each
	// journey's as it writes the profile it flies.
	double departureClearance{0.0};
	double arrivalClearance{0.0};
	float crossingAcceleration{0.0f};

	// The spheres a drive with a crossing may not thrust within, which its cruise is checked against as
	// flown (SettleCruise); a game writes each journey's with its clearances.
	std::vector<Keepout> keepouts{};

	bool operator==(const CruiseDrive &) const = default;
};

// The seconds a cruise of that many metres takes on the drive: seconds × (metres / reference) to the
// exponent, or the profile's time with its clearances crossed at their ends, at rest at both ends.
// None for no distance.
double CruiseSeconds(const CruiseDrive &drive, double metres);

// The metres of a cruise of that many the drive itself thrusts over: all but its clearance at each end,
// at most half the cruise each.
double DriveMetres(const CruiseDrive &drive, double metres);

// Checks a drive a game defines: its seconds and reference positive and finite, and its exponent
// above 0 and at most 1, so its time rises with the distance and no faster than it; its profile, if it
// has one, as Transit::CheckProfile does; and its clearances finite and not below 0, with a crossing
// acceleration above 0 where either is above 0, and its keepouts' radii finite and above 0. Throws
// Jettison::Serialise::FormatError naming the drive.
void CheckCruiseDrive(std::string_view name, const CruiseDrive &drive);

// What a cruise is aimed at (SettleCruise): a body of the ship's system, by name; or, with no body, a
// ship, where it is in the system's axes as the cruise sets out, the body whose local frame it is in
// (Jettison::Physics::LocalFrame; none for the system's own), and its velocity relative to that
// frame, in m/s, which the course is plotted to meet as though it held it.
struct CruiseTarget
{
	std::string body;
	glm::dvec3 place{0.0};
	std::string frame;
	glm::dvec3 velocity{0.0};
};

// A ship cruising: its Jettison::Sim::InFrame stays its system, and it has no Jettison::Physics::Body,
// Velocity or LocalFrame from the step it set out in to the step it arrives in, so physics, steering,
// the docks and the lanes leave it alone; its Transform is where its course puts it at each step's end.
// Registered with Reflect, every field Saved, by RegisterTypes, and shown in snapshots.
struct Cruising
{
	// The body it is cruising to, and the frame it arrives in (Jettison::Physics::LocalFrame): the
	// body's own, or for a jump point that keeps station by a station, the station's, as a lane's
	// arrival is. For a cruise to a ship, no body, and the frame the ship was in as it set out: it
	// arrives in the frame the ship is in then.
	std::string target;
	std::string frame;

	// The ship it is cruising to, or none for a body. Its course's end is aimed again at where the
	// ship is each step (CruiseTravel::kShipStandoff off it), and the cruise is broken off where it
	// is should the ship leave the system or the world.
	entt::entity targetShip{entt::null};

	// The body the course moves with, the nearest both the frame it left and the target orbit (the
	// planet between two of its stations, the star between two planets), or none for the system's
	// own frame.
	std::string carrier;

	// From where it set out to where it arrives, both included, with the turns round the bodies in
	// the way between (SettleCruise); metres from the carrier, in the system's axes.
	std::vector<glm::dvec3> course;

	// The clock's step counts as it set out and once it has arrived (Jettison::Time::SimClock::Step):
	// it arrives as the step before the second ends.
	std::uint64_t departureStep{0};
	std::uint64_t arrivalStep{0};

	// Its body, which it has back on arrival.
	Jettison::Physics::Body body;

	// Its drive's profile as it set out, which it covers its course by, or none to ease along it; and
	// with a profile, its clearances as its cruise settled them (SettleCruise) and its crossing
	// acceleration, the ends it crosses on thrusters.
	std::optional<Jettison::Transit::MotionProfile> profile;
	double departureClearance{0.0};
	double arrivalClearance{0.0};
	float crossingAcceleration{0.0f};

	// The velocities it set out with and arrives with, relative to the carrier, in m/s: the frame's it
	// left and its own, and the frame's it arrives in (a ship's, its own too). On a profile it leaves
	// and arrives moving so, each velocity's speed along the course met by the profile where it can
	// and the rest carried off the course by a drift (SettleCruise); eased, it leaves and arrives at
	// rest relative to the carrier.
	glm::dvec3 departureVelocity{0.0};
	glm::dvec3 arrivalVelocity{0.0};

	bool operator==(const Cruising &) const = default;
};

// The command that sends a ship on a cruise to a body of its system, or to another ship in it
// (targetShip, which the body is then not looked at for), applied as the step begins. Refused, with a
// CruiseRefused, for a ship its sender does not command (Jettison::Sim::MayCommand), one not in a
// defined system (on a lane, or in none), one already cruising, one that is not a dynamic body with a
// CruiseDrive, or on rails (clamped to a pad), a body its system does not have, a target ship that is
// the ship itself or not placed in its system (NoSuchTarget), a ship
// within a station's reach (CruiseTravel::kStationReach), so docking stays in physics, a ship already
// within reach of where it would arrive, a course every way is blocked across, and one whose drive
// still thrusts within a keepout once it is settled (SettleCruise; InsideRadius). A ship sent
// gives up its Destination, which a cruise replaces, and with it what steering last demanded of its
// controls, as Jettison::Ai::ClearDestination does, so it comes out still; a pilot's controls are kept.
// Registered with Reflect, every field Saved, by RegisterTypes.
struct Cruise
{
	Jettison::Sim::Controller sender;
	entt::entity ship{entt::null};
	std::string body;
	entt::entity targetShip{entt::null};
};

// The command that breaks a ship's cruise off where it is, as a privateer's would be, applied as the
// step begins: the ship comes out at its place, its body back, in its carrier's local frame (the
// system's own for none), moving as its cruise had it over the last step, which physics then holds to
// its speed limit (Jettison::Physics::PhysicsSettings::maxLinearVelocity); a CruiseBrokenOff is raised.
// Refused, with a CruiseRefused, for a ship its sender does not command, or one not cruising
// (NotCruising). Registered with Reflect, every field Saved, by RegisterTypes.
struct BreakOffCruise
{
	Jettison::Sim::Controller sender;
	entt::entity ship{entt::null};
};

// A ship that came out of a cruise at rest (L37's moving off): the local frame it came out in (none
// for the system's own), and its place relative to that frame's origin then. While it is still there,
// steered nowhere and clamped to nothing, an arrival that comes out where it is makes it move off: it
// is given a Jettison::Ai::Destination CruiseTravel::kMoveOffDistance further from the arrival, and
// loses this. Registered with Reflect, every field Saved, by RegisterTypes.
struct CameOut
{
	std::string frame;
	glm::dvec3 offset{0.0};

	bool operator==(const CameOut &) const = default;
};

// Raised as a ship sets out on a cruise, with how long it takes and how far its course is; as it
// arrives; and as a cruise is refused. Registered with Reflect, every field Saved, by RegisterTypes.
struct CruiseStarted
{
	entt::entity ship{entt::null};
	std::string body;
	double seconds{0.0};
	double metres{0.0};
	std::uint64_t arrivalStep{0};
};
struct CruiseEnded
{
	entt::entity ship{entt::null};
	std::string body;
};
struct CruiseRefused
{
	entt::entity ship{entt::null};
	std::string body;
	Jettison::Sim::Reason reason;
};

// Raised as a cruise is broken off (BreakOffCruise, or its target ship lost), with the body it was
// cruising to, none for a ship. Registered with Reflect, every field Saved, by RegisterTypes.
struct CruiseBrokenOff
{
	entt::entity ship{entt::null};
	std::string body;
};

// Metres a flight out of a station's reach keeps from its sphere, and goes past the reach.
inline constexpr double kReachClearance = 100.0;
inline constexpr double kReachMargin = 300.0;

// The flight a ship takes out of the reach of the station it left, where a Cruise is refused
// (NearStation), so that a Cruise to the body is then accepted from beyond it: to kReachMargin beyond
// the reach (CruiseTravel::kStationReach) on the side of the body, round the station's sphere (its
// Jettison::Physics::Body's shape) by kReachClearance, in the ship's frame
// (Jettison::Physics::LocalFrame), for an Jettison::Ai::SetDestination. The station is an entity on a
// body's rails (Jettison::Physics::OnRails), as the docks build one, and the system the ship's. None
// for a ship outside the station's reach, and for one it cannot plan from: in another system or in no
// frame, or with the station or the body not of the system.
std::optional<Jettison::Ai::Destination> ClearOf(
    const Jettison::Sim::SimWorld &world, const Jettison::Physics::PhysicsWorld &physics,
    const Space::StarSystem &system, entt::entity ship, entt::entity station, std::string_view body);

// Cruising in a simulation world: the CruiseDrive and Cruising components, the Cruise command, and
// the system "cruise" (kSystemName), which runs after the lanes' (after physics') and places each
// cruising ship where its course has it as the step ends:
//   - the course is solved as the command is applied (SettleCruise), from where the ship is and the
//     time its drive takes over the course: the target's place at the arrival step, a course to the
//     point its ArrivalDistance (a ship's kShipStandoff) from it, on the side the course comes in
//     from, round the stars, planets and moons in the way (Waypoints, kCourseClearance), each turn
//     bent round (kBendPoints), its length giving the time again, in kSolveIterations; the time is
//     whole steps, at least one;
//   - a drive with a profile is given the fewest whole steps found, from that solve's, in which it
//     covers the course they give, so no step of it accelerates past the boost;
//   - along the course it eases out of its start and into its end (an in-out cubic of the steps
//     run), or on a profile covers it as the profile does (Transit::Covered) from the speed along
//     it it leaves with to the speed it arrives with, slowed to the whole steps, and drifts off it by
//     the rest of those velocities (Transit::Drift); carried with the carrier on its orbit, and
//     faces along the leg it is on; a cruise to a ship has its end aimed at where the ship is;
//   - in the step that ends its cruise it is at the course's end, or if that is not clear of every
//     body already there and of what arrived before it in the step, at the first place clear on
//     rings about it, as a lane's arrivals are (LaneTravel::kArrivalClearance); a ship that came
//     out at rest where it arrives is made to move off (CameOut); facing the target, at rest in the
//     frame it arrives in (moving as a target ship is in its frame), with its body back, which
//     physics makes in the next step, and its Jettison::Physics::FlightControls taken against
//     nothing; it is marked CameOut, and a CruiseEnded is raised.
// Ships are placed in entity order. A cruise to a ship is never passed over (Jettison::Sim::Quiet):
// where its target is is read each step.
//
// The star systems are definitions, the game's start-up as physics' are, and not state: a save names
// bodies by name, and a world it is loaded into is given the same systems.
//
// The world holds it from Attach on, through the systems and commands it registers.
class CruiseTravel
{
  public:
	static constexpr std::string_view kSystemName = "cruise";

	// Metres from a station's radius within which a cruise does not engage.
	static constexpr double kStationReach = 2000.0;

	// The acceleration a drive's clearance is crossed at, in m/s²: the reverse thrust every ship is
	// given, Jettison::Physics::Handling's, which Cruise.cpp holds it to.
	static constexpr float kCrossingAcceleration = 15.0f;

	// Where a cruise arrives: this many metres beyond the target's radius and a quarter of it, so a
	// ship is a few kilometres off a station or a gate and clear of a planet's air; and no nearer a
	// body with gravity than where it pulls at kArrivalGravity. A station's or a gate's frame falls
	// round its planet with the ship, but a planet's own pull is the ship's to hold against: at 1.25
	// radii Corva pulls at 16.6 m/s², past a skiff's 15 m/s² of reverse thrust, where a fifteenth of
	// that leaves its assist all it needs (Ostra's is 20,000 km out, Corva's 356,000 km).
	static constexpr double kArrivalStandoff = 4000.0;
	static constexpr double kArrivalRadiusShare = 1.25;
	static constexpr double kArrivalGravity = 1.0;

	// Metres a course keeps from a star's, a planet's or a moon's radius.
	static constexpr double kCourseClearance = 2000.0;
	static constexpr int kSolveIterations = 8;

	// Where a cruise to a ship arrives: this many metres off it, near enough to hail or board it and
	// clear of any hull's sphere, on the side it comes in from.
	static constexpr double kShipStandoff = 1000.0;

	// A turn at a waypoint is bent round on a curve of this many points from half-way along the leg
	// into it to half-way along the leg out: at a few degrees a point, the ship's facing turns over
	// many steps rather than in one. Where the curve would come within a body's clearance, it is cut
	// nearer the waypoint by halves, at most kBendCuts times, and then left a corner.
	static constexpr int kBendPoints = 16;
	static constexpr int kBendCuts = 4;

	// How many times through a cruise the ship's place is sampled to find where it passes each body,
	// which is placed there, widened by how far it moves to the samples beside: closer together towards
	// the ends (SettleCruise), so a planet grows by a few minutes' motion beside an end and about a
	// fortieth of its motion over the cruise mid-course, not half of it.
	static constexpr std::size_t kPassSamples = 64;

	// How far an arrival moves a ship that came out at rest where it comes out (CameOut): this many
	// metres further from the arrival, clear of a hull's sphere for the next; and how far such a ship
	// may have drifted, or been flown, from where it came out and still be waiting there.
	static constexpr double kMoveOffDistance = 250.0;
	static constexpr double kStillThere = 50.0;

	// Times a profile's steps may double from the solve's before a cruise is refused (NoCourse).
	static constexpr int kProfileDoublings = 32;

	// How often a cruise with a crossing is sampled as flown (SettleCruise): each minute while its
	// drive thrusts or it crosses on thrusters, near the bodies its spheres are about, and each hour
	// while it coasts between; and the times it plans again before it is refused.
	static constexpr double kThrustSampleSeconds = 60.0;
	static constexpr double kCoastSampleSeconds = 3600.0;
	static constexpr int kClearanceRounds = 4;

	// Registers the components, the command and the "cruise" system, ordered after the lanes'
	// (LaneTravel::Attach), which have to be attached first, and given the physics world whose shapes
	// say how far apart arrivals come out. Throws std::logic_error as SimWorld's registrations do,
	// for one already made.
	static CruiseTravel &Attach(
	    Jettison::Sim::SimWorld &world, const Jettison::Physics::PhysicsWorld &physics);

	// A star system, for where its bodies are. It has to be resolved; throws std::invalid_argument
	// for a system already defined. A Cruise in a system not defined is refused.
	void DefineStarSystem(Space::StarSystem system);

	bool HasStarSystem(std::uint64_t system) const;

	// How far from a body's centre a cruise to it arrives.
	static double ArrivalDistance(const Space::Body &body);

  private:
	explicit CruiseTravel(const Jettison::Physics::PhysicsWorld &physics) : m_physics{physics}
	{
	}

	void Start(Jettison::Sim::SimWorld &world, const Cruise &command) const;

	void BreakOff(Jettison::Sim::SimWorld &world, const BreakOffCruise &command) const;

	// Ends a ship's cruise where it is, as the last step left it (BreakOffCruise).
	void Leave(
	    Jettison::Sim::SimWorld &world, entt::entity ship, const Space::StarSystem &system) const;

	// The ships that came out of a cruise at rest in a system and are still waiting there (CameOut), as
	// the step ends at that time, in entity order.
	std::vector<entt::entity> Waiting(const Jettison::Sim::SimWorld &world, double endsAt) const;

	glm::dvec3 FrameOrigin(std::uint64_t system, const std::string &frame, double time) const;

	void Place(Jettison::Sim::SimWorld &world) const;

	// Places each cruising ship as the last step passed over would have (Jettison::Sim::Quiet), none
	// arriving.
	void Skip(Jettison::Sim::SimWorld &world) const;

	const Jettison::Physics::PhysicsWorld &m_physics;

	// By Jettison::Sim::FrameId; looked up, and ordered regardless.
	std::map<std::uint64_t, Space::StarSystem> m_systems;
};

// A cruise's course as it is settled (SettleCruise): from where the ship sets out to where it
// arrives, metres from the carrier in the system's axes, with its whole steps, its carrier, its
// clearances as the check left them and the velocities it leaves and arrives with (Cruising).
struct SettledCruise
{
	std::vector<glm::dvec3> course;
	std::uint64_t steps{0};
	std::string carrier;
	double departureClearance{0.0};
	double arrivalClearance{0.0};
	double length{0.0};
	glm::dvec3 departureVelocity{0.0};
	glm::dvec3 arrivalVelocity{0.0};

	// The course's metres, and those of them the drive thrusts over: all but its crossings.
	double Metres() const;

	double DriveMetres() const;
};

// A cruise settled, or the reason it is refused: NoSuchBody for a target, a target ship's frame or a
// keepout of no body of the system, NoCourse for no course round the bodies, and InsideRadius for a
// drive still thrusting within a keepout once its rounds are spent.
struct Settlement
{
	std::optional<SettledCruise> cruise;
	Reason refusal{Reason::NoCourse};
};

// Settles a cruise to the target from that place, in the frame of that body (a ship's
// Jettison::Physics::LocalFrame) and moving at that velocity relative to it (its
// Jettison::Physics::Velocity), at that time and step, as a Cruise does: the course and its time
// solved round the bodies in the way (CruiseTravel), and, for a drive with a profile and a crossing,
// checked as flown. The ship leaves with its frame's velocity and its own, relative to the carrier,
// and arrives with its target frame's (and a target ship's own), so the course is flown as they
// carry it: each body in the way is placed where it is as the ship passes nearest it, less how far
// the drift has the ship off its course then (CruiseTravel::kPassSamples), and a body only the
// frame it left orbits is placed as it sets out, one only the target's frame orbits as it arrives.
// The check samples the cruise where the bodies then are, each CruiseTravel::kThrustSampleSeconds
// while the drive thrusts or the ship crosses and each kCoastSampleSeconds while it coasts: while the
// drive thrusts no sample may be within a keepout, widened by how far the ship moved relative to its
// body since the last sample. Where it is, the crossing at that end (the half of the drive's metres it
// is in) is lengthened to reach past it, and the cruise is solved again, at most that many rounds.
// Deterministic: a game settles the same cruise to price it that a Cruise then flies. Fewer rounds are
// for tests.
Settlement SettleCruise(
    const StarSystem &system, const CruiseDrive &drive, const glm::dvec3 &from, std::string_view frame,
    const glm::dvec3 &velocity, const CruiseTarget &target, double now, double fixedStep,
    int rounds = CruiseTravel::kClearanceRounds);
} // namespace Manifest::Space
