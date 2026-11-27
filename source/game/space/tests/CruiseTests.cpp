#include "Systems.h"

#include <jettison/ai/Steering.h>
#include <jettison/physics/Flight.h>
#include <jettison/physics/PhysicsWorld.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/transit/Path.h>
#include <manifest/space/Cruise.h>
#include <manifest/space/Lanes.h>
#include <manifest/space/StarSystemFrame.h>
#include <manifest/space/Types.h>

#include <doctest/doctest.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace Jettison;

namespace
{
constexpr double kStep = 1.0 / 60.0;
const std::uint64_t kHalcyon = Jettison::Sim::FrameId("Halcyon");

// A shuttle's drive (defs/hulls/shuttle.yaml).
constexpr Manifest::Space::CruiseDrive kDrive{40.0f, 1.0e8, 0.25f, {}};

// A profile far past any crew's, so a test's cruise from Highport to Pell is minutes, not hours:
// boosting 10 s at each end, sustained between.
constexpr Transit::MotionProfile kHard{20000.0f, 20.0f, 5000.0f};

// The same thrusting for a quarter of the cruise, coasting for the rest.
constexpr Transit::MotionProfile kCoasting{20000.0f, 20.0f, 5000.0f, 0.25f};

// A world as a game's start-up makes one: physics, the lanes, then cruising, given the sample
// galaxy's systems.
struct Fixture
{
	Jettison::Sim::SimWorld world{kStep, 7};
	Jettison::Physics::PhysicsWorld &physics;
	Manifest::Space::LaneTravel &lanes;
	Manifest::Space::CruiseTravel &cruises;


	Fixture()
	    : physics{Setup(world)}, lanes{Manifest::Space::LaneTravel::Attach(world, physics)},
	      cruises{Manifest::Space::CruiseTravel::Attach(world, physics)}
	{
		Jettison::Physics::ShapeDefinition hull;
		hull.halfExtents = {3.2f, 0.9f, 4.9f};
		physics.DefineShape("hull", hull);
		for (const Manifest::Space::StarSystem &system :
		     {AiTests::Halcyon(), AiTests::Vesper(), AiTests::Marrow()})
		{
			Manifest::Space::DefineStarSystem(physics, system);
			lanes.DefineStarSystem(system);
			cruises.DefineStarSystem(system);
		}
		lanes.SetGalaxy(AiTests::Sample());
	}


	static Jettison::Physics::PhysicsWorld &Setup(Jettison::Sim::SimWorld &world)
	{
		Jettison::Sim::RegisterTypes();
		world.RegisterComponent<Jettison::Sim::Transform>();
		world.RegisterComponent<Jettison::Sim::InFrame>();
		world.RegisterComponent<Jettison::Sim::ControlledBy>();
		return Jettison::Physics::PhysicsWorld::Attach(world);
	}


	// A ship at rest in the frame of a body of Halcyon, that far from it along the direction, with a
	// shuttle's drive.
	entt::entity Ship(std::string_view frame, const glm::dvec3 &offset)
	{
		entt::registry &registry = world.Registry();
		const entt::entity ship = registry.create();
		registry.emplace<Jettison::Sim::Transform>(
		    ship, Jettison::Sim::Transform{
		              Where(frame, world.Clock().Now()) + offset, glm::dquat{1.0, 0.0, 0.0, 0.0}});
		registry.emplace<Jettison::Sim::InFrame>(ship, kHalcyon);
		registry.emplace<Jettison::Physics::LocalFrame>(ship, std::string{frame});
		Jettison::Physics::Body body;
		body.kind = Jettison::Physics::BodyKind::Dynamic;
		body.shape = "hull";
		body.mass = 20000.0f;
		registry.emplace<Jettison::Physics::Body>(ship, body);
		registry.emplace<Jettison::Physics::Velocity>(ship);
		registry.emplace<Jettison::Physics::FlightControls>(ship);
		registry.emplace<Manifest::Space::CruiseDrive>(ship, kDrive);
		return ship;
	}


	static glm::dvec3 Where(std::string_view body, double time)
	{
		const Manifest::Space::StarSystem halcyon = AiTests::Halcyon();
		return Manifest::Space::EvaluateBody(
		           halcyon, static_cast<std::size_t>(halcyon.Find(body)), time)
		    .metres;
	}


	void Run(std::uint64_t steps)
	{
		for (std::uint64_t step = 0; step < steps; ++step)
			world.Step();
	}


	template <typename T> std::vector<T> Events()
	{
		std::vector<T> found;
		for (const Jettison::Sim::SimEvent &event : world.Events().Take())
			if (const T *value = event.event.try_cast<T>())
				found.push_back(*value);
		return found;
	}


	// The cruise's refusal, from the step the command is applied in.
	Jettison::Sim::Reason Refusal(const Manifest::Space::Cruise &command)
	{
		Events<Manifest::Space::CruiseRefused>();
		world.Commands().Push(command);
		Run(1);
		const std::vector<Manifest::Space::CruiseRefused> refused =
		    Events<Manifest::Space::CruiseRefused>();
		REQUIRE(refused.size() == 1);
		CHECK(refused[0].ship == command.ship);
		CHECK(refused[0].body == command.body);
		return refused[0].reason;
	}
};


const Manifest::Space::Body &BodyOf(std::string_view name)
{
	static const Manifest::Space::StarSystem halcyon = AiTests::Halcyon();
	return halcyon.bodies[static_cast<std::size_t>(halcyon.Find(name))];
}


// The way a transform's nose (-Z) points.
glm::dvec3 Nose(const Jettison::Sim::Transform &transform)
{
	return transform.orientation * glm::dvec3{0.0, 0.0, -1.0};
}
} // namespace


TEST_CASE("Cruise: a drive's time rises as the distance's fourth root")
{
	CHECK(Manifest::Space::CruiseSeconds(kDrive, 1.0e8) == doctest::Approx(40.0));
	CHECK(Manifest::Space::CruiseSeconds(kDrive, 1.6e9) == doctest::Approx(80.0));
	CHECK(Manifest::Space::CruiseSeconds(kDrive, 0.0) == 0.0);

	// Off a station by its radius, off a body with gravity where it pulls at 1 m/s².
	CHECK(
	    Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Ostra Highport")) ==
	    doctest::Approx(4075.0));
	CHECK(
	    Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Pell")) == doctest::Approx(2.214688e6));
	CHECK(
	    Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Ostra")) == doctest::Approx(1.996498e7));
}


TEST_CASE("Cruise: a drive with a profile takes the profile's time, and a bad profile names the drive")
{
	const Manifest::Space::CruiseDrive drive{.profile = kHard};
	for (const double metres : {0.0, 1.0e6, 3.93e8})
		CHECK(Manifest::Space::CruiseSeconds(drive, metres) == Transit::Seconds(kHard, metres));
	CHECK_NOTHROW(Manifest::Space::CheckCruiseDrive("hard", drive));
	CHECK_THROWS_WITH_AS(
	    Manifest::Space::CheckCruiseDrive(
	        "bad", {.profile = Transit::MotionProfile{0.0f, 60.0f, 0.0f}}),
	    doctest::Contains("Cruise drive 'bad' has a profile"), Jettison::Serialise::FormatError);
}


TEST_CASE("Cruise: a drive of no time, no reference or an exponent outside (0, 1] is refused")
{
	CHECK_NOTHROW(Manifest::Space::CheckCruiseDrive("shuttle", kDrive));
	CHECK_NOTHROW(Manifest::Space::CheckCruiseDrive("linear", {40.0f, 1.0e8, 1.0f, {}}));
	for (const Manifest::Space::CruiseDrive &drive :
	     {Manifest::Space::CruiseDrive{0.0f, 1.0e8, 0.25f, {}},
	      Manifest::Space::CruiseDrive{40.0f, 0.0, 0.25f, {}},
	      Manifest::Space::CruiseDrive{40.0f, 1.0e8, 0.0f, {}},
	      Manifest::Space::CruiseDrive{40.0f, 1.0e8, 1.5f, {}},
	      Manifest::Space::CruiseDrive{std::numeric_limits<float>::infinity(), 1.0e8, 0.25f, {}}})
		CHECK_THROWS_AS(
		    Manifest::Space::CheckCruiseDrive("bad", drive), Jettison::Serialise::FormatError);
}


TEST_CASE("Cruise: a ship crosses from Highport to Pell out of physics, and arrives at rest off it")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	fixture.Run(5);
	CHECK(fixture.physics.BodyCount(kHalcyon) == 1);
	fixture.Events<Manifest::Space::CruiseStarted>();

	const std::uint64_t departure = fixture.world.Clock().Step();
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	const std::vector<Manifest::Space::CruiseStarted> started =
	    fixture.Events<Manifest::Space::CruiseStarted>();
	REQUIRE(started.size() == 1);
	CHECK(started[0].ship == ship);
	CHECK(started[0].body == "Pell");

	// The time its drive takes over its course, to within the step it is rounded up to and what the
	// last round of the solve moved the course by.
	CHECK(started[0].metres > 3.6e8);
	CHECK(
	    std::abs(started[0].seconds - Manifest::Space::CruiseSeconds(kDrive, started[0].metres)) <
	    2.0 * kStep);
	const std::uint64_t arrival = started[0].arrivalStep;
	CHECK(arrival == departure + static_cast<std::uint64_t>(std::llround(started[0].seconds / kStep)));

	// Out of physics and its frame, and still in its system, placed along its course.
	const Manifest::Space::Cruising *cruising = registry.try_get<Manifest::Space::Cruising>(ship);
	REQUIRE(cruising);
	CHECK(cruising->target == "Pell");
	CHECK(cruising->frame == "Pell");
	CHECK(cruising->carrier == "Ostra");
	CHECK(cruising->departureStep == departure);
	CHECK(!registry.any_of<
	       Jettison::Physics::Body, Jettison::Physics::Velocity, Jettison::Physics::LocalFrame>(ship));
	CHECK(fixture.physics.BodyCount() == 0);
	CHECK(registry.get<Jettison::Sim::InFrame>(ship).frame == kHalcyon);

	// Clear of Ostra and Pell at every step, and closing on Pell, slowly at first.
	double nearestOstra = std::numeric_limits<double>::infinity();
	double firstSecond = 0.0;
	const glm::dvec3 setOut = registry.get<Jettison::Sim::Transform>(ship).position -
	                          Fixture::Where("Ostra Highport", fixture.world.Clock().Now());
	while (fixture.world.Clock().Step() + 1 < arrival)
	{
		fixture.Run(1);
		const double now = fixture.world.Clock().Now();
		const glm::dvec3 at = registry.get<Jettison::Sim::Transform>(ship).position;
		nearestOstra = std::min(nearestOstra, glm::distance(at, Fixture::Where("Ostra", now)));
		CHECK(glm::distance(at, Fixture::Where("Pell", now)) > BodyOf("Pell").radius);
		if (fixture.world.Clock().Step() == departure + 61)
			firstSecond = glm::length(at - Fixture::Where("Ostra Highport", now) - setOut);
	}
	MESSAGE(
	    "Highport to Pell: " << started[0].metres / 1000.0 << " km in " << started[0].seconds << " s, "
	                         << firstSecond << " m out in the first second, nearest Ostra "
	                         << (nearestOstra - BodyOf("Ostra").radius) / 1000.0 << " km up");
	CHECK(nearestOstra > BodyOf("Ostra").radius);
	CHECK(firstSecond > 0.0);
	CHECK(firstSecond < 20000.0);
	CHECK(registry.all_of<Manifest::Space::Cruising>(ship));
	CHECK(fixture.Events<Manifest::Space::CruiseEnded>().empty());

	// The step that ends the cruise brings it out, facing Pell, at rest in its frame, its body back.
	fixture.Run(1);
	CHECK(fixture.world.Clock().Step() == arrival);
	const std::vector<Manifest::Space::CruiseEnded> ended =
	    fixture.Events<Manifest::Space::CruiseEnded>();
	REQUIRE(ended.size() == 1);
	CHECK(ended[0].ship == ship);
	CHECK(ended[0].body == "Pell");
	CHECK(!registry.all_of<Manifest::Space::Cruising>(ship));
	CHECK(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Pell");
	CHECK(registry.get<Jettison::Physics::Velocity>(ship) == Jettison::Physics::Velocity{});
	const Jettison::Sim::Transform arrived = registry.get<Jettison::Sim::Transform>(ship);
	const glm::dvec3 pell = Fixture::Where("Pell", fixture.world.Clock().Now());
	CHECK(
	    glm::distance(arrived.position, pell) ==
	    doctest::Approx(Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Pell")))
	        .epsilon(1.0e-9));
	CHECK(glm::dot(Nose(arrived), glm::normalize(pell - arrived.position)) > 0.999999);

	// Physics makes its body again in the next step, in Pell's frame, where it is all but still.
	fixture.Run(1);
	CHECK(fixture.physics.BodyCount(kHalcyon) == 1);
	CHECK(fixture.physics.FrameCount(kHalcyon) == 1);
	CHECK(glm::length(registry.get<Jettison::Physics::Velocity>(ship).linear) < 0.1f);

	// And back to the gate, which keeps station by Highport, in whose frame it arrives.
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Halcyon Gate"});
	fixture.Run(1);
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(ship));
	CHECK(registry.get<Manifest::Space::Cruising>(ship).frame == "Ostra Highport");
	fixture.Run(
	    registry.get<Manifest::Space::Cruising>(ship).arrivalStep - fixture.world.Clock().Step());
	CHECK(!registry.all_of<Manifest::Space::Cruising>(ship));
	CHECK(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Ostra Highport");

	// To the millimetre: a place 1.5 x 10^11 m from the star is held to about 30 µm.
	CHECK(
	    std::abs(
	        glm::distance(
	            registry.get<Jettison::Sim::Transform>(ship).position,
	            Fixture::Where("Halcyon Gate", fixture.world.Clock().Now())) -
	        Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Halcyon Gate"))) < 0.001);
}


TEST_CASE("Cruise: ships that arrive together at one body come out clear of each other")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();

	// Two that set out a few metres apart, and one that has been waiting where they would come out.
	const entt::entity first = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	const entt::entity second = fixture.Ship("Ostra Highport", {4.0, 0.0, 3000.0});
	fixture.Run(5);
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, first, "Pell"});
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, second, "Pell"});
	fixture.Run(1);
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(first));
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(second));
	const std::uint64_t arrival = std::max(
	    registry.get<Manifest::Space::Cruising>(first).arrivalStep,
	    registry.get<Manifest::Space::Cruising>(second).arrivalStep);
	fixture.Run(arrival - fixture.world.Clock().Step());
	REQUIRE(!registry.all_of<Manifest::Space::Cruising>(first));
	REQUIRE(!registry.all_of<Manifest::Space::Cruising>(second));

	const double hull = fixture.physics.ShapeRadius("hull");
	const glm::dvec3 firstAt = registry.get<Jettison::Sim::Transform>(first).position;
	const glm::dvec3 secondAt = registry.get<Jettison::Sim::Transform>(second).position;
	const double apart = glm::distance(firstAt, secondAt) - 2.0 * hull;
	MESSAGE("Arrived " << apart << " m between hulls");
	CHECK(apart >= Manifest::Space::LaneTravel::kArrivalClearance);

	// Not further than the first ring, a hull's breadth and the clearance out, and the few metres
	// the two courses differ by.
	CHECK(
	    glm::distance(firstAt, secondAt) <=
	    2.0 * hull + Manifest::Space::LaneTravel::kArrivalClearance + 10.0);
	const glm::dvec3 pell = Fixture::Where("Pell", fixture.world.Clock().Now());
	for (const glm::dvec3 &at : {firstAt, secondAt})
		CHECK(
		    std::abs(
		        glm::distance(at, pell) -
		        Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Pell"))) <
		    2.0 * hull + Manifest::Space::LaneTravel::kArrivalClearance);
}


TEST_CASE("Cruise: a course goes round the planet between a ship and its target")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();

	// In Ostra's frame, three of its radii out on the far side from Pell.
	const glm::dvec3 away = glm::normalize(Fixture::Where("Ostra", 0.0) - Fixture::Where("Pell", 0.0));
	const entt::entity ship = fixture.Ship("Ostra", away * 2.0e7);
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	const Manifest::Space::Cruising *cruising = registry.try_get<Manifest::Space::Cruising>(ship);
	REQUIRE(cruising);
	CHECK(cruising->carrier == "Ostra");
	CHECK(cruising->course.size() > 2);
	const std::size_t cruisingPoints = cruising->course.size();

	// Ostra carries the course, so its clearance holds at every step.
	double nearest = std::numeric_limits<double>::infinity();
	const std::uint64_t arrival = cruising->arrivalStep;
	while (fixture.world.Clock().Step() < arrival)
	{
		fixture.Run(1);
		nearest = std::min(
		    nearest, glm::distance(
		                 registry.get<Jettison::Sim::Transform>(ship).position,
		                 Fixture::Where("Ostra", fixture.world.Clock().Now())));
	}
	MESSAGE(
	    "Round Ostra to Pell: " << cruisingPoints << " points, "
	                            << (nearest - BodyOf("Ostra").radius) / 1000.0 << " km up at nearest");
	CHECK(nearest > BodyOf("Ostra").radius + Manifest::Space::CruiseTravel::kCourseClearance - 1.0);
	CHECK(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Pell");
}


TEST_CASE("Cruise: off a planet where its pull is one its assist holds against")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();

	// From Pell: Highport, in low orbit, is already where a cruise to Ostra would arrive.
	const entt::entity ship = fixture.Ship("Pell", {0.0, 3.0e6, 0.0});
	registry.emplace<Jettison::Physics::Handling>(ship);
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Ostra"});
	fixture.Run(1);
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(ship));
	fixture.Run(
	    registry.get<Manifest::Space::Cruising>(ship).arrivalStep - fixture.world.Clock().Step());
	REQUIRE(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Ostra");
	const glm::dvec3 arrived = registry.get<Jettison::Sim::Transform>(ship).position -
	                           Fixture::Where("Ostra", fixture.world.Clock().Now());
	CHECK(glm::length(arrived) == doctest::Approx(1.996498e7));

	// A minute later, held by the assist against Ostra's 1 m/s², it has hardly moved in its frame.
	fixture.Run(3600);
	const glm::dvec3 held = registry.get<Jettison::Sim::Transform>(ship).position -
	                        Fixture::Where("Ostra", fixture.world.Clock().Now());
	MESSAGE("Off Ostra, a minute after arriving: " << glm::distance(held, arrived) << " m from it");
	CHECK(glm::distance(held, arrived) < 5.0);
}


TEST_CASE("Cruise: to the star, a system's breadth, carried by nothing that moves")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 3000.0, 0.0});
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Halcyon"});
	fixture.Run(1);
	const Manifest::Space::Cruising *cruising = registry.try_get<Manifest::Space::Cruising>(ship);
	REQUIRE(cruising);
	CHECK(cruising->carrier == "Halcyon");

	// A few minutes for a hundred and fifty million kilometres.
	const double seconds = static_cast<double>(cruising->arrivalStep - cruising->departureStep) * kStep;
	MESSAGE("Highport to Halcyon: " << seconds << " s");
	CHECK(seconds > 200.0);
	CHECK(seconds < 300.0);
	fixture.Run(cruising->arrivalStep - fixture.world.Clock().Step());
	CHECK(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Halcyon");
	CHECK(
	    glm::length(registry.get<Jettison::Sim::Transform>(ship).position) ==
	    doctest::Approx(Manifest::Space::CruiseTravel::ArrivalDistance(BodyOf("Halcyon")))
	        .epsilon(1.0e-9));
}


TEST_CASE("Cruise: refused, with its reason")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	const auto reason =
	    [](Manifest::Space::Reason reason, std::vector<Jettison::Sim::ReasonArgument> arguments = {})
	{ return Jettison::Sim::MakeReason(reason, std::move(arguments)); };
	const auto identifier = [](std::string name)
	{ return Jettison::Sim::ReasonArgument::Identifier(std::move(name)); };

	CHECK(
	    fixture.Refusal({{}, entt::entity{999}, "Pell"}) ==
	    reason(Manifest::Space::Reason::NoSuchEntity));
	CHECK(
	    fixture.Refusal({{}, ship, "Corva"}) ==
	    reason(Manifest::Space::Reason::NoSuchBody, {identifier("Halcyon"), identifier("Corva")}));

	// Commanded by a player, from another.
	const Jettison::Sim::Controller player{Jettison::Sim::ControllerKind::Player, 0};
	registry.emplace<Jettison::Sim::ControlledBy>(ship, player);
	CHECK(
	    fixture.Refusal({{Jettison::Sim::ControllerKind::Player, 1}, ship, "Pell"}) ==
	    reason(Manifest::Space::Reason::NotController));

	// Within a station's reach, so docking stays in physics.
	const entt::entity docking = fixture.Ship("Ostra Highport", {0.0, 0.0, 1500.0});
	CHECK(
	    fixture.Refusal({{}, docking, "Pell"}) ==
	    reason(Manifest::Space::Reason::NearStation, {identifier("Ostra Highport")}));

	// Without a drive, and held on rails.
	registry.remove<Manifest::Space::CruiseDrive>(docking);
	registry.get<Jettison::Sim::Transform>(docking).position += glm::dvec3{0.0, 0.0, 2000.0};
	CHECK(fixture.Refusal({{}, docking, "Pell"}) == reason(Manifest::Space::Reason::CannotCruise));
	registry.emplace<Manifest::Space::CruiseDrive>(docking, kDrive);
	registry.emplace<Jettison::Physics::OnRails>(docking, Jettison::Physics::OnRails{"Ostra Highport"});
	CHECK(fixture.Refusal({{}, docking, "Pell"}) == reason(Manifest::Space::Reason::CannotCruise));

	// Already where a cruise would bring it.
	const entt::entity near = fixture.Ship("Pell", {0.0, 2.0e6, 0.0});
	CHECK(
	    fixture.Refusal({{}, near, "Pell"}) ==
	    reason(Manifest::Space::Reason::AlreadyThere, {identifier("Pell")}));

	// Cruising, a second cruise and a lane are refused.
	fixture.world.Commands().Push(Manifest::Space::Cruise{player, ship, "Pell"});
	fixture.Run(1);
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(ship));
	CHECK(
	    fixture.Refusal({player, ship, "Halcyon"}) == reason(Manifest::Space::Reason::AlreadyCruising));
	fixture.Events<Manifest::Space::LaneRefused>();
	fixture.world.Commands().Push(Manifest::Space::EnterLane{{}, ship, "Halcyon Gate"});
	fixture.Run(1);
	const std::vector<Manifest::Space::LaneRefused> lane =
	    fixture.Events<Manifest::Space::LaneRefused>();
	REQUIRE(lane.size() == 1);
	CHECK(lane[0].reason == reason(Manifest::Space::Reason::AlreadyCruising));
	CHECK(registry.all_of<Manifest::Space::Cruising>(ship));
}


TEST_CASE(
    "Cruise: a ship within a station's reach is given the flight out of it, from where it cruises")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	Jettison::Physics::ShapeDefinition hub;
	hub.halfExtents = {40.0f, 20.0f, 40.0f};
	fixture.physics.DefineShape("hub", hub);
	const entt::entity station = registry.create();
	registry.emplace<Jettison::Sim::Transform>(
	    station,
	    Jettison::Sim::Transform{Fixture::Where("Ostra Highport", fixture.world.Clock().Now())});
	registry.emplace<Jettison::Sim::InFrame>(station, kHalcyon);
	registry.emplace<Jettison::Physics::OnRails>(station, Jettison::Physics::OnRails{"Ostra Highport"});
	registry.emplace<Jettison::Physics::Body>(
	    station, Jettison::Physics::Body{Jettison::Physics::BodyKind::Kinematic, "hub"});
	const Manifest::Space::StarSystem halcyon = AiTests::Halcyon();

	// Beyond Pell from the station, so the way out runs through the hub and goes round it.
	const glm::dvec3 toPell =
	    glm::normalize(Fixture::Where("Pell", 0.0) - Fixture::Where("Ostra Highport", 0.0));
	const entt::entity ship = fixture.Ship("Ostra Highport", -toPell * 200.0);
	const std::optional<Jettison::Ai::Destination> flight =
	    Manifest::Space::ClearOf(fixture.world, fixture.physics, halcyon, ship, station, "Pell");
	REQUIRE(flight);
	CHECK_FALSE(flight->waypoints.empty());
	const glm::dvec3 origin = fixture.physics.FrameOrigin(kHalcyon, "Ostra Highport", 0.0);
	const glm::dvec3 end = origin + flight->offset;
	const double reach = BodyOf("Ostra Highport").radius + Manifest::Space::CruiseTravel::kStationReach;
	CHECK(
	    glm::distance(end, Fixture::Where("Ostra Highport", 0.0)) ==
	    doctest::Approx(reach + Manifest::Space::kReachMargin));
	CHECK(glm::dot(end - Fixture::Where("Ostra Highport", 0.0), toPell) > 0.0);

	// Each waypoint is clear of the hub's sphere.
	for (const glm::dvec3 &waypoint : flight->waypoints)
		CHECK(
		    glm::distance(origin + waypoint, Fixture::Where("Ostra Highport", 0.0)) >
		    fixture.physics.ShapeRadius("hub"));

	// Where the flight ends, the cruise is accepted; outside the reach, there is no flight to take.
	registry.get<Jettison::Sim::Transform>(ship).position = end;
	CHECK_FALSE(
	    Manifest::Space::ClearOf(fixture.world, fixture.physics, halcyon, ship, station, "Pell"));
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	CHECK(registry.all_of<Manifest::Space::Cruising>(ship));

	// Nor for a body the system does not have, or a ship in no frame.
	const entt::entity other = fixture.Ship("Ostra Highport", {0.0, 0.0, 500.0});
	CHECK_FALSE(
	    Manifest::Space::ClearOf(fixture.world, fixture.physics, halcyon, other, station, "Corva"));
	registry.remove<Jettison::Physics::LocalFrame>(other);
	CHECK_FALSE(
	    Manifest::Space::ClearOf(fixture.world, fixture.physics, halcyon, other, station, "Pell"));
}


// A game's system after physics, as Manifest's travel is, sees every Transform placed for the step's
// end; a station's frame moves with its planet by far more than the reach's margin in a step.
TEST_CASE("Cruise: after physics in a step, a station's reach is measured where physics placed it")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	Jettison::Physics::ShapeDefinition hub;
	hub.halfExtents = {40.0f, 20.0f, 40.0f};
	fixture.physics.DefineShape("hub", hub);
	const entt::entity station = registry.create();
	registry.emplace<Jettison::Sim::Transform>(
	    station,
	    Jettison::Sim::Transform{Fixture::Where("Ostra Highport", fixture.world.Clock().Now())});
	registry.emplace<Jettison::Sim::InFrame>(station, kHalcyon);
	registry.emplace<Jettison::Physics::OnRails>(station, Jettison::Physics::OnRails{"Ostra Highport"});
	registry.emplace<Jettison::Physics::Body>(
	    station, Jettison::Physics::Body{Jettison::Physics::BodyKind::Kinematic, "hub"});
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 500.0});
	const Manifest::Space::StarSystem halcyon = AiTests::Halcyon();
	const double reach = BodyOf("Ostra Highport").radius + Manifest::Space::CruiseTravel::kStationReach;
	constexpr double kOutside = 10.0;

	bool checked = false;
	bool cleared = false;
	bool cruising = false;
	double moved = 0.0;
	fixture.world.RegisterSystem(
	    {"test.after_physics",
	     Jettison::Ecs::TickPhase::Simulation,
	     Jettison::Ecs::Access::Of<
	         Jettison::Sim::Transform, Jettison::Sim::InFrame, Jettison::Physics::OnRails,
	         Jettison::Physics::Body, Jettison::Physics::LocalFrame, Manifest::Space::CruiseDrive>()
	         .Writing<
	             Jettison::Sim::Transform, Manifest::Space::Cruising, Jettison::Physics::Body,
	             Jettison::Physics::LocalFrame, Jettison::Physics::Velocity,
	             Jettison::Physics::FlightControls>(),
	     {std::string{Jettison::Physics::PhysicsWorld::kSystemName}},
	     [&](entt::registry &, double)
	     {
		     if (checked)
			     return;
		     checked = true;

		     // Just outside the reach of the station as placed, behind it as it goes.
		     const double now = fixture.world.Clock().Now();
		     const glm::dvec3 placed = registry.get<const Jettison::Sim::Transform>(station).position;
		     moved = glm::distance(placed, Fixture::Where("Ostra Highport", now));
		     const glm::dvec3 going = glm::normalize(placed - Fixture::Where("Ostra Highport", now));
		     registry.get<Jettison::Sim::Transform>(ship).position =
		         placed - (going * (reach + kOutside));
		     cleared = !Manifest::Space::ClearOf(
		         fixture.world, fixture.physics, halcyon, ship, station, "Pell");
		     fixture.world.Apply(Manifest::Space::Cruise{{}, ship, "Pell"});
		     cruising = registry.all_of<Manifest::Space::Cruising>(ship);
	     }});
	fixture.Run(1);
	REQUIRE(checked);
	REQUIRE(fixture.physics.PlacedAt());
	CHECK(*fixture.physics.PlacedAt() == doctest::Approx(kStep));

	// Measured at the clock's Now, it would be inside by all but the margin.
	CHECK(moved > 10.0 * kOutside);
	CHECK(cleared);
	CHECK(cruising);
}


TEST_CASE(
    "Cruise: a steered ship gives up its destination and controls with it, a piloted one keeps them")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity steered = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	const entt::entity piloted = fixture.Ship("Ostra Highport", {0.0, 0.0, -3000.0});

	// What steering last demanded of one, and a pilot of the other.
	registry.emplace<Jettison::Ai::Destination>(steered);
	const Jettison::Physics::FlightControls demand{{0.0f, 0.0f, -0.6f}, {}, true, entt::null};
	registry.replace<Jettison::Physics::FlightControls>(steered, demand);
	registry.replace<Jettison::Physics::FlightControls>(piloted, demand);
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, steered, "Pell"});
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, piloted, "Pell"});
	fixture.Run(1);
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(steered));
	CHECK_FALSE(registry.all_of<Jettison::Ai::Destination>(steered));
	CHECK(
	    registry.get<Jettison::Physics::FlightControls>(steered) ==
	    Jettison::Physics::FlightControls{});
	CHECK(registry.get<Jettison::Physics::FlightControls>(piloted) == demand);

	// And so it comes out: let go, taken against nothing.
	while (registry.all_of<Manifest::Space::Cruising>(steered))
		fixture.Run(1);
	CHECK(
	    registry.get<Jettison::Physics::FlightControls>(steered) ==
	    Jettison::Physics::FlightControls{});
}


TEST_CASE("Cruise: on a profile, a ship covers its course as the profile does, never past its boost")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	const Manifest::Space::CruiseDrive drive{.profile = kHard};
	registry.replace<Manifest::Space::CruiseDrive>(ship, drive);
	fixture.Run(5);
	fixture.Events<Manifest::Space::CruiseStarted>();
	const std::uint64_t departure = fixture.world.Clock().Step();
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	const std::vector<Manifest::Space::CruiseStarted> started =
	    fixture.Events<Manifest::Space::CruiseStarted>();
	REQUIRE(started.size() == 1);
	const Manifest::Space::Cruising cruising = registry.get<Manifest::Space::Cruising>(ship);
	REQUIRE(cruising.course.size() == 2);
	CHECK(cruising.profile == kHard);

	// Time enough for the course, and within a few steps of the least.
	const double least = Manifest::Space::CruiseSeconds(drive, started[0].metres);
	CHECK(started[0].seconds >= least);
	CHECK(started[0].seconds < least + (4.0 * kStep));

	// Along a straight course from the carrier's point of view: no step accelerates past the boost,
	// and the boost and the sustained rate are both reached.
	const auto relative = [&]
	{
		return registry.get<Jettison::Sim::Transform>(ship).position -
		       Fixture::Where("Ostra", fixture.world.Clock().Now());
	};
	std::vector<glm::dvec3> places{cruising.course.front(), relative()};
	double hardest = 0.0;
	double quarter = 0.0;
	const std::uint64_t quarterway = departure + ((started[0].arrivalStep - departure) / 4);
	while (fixture.world.Clock().Step() + 1 < started[0].arrivalStep)
	{
		fixture.Run(1);
		places.push_back(relative());
		const std::size_t last = places.size() - 1;
		const double acceleration =
		    glm::length(places[last] - (2.0 * places[last - 1]) + places[last - 2]) / (kStep * kStep);
		hardest = std::max(hardest, acceleration);
		if (fixture.world.Clock().Step() == quarterway)
			quarter = acceleration;
	}
	MESSAGE(
	    "Highport to Pell on a profile: " << started[0].metres / 1000.0 << " km in "
	                                      << started[0].seconds << " s, hardest " << hardest
	                                      << " m/s², a quarter in " << quarter << " m/s²");
	CHECK(hardest <= double{kHard.boostAcceleration} * 1.001);
	CHECK(hardest > double{kHard.boostAcceleration} * 0.99);
	CHECK(quarter == doctest::Approx(double{kHard.sustainedAcceleration}).epsilon(0.01));
	CHECK(glm::distance(places.back(), cruising.course.back()) < 10.0);

	fixture.Run(1);
	CHECK(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Pell");
	CHECK(registry.get<Jettison::Physics::Velocity>(ship) == Jettison::Physics::Velocity{});
}


TEST_CASE("Cruise: a drive's clearance is crossed on thrusters at each end, from rest to rest")
{
	const Manifest::Space::CruiseDrive drive{
	    .profile = kHard,
	    .departureClearance = 1.0e6,
	    .arrivalClearance = 1.0e6,
	    .crossingAcceleration = 15.0f};
	const Transit::MotionProfile thrusters{15.0f, 0.0f, 15.0f};
	CHECK(
	    Manifest::Space::CruiseSeconds(drive, 3.93e8) ==
	    doctest::Approx(
	        (2.0 * Transit::Seconds(thrusters, 1.0e6)) + Transit::Seconds(kHard, 3.93e8 - 2.0e6)));
	CHECK(Manifest::Space::DriveMetres(drive, 3.93e8) == doctest::Approx(3.91e8));

	// A cruise shorter than both crossings is thrusters all the way.
	CHECK(
	    Manifest::Space::CruiseSeconds(drive, 1.0e6) ==
	    doctest::Approx(2.0 * Transit::Seconds(thrusters, 5.0e5)));
	CHECK(Manifest::Space::DriveMetres(drive, 1.0e6) == 0.0);

	// The power law crosses nothing.
	CHECK(
	    Manifest::Space::DriveMetres(
	        {.profile = {},
	         .departureClearance = 1.0e6,
	         .arrivalClearance = 1.0e6,
	         .crossingAcceleration = 15.0f},
	        5.0e6) == 5.0e6);
	CHECK_NOTHROW(Manifest::Space::CheckCruiseDrive("crossing", drive));
	for (const Manifest::Space::CruiseDrive &bad :
	     {Manifest::Space::CruiseDrive{.profile = kHard, .departureClearance = -1.0},
	      Manifest::Space::CruiseDrive{.profile = kHard, .arrivalClearance = 1.0e6}})
		CHECK_THROWS_WITH_AS(
		    Manifest::Space::CheckCruiseDrive("bad", bad),
		    doctest::Contains("Cruise drive 'bad' has a clearance"), Serialise::FormatError);
}


TEST_CASE("Cruise: a ship crosses its drive's clearance at its thrusters' acceleration, then drives")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	const Manifest::Space::CruiseDrive drive{
	    .profile = kHard,
	    .departureClearance = 1.0e6,
	    .arrivalClearance = 1.0e6,
	    .crossingAcceleration = 15.0f};
	registry.replace<Manifest::Space::CruiseDrive>(ship, drive);
	fixture.Run(5);
	fixture.Events<Manifest::Space::CruiseStarted>();
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	const std::vector<Manifest::Space::CruiseStarted> started =
	    fixture.Events<Manifest::Space::CruiseStarted>();
	REQUIRE(started.size() == 1);
	const Manifest::Space::Cruising cruising = registry.get<Manifest::Space::Cruising>(ship);
	CHECK(cruising.departureClearance == 1.0e6);
	CHECK(cruising.arrivalClearance == 1.0e6);
	CHECK(cruising.departureBody == "Ostra");
	const double least = Manifest::Space::CruiseSeconds(drive, started[0].metres);
	CHECK(started[0].seconds >= least);
	CHECK(started[0].seconds < least + (4.0 * kStep));
	CHECK(started[0].seconds > Manifest::Space::CruiseSeconds({.profile = kHard}, started[0].metres));

	const auto relative = [&]
	{
		return registry.get<Jettison::Sim::Transform>(ship).position -
		       Fixture::Where("Ostra", fixture.world.Clock().Now());
	};
	std::vector<glm::dvec3> places{cruising.course.front(), relative()};
	std::vector<double> accelerations;
	while (fixture.world.Clock().Step() + 1 < started[0].arrivalStep)
	{
		fixture.Run(1);
		places.push_back(relative());
		const std::size_t last = places.size() - 1;
		accelerations.push_back(
		    glm::length(places[last] - (2.0 * places[last - 1]) + places[last - 2]) / (kStep * kStep));
	}

	// The first minute is on thrusters, the crossing taking 516 s; the drive's boost comes after.
	const double crossing = 2.0 * std::sqrt(1.0e6 / 15.0);
	const auto atSecond = [&](double seconds)
	{ return accelerations[static_cast<std::size_t>(seconds / kStep)]; };
	CHECK(atSecond(60.0) == doctest::Approx(15.0).epsilon(0.01));
	CHECK(atSecond(crossing + 5.0) == doctest::Approx(double{kHard.boostAcceleration}).epsilon(0.01));
	CHECK(std::ranges::max(accelerations) <= double{kHard.boostAcceleration} * 1.001);
	MESSAGE(
	    "Highport to Pell crossing 1,000 km at each end: " << started[0].metres / 1000.0 << " km in "
	                                                       << started[0].seconds << " s");
	// Its arrival crossing is flown in Pell's frame, which moves some 17 m a step against Ostra's.
	CHECK(glm::distance(places.back(), cruising.course.back()) < 30.0);
}


TEST_CASE("Cruise: on a profile with a share, a ship thrusts out, coasts, and thrusts in to rest")
{
	Fixture fixture;
	entt::registry &registry = fixture.world.Registry();
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	const Manifest::Space::CruiseDrive drive{.profile = kCoasting};
	registry.replace<Manifest::Space::CruiseDrive>(ship, drive);
	fixture.Run(5);
	fixture.Events<Manifest::Space::CruiseStarted>();
	const std::uint64_t departure = fixture.world.Clock().Step();
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	const std::vector<Manifest::Space::CruiseStarted> started =
	    fixture.Events<Manifest::Space::CruiseStarted>();
	REQUIRE(started.size() == 1);
	const Manifest::Space::Cruising cruising = registry.get<Manifest::Space::Cruising>(ship);
	CHECK(cruising.profile == kCoasting);
	const double least = Manifest::Space::CruiseSeconds(drive, started[0].metres);
	CHECK(started[0].seconds >= least);
	CHECK(started[0].seconds < least + (4.0 * kStep));

	// Slower than thrusting throughout.
	CHECK(least > Manifest::Space::CruiseSeconds({.profile = kHard}, started[0].metres) * 1.4);

	const auto relative = [&]
	{
		return registry.get<Jettison::Sim::Transform>(ship).position -
		       Fixture::Where("Ostra", fixture.world.Clock().Now());
	};
	std::vector<glm::dvec3> places{cruising.course.front(), relative()};
	double hardest = 0.0;
	double coasting = std::numeric_limits<double>::infinity();
	double midSpeed = 0.0;
	const std::uint64_t midway = departure + ((started[0].arrivalStep - departure) / 2);
	while (fixture.world.Clock().Step() + 1 < started[0].arrivalStep)
	{
		fixture.Run(1);
		places.push_back(relative());
		const std::size_t last = places.size() - 1;
		const double acceleration =
		    glm::length(places[last] - (2.0 * places[last - 1]) + places[last - 2]) / (kStep * kStep);
		hardest = std::max(hardest, acceleration);
		if (fixture.world.Clock().Step() == midway)
		{
			coasting = acceleration;
			midSpeed = glm::length(places[last] - places[last - 1]) / kStep;
		}
	}
	const Transit::ThrustSeconds thrust = Transit::Thrust(kCoasting, started[0].metres);
	const double top = (double{kCoasting.boostAcceleration} * thrust.boost) +
	                   (double{kCoasting.sustainedAcceleration} * thrust.sustained);
	MESSAGE(
	    "Highport to Pell, thrusting a quarter of the time: "
	    << started[0].metres / 1000.0 << " km in " << started[0].seconds << " s, hardest " << hardest
	    << " m/s², coasting at " << midSpeed << " m/s with " << coasting << " m/s²");
	CHECK(hardest <= double{kCoasting.boostAcceleration} * 1.001);
	CHECK(coasting < 1.0);

	// The speed it reached, slowed to the whole steps it was given.
	CHECK(midSpeed == doctest::Approx(top * least / started[0].seconds).epsilon(1.0e-4));
	CHECK(glm::distance(places.back(), cruising.course.back()) < 10.0);

	fixture.Run(1);
	CHECK(registry.get<Jettison::Physics::LocalFrame>(ship).body == "Pell");
	CHECK(registry.get<Jettison::Physics::Velocity>(ship) == Jettison::Physics::Velocity{});
}


TEST_CASE("Cruise: a world saved mid-cruise arrives as the one saved does")
{
	Fixture fixture;
	const entt::entity ship = fixture.Ship("Ostra Highport", {0.0, 0.0, 3000.0});
	SUBCASE("on a profile")
	{
		fixture.world.Registry().replace<Manifest::Space::CruiseDrive>(
		    ship, Manifest::Space::CruiseDrive{.profile = kHard});
	}
	SUBCASE("coasting")
	{
		fixture.world.Registry().replace<Manifest::Space::CruiseDrive>(
		    ship, Manifest::Space::CruiseDrive{.profile = kCoasting});
	}
	fixture.Run(10);
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	fixture.Run(1);
	const std::uint64_t arrival =
	    fixture.world.Registry().get<Manifest::Space::Cruising>(ship).arrivalStep;

	// Half-way, where a coasting profile is coasting.
	fixture.Run((arrival - fixture.world.Clock().Step()) / 2);
	const std::vector<std::byte> midway = fixture.world.Save();
	fixture.Run(arrival - fixture.world.Clock().Step() + 30);
	const std::uint64_t arrived = fixture.world.StateHash();

	Fixture loaded;
	loaded.world.Load(midway);
	REQUIRE(loaded.world.Registry().all_of<Manifest::Space::Cruising>(ship));
	loaded.Run(arrival - loaded.world.Clock().Step() + 30);
	CHECK(loaded.world.Registry().get<Jettison::Physics::LocalFrame>(ship).body == "Pell");
	CHECK(loaded.world.StateHash() == arrived);
}


TEST_CASE(
    "Cruise: a cruise fast-forwarded arrives as the one stepped does, past a ship parked on rails")
{
	Fixture forwarded;
	Fixture stepped;
	for (Fixture *fixture : {&forwarded, &stepped})
	{
		fixture->Ship("Ostra Highport", {0.0, 0.0, 3000.0});

		// Kinematic on Highport's rails, at an offset no coordinate's spacing divides.
		entt::registry &registry = fixture->world.Registry();
		const entt::entity parked = registry.create();
		const Jettison::Physics::OnRails rails{.body = "Ostra Highport", .offset = {40.3, 0.7, -12.1}};
		registry.emplace<Jettison::Sim::Transform>(
		    parked, fixture->physics.RailsPlace(kHalcyon, rails, fixture->world.Clock().Now()));
		registry.emplace<Jettison::Sim::InFrame>(parked, kHalcyon);
		registry.emplace<Jettison::Physics::LocalFrame>(parked, "Ostra Highport");
		registry.emplace<Jettison::Physics::OnRails>(parked, rails);
		registry.emplace<Jettison::Physics::Body>(
		    parked,
		    Jettison::Physics::Body{.kind = Jettison::Physics::BodyKind::Kinematic, .shape = "hull"});
		fixture->Run(10);
	}
	const entt::entity ship = forwarded.world.Registry().view<Manifest::Space::CruiseDrive>().front();
	forwarded.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});
	stepped.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Pell"});

	// To the step that places the arrival, then on with the ship in physics again; each stop's state
	// as stepping leaves it.
	std::uint64_t end = std::numeric_limits<std::uint64_t>::max();
	int calls = 0;
	while (forwarded.world.Clock().Step() < end)
	{
		forwarded.world.FastForward(end);
		++calls;
		if (const auto *cruising = forwarded.world.Registry().try_get<Manifest::Space::Cruising>(ship);
		    cruising && end == std::numeric_limits<std::uint64_t>::max())
			end = cruising->arrivalStep + 120;
		while (stepped.world.Clock().Step() < forwarded.world.Clock().Step())
			stepped.world.Step();
		REQUIRE(forwarded.world.Save() == stepped.world.Save());
	}
	CHECK(forwarded.world.Registry().get<Jettison::Physics::LocalFrame>(ship).body == "Pell");

	// The cruise's 3,000-odd steps in a handful of calls, and each of the 120 after it run.
	MESSAGE("Highport to Pell and two seconds on in " << calls << " calls");
	CHECK(calls < 140);
}


namespace
{
// Two planets as the trading fixture's system has them (data/systems/system-anvil.yaml), each with a
// station 42,000 km out, and a ship's pulse drive's spheres about them: 100,000 km above each planet
// and about each station, so each end's ball is 142,000 km and its crossing 184,000 km (D-108).
Manifest::Space::StarSystem Anvil()
{
	using enum Manifest::Space::BodyKind;
	Manifest::Space::StarSystem system;
	system.name = "Anvil";
	system.bodies = {
	    AiTests::MakeBody("Star", Star, "", 6.957e8, 1.32712440018e20),
	    AiTests::MakeBody("Ore", Planet, "Star", 6.371e6, 3.986004418e14, {1.496e11, 0.0167}),
	    AiTests::MakeBody("Ore Highport", Station, "Ore", 763.0, 0.0, {4.2e7}),
	    AiTests::MakeBody(
	        "Ore Vale", Station, "Ore", 763.0, 0.0, {4.2e7, 0.0, 0.0, 0.0, 0.0, 1.25663706}),
	    AiTests::MakeBody(
	        "Brine", Planet, "Star", 3.3895e6, 4.282837e13,
	        {2.279e11, 0.0934, 0.0, 0.0, 0.0, 3.14159265}),
	    AiTests::MakeBody("Brine Highport", Station, "Brine", 763.0, 0.0, {4.2e7}),
	};
	Manifest::Space::Resolve(system);
	return system;
}

const std::uint64_t kAnvil = Jettison::Sim::FrameId("Anvil");
constexpr double kPulseRadius = 1.0e8;
constexpr double kPulseClearance = 1.84e8;
constexpr Transit::MotionProfile kPulse{19.6133f, 7200.0f, 4.903325f};

Manifest::Space::CruiseDrive PulseDrive(float thrustShare)
{
	Manifest::Space::CruiseDrive drive{.profile = kPulse};
	drive.profile->thrustShare = thrustShare;
	drive.departureClearance = kPulseClearance;
	drive.arrivalClearance = kPulseClearance;
	drive.crossingAcceleration = Manifest::Space::CruiseTravel::kCrossingAcceleration;
	drive.keepouts = {
	    {"Ore", 6.371e6 + kPulseRadius},
	    {"Ore Highport", kPulseRadius},
	    {"Ore Vale", kPulseRadius},
	    {"Brine", 3.3895e6 + kPulseRadius},
	    {"Brine Highport", kPulseRadius}};
	return drive;
}


// A world with cruising in Anvil alone.
struct AnvilFixture
{
	Jettison::Sim::SimWorld world{kStep, 7};
	Jettison::Physics::PhysicsWorld &physics;
	Manifest::Space::LaneTravel &lanes;
	Manifest::Space::CruiseTravel &cruises;
	const Manifest::Space::StarSystem system = Anvil();


	AnvilFixture()
	    : physics{Fixture::Setup(world)}, lanes{Manifest::Space::LaneTravel::Attach(world, physics)},
	      cruises{Manifest::Space::CruiseTravel::Attach(world, physics)}
	{
		Jettison::Physics::ShapeDefinition hull;
		hull.halfExtents = {3.2f, 0.9f, 4.9f};
		physics.DefineShape("hull", hull);
		Manifest::Space::DefineStarSystem(physics, system);
		lanes.DefineStarSystem(system);
		cruises.DefineStarSystem(system);
	}


	glm::dvec3 Where(std::string_view body, double time) const
	{
		return Manifest::Space::EvaluateBody(system, static_cast<std::size_t>(system.Find(body)), time)
		    .metres;
	}


	// A ship 3 km off Ore's highport in its frame, on the drive.
	entt::entity Ship(const Manifest::Space::CruiseDrive &drive)
	{
		entt::registry &registry = world.Registry();
		const entt::entity ship = registry.create();
		registry.emplace<Jettison::Sim::Transform>(
		    ship, Jettison::Sim::Transform{
		              Where("Ore Highport", world.Clock().Now()) + glm::dvec3{0.0, 0.0, 3000.0},
		              glm::dquat{1.0, 0.0, 0.0, 0.0}});
		registry.emplace<Jettison::Sim::InFrame>(ship, kAnvil);
		registry.emplace<Jettison::Physics::LocalFrame>(ship, std::string{"Ore Highport"});
		Jettison::Physics::Body body;
		body.kind = Jettison::Physics::BodyKind::Dynamic;
		body.shape = "hull";
		body.mass = 20000.0f;
		registry.emplace<Jettison::Physics::Body>(ship, body);
		registry.emplace<Jettison::Physics::Velocity>(ship);
		registry.emplace<Jettison::Physics::FlightControls>(ship);
		registry.emplace<Manifest::Space::CruiseDrive>(ship, drive);
		return ship;
	}
};


// Whether a cruise's drive thrusts as the step ending at that count ends, as its course is covered.
bool Thrusting(const Manifest::Space::Cruising &cruising, std::uint64_t ending)
{
	const double length = Transit::Length(cruising.course);
	const double departure = std::min(cruising.departureClearance, length / 2.0);
	const double arrival = std::min(cruising.arrivalClearance, length / 2.0);
	const Transit::MotionProfile thrusters{15.0f, 0.0f, 15.0f};
	const double driven = length - (departure + arrival);
	const double across = Transit::Seconds(thrusters, departure);
	const double driving = Transit::Seconds(*cruising.profile, driven);
	const double seconds = across + Transit::Seconds(thrusters, arrival) + driving;
	const double into = static_cast<double>(ending - cruising.departureStep) /
	                        static_cast<double>(cruising.arrivalStep - cruising.departureStep) *
	                        seconds -
	                    across;
	const Transit::ThrustSeconds thrust = Transit::Thrust(*cruising.profile, driven);
	const double each = thrust.boost + thrust.sustained;
	return into >= 0.0 && into < driving && (into < each || into > driving - each);
}
} // namespace


TEST_CASE("Cruise: a pulse ship never thrusts within a sphere nor comes near a body, from any phase")
{
	const auto day = [](double days) { return static_cast<std::uint64_t>(days * 86400.0 / kStep); };
	for (const auto [start, shareBps] :
	     {std::pair{0.0, 2500}, std::pair{460.0, 20}, std::pair{751.0, 1000}, std::pair{200.0, 10000}})
	{
		CAPTURE(start);
		CAPTURE(shareBps);
		AnvilFixture fixture;
		while (fixture.world.Clock().Step() < day(start))
			fixture.world.FastForward(day(start));
		const entt::entity ship = fixture.Ship(PulseDrive(static_cast<float>(shareBps) / 10000.0f));
		for (int step = 0; step < 5; ++step)
			fixture.world.Step();
		fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Brine Highport"});
		fixture.world.Step();
		const entt::registry &registry = fixture.world.Registry();
		REQUIRE(registry.all_of<Manifest::Space::Cruising>(ship));
		const Manifest::Space::Cruising cruising = registry.get<Manifest::Space::Cruising>(ship);
		CHECK(cruising.departureBody == "Ore");
		CHECK(cruising.departureClearance >= kPulseClearance);
		CHECK(cruising.arrivalClearance >= kPulseClearance);

		// Each minute of the whole cruise, where the bodies then are.
		const std::uint64_t minute = std::uint64_t{60} * 60;
		double nearestSphere = std::numeric_limits<double>::infinity();
		double nearestBody = std::numeric_limits<double>::infinity();
		std::uint64_t thrusting = 0;
		for (std::uint64_t at = cruising.departureStep + minute; at < cruising.arrivalStep;
		     at += minute)
		{
			while (fixture.world.Clock().Step() < at)
				fixture.world.FastForward(at);
			const double now = fixture.world.Clock().Now();
			const glm::dvec3 place = registry.get<Jettison::Sim::Transform>(ship).position;
			for (const Manifest::Space::Body &body : fixture.system.bodies)
				if (body.kind != Manifest::Space::BodyKind::Station)
					nearestBody = std::min(
					    nearestBody, glm::distance(place, fixture.Where(body.name, now)) - body.radius -
					                     Manifest::Space::CruiseTravel::kCourseClearance);
			if (!Thrusting(cruising, at))
				continue;
			++thrusting;
			for (const Manifest::Space::Keepout &keepout : PulseDrive(1.0f).keepouts)
				nearestSphere = std::min(
				    nearestSphere,
				    glm::distance(place, fixture.Where(keepout.body, now)) - keepout.radius);
		}
		MESSAGE(
		    "From day " << start << " at " << shareBps << " basis points: "
		                << static_cast<double>(cruising.arrivalStep - cruising.departureStep) * kStep /
		                       86400.0
		                << " days, crossings of " << cruising.departureClearance / 1000.0 << " and "
		                << cruising.arrivalClearance / 1000.0 << " km, " << thrusting
		                << " minutes thrusting, nearest a sphere " << nearestSphere / 1000.0
		                << " km and a body " << nearestBody / 1000.0 << " km");
		CHECK(thrusting > 0);
		CHECK(nearestSphere >= 0.0);
		CHECK(nearestBody >= 0.0);
	}
}


TEST_CASE("Cruise: a pulse ship crosses out of Ore's ball and into Brine's on thrusters, in each frame")
{
	AnvilFixture fixture;
	const entt::entity ship = fixture.Ship(PulseDrive(0.1f));
	for (int step = 0; step < 5; ++step)
		fixture.world.Step();
	fixture.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Brine Highport"});
	fixture.world.Step();
	const entt::registry &registry = fixture.world.Registry();
	REQUIRE(registry.all_of<Manifest::Space::Cruising>(ship));
	const Manifest::Space::Cruising cruising = registry.get<Manifest::Space::Cruising>(ship);
	const double t0 = fixture.world.Clock().Now() - kStep;
	const Transit::MotionProfile thrusters{15.0f, 0.0f, 15.0f};
	const auto at = [&](std::uint64_t step)
	{
		while (fixture.world.Clock().Step() < step)
			fixture.world.FastForward(step);
		return registry.get<Jettison::Sim::Transform>(ship).position;
	};

	// Its departure crossing, 184,000 km at 15 m/s² from rest to rest, about 1.9 hours: a straight
	// line from where it set out, as Ore moves, so it keeps the planet's motion and none of the star's
	// pull on it.
	const double across = Transit::Seconds(thrusters, cruising.departureClearance);
	const glm::dvec3 start = at(cruising.departureStep + 1) - fixture.Where("Ore", t0 + kStep);
	const auto steps = [](double seconds) { return static_cast<std::uint64_t>(seconds / kStep); };
	for (const double share : {0.25, 0.5, 0.75})
	{
		const std::uint64_t when = cruising.departureStep + steps(share * across);
		const glm::dvec3 relative = at(when) - fixture.Where("Ore", fixture.world.Clock().Now());
		CHECK(
		    glm::length(relative - start) ==
		    doctest::Approx(Transit::Covered(thrusters, cruising.departureClearance, share * across))
		        .epsilon(0.01));
	}

	// Out of every sphere about Ore as the crossing ends, where the drive takes over.
	const glm::dvec3 out = at(cruising.departureStep + steps(across));
	for (const Manifest::Space::Keepout &keepout : PulseDrive(1.0f).keepouts)
		if (keepout.body.starts_with("Ore"))
			CHECK(
			    glm::distance(out, fixture.Where(keepout.body, fixture.world.Clock().Now())) >
			    keepout.radius);

	// Its arrival crossing in Brine's frame: the last of it a straight line to where it arrives,
	// relative to Brine, and that place the course's end, Brine Highport's standoff.
	const double into = Transit::Seconds(thrusters, cruising.arrivalClearance);
	const std::uint64_t arriving = cruising.arrivalStep - steps(into);
	const glm::dvec3 entered = at(arriving) - fixture.Where("Brine", fixture.world.Clock().Now());
	const glm::dvec3 middle = at(cruising.arrivalStep - steps(into / 2.0)) -
	                          fixture.Where("Brine", fixture.world.Clock().Now());
	const glm::dvec3 last =
	    at(cruising.arrivalStep - 1) - fixture.Where("Brine", fixture.world.Clock().Now());
	CHECK(glm::length(entered - last) == doctest::Approx(cruising.arrivalClearance).epsilon(0.01));
	CHECK(
	    glm::length(glm::cross(glm::normalize(middle - entered), glm::normalize(last - entered))) <
	    1.0e-3);
	CHECK(
	    glm::distance(
	        at(cruising.arrivalStep + 1),
	        fixture.Where("Brine Highport", fixture.world.Clock().Now())) <
	    Manifest::Space::CruiseTravel::kArrivalStandoff * 2.0);
}


TEST_CASE("Cruise: a pulse cruise saved mid-crossing and mid-coast flies on as the unsaved does")
{
	AnvilFixture unsaved;
	const entt::entity ship = unsaved.Ship(PulseDrive(0.1f));
	for (int step = 0; step < 5; ++step)
		unsaved.world.Step();
	unsaved.world.Commands().Push(Manifest::Space::Cruise{{}, ship, "Brine Highport"});
	unsaved.world.Step();
	const Manifest::Space::Cruising cruising =
	    unsaved.world.Registry().get<Manifest::Space::Cruising>(ship);
	const auto forward = [](AnvilFixture &fixture, std::uint64_t step)
	{
		while (fixture.world.Clock().Step() < step)
			fixture.world.FastForward(step);
	};
	const std::uint64_t crossing = cruising.departureStep + 3600 * 60;
	const std::uint64_t coasting = (cruising.departureStep + cruising.arrivalStep) / 2;
	const std::uint64_t end = cruising.arrivalStep + 60;
	for (const std::uint64_t saved : {crossing, coasting})
	{
		CAPTURE(saved);
		AnvilFixture original;
		original.world.Load(unsaved.world.Save());
		forward(original, saved);
		AnvilFixture loaded;
		loaded.world.Load(original.world.Save());
		REQUIRE(loaded.world.Registry().all_of<Manifest::Space::Cruising>(ship));

		// Stepped a minute, then passed over to the end, each alike.
		for (int step = 0; step < 3600; ++step)
		{
			original.world.Step();
			loaded.world.Step();
		}
		CHECK(loaded.world.Save() == original.world.Save());
		forward(original, end);
		forward(loaded, end);
		CHECK(loaded.world.Save() == original.world.Save());
		CHECK(
		    loaded.world.Registry().get<Jettison::Physics::LocalFrame>(ship).body == "Brine Highport");
	}
}


TEST_CASE("Cruise: a drive still thrusting within a sphere once its rounds are spent is refused")
{
	const Manifest::Space::StarSystem system = Anvil();
	const glm::dvec3 from = Manifest::Space::EvaluateBody(
	                            system, static_cast<std::size_t>(system.Find("Ore Highport")), 0.0)
	                            .metres +
	                        glm::dvec3{0.0, 0.0, 3000.0};

	// A sphere about the star the whole course lies in: the drive may not thrust anywhere.
	Manifest::Space::CruiseDrive drive = PulseDrive(1.0f);
	drive.keepouts.push_back({"Star", 1.0e12});
	const Manifest::Space::Settlement refused = Manifest::Space::SettleCruise(
	    system, drive, from, "Ore Highport", "Brine Highport", 0.0, kStep, 0);
	CHECK_FALSE(refused.cruise);
	CHECK(refused.refusal == Manifest::Space::Reason::InsideRadius);

	// With its rounds, each crossing lengthened past the sphere, to half the course: thrusters all the
	// way, the drive's metres none.
	const Manifest::Space::Settlement settled = Manifest::Space::SettleCruise(
	    system, drive, from, "Ore Highport", "Brine Highport", 0.0, kStep);
	REQUIRE(settled.cruise);
	CHECK(settled.cruise->DriveMetres() == 0.0);

	// A sphere about no body of the system.
	drive.keepouts.push_back({"Nowhere", 1.0});
	CHECK(
	    Manifest::Space::SettleCruise(system, drive, from, "Ore Highport", "Brine Highport", 0.0, kStep)
	        .refusal == Manifest::Space::Reason::NoSuchBody);
}
