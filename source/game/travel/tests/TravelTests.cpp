#include "TestHelpers.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>

#include <jettison/dock/Docks.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/host/Session.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/space/Cruise.h>

#include <manifest/company/Company.h>
#include <manifest/host/Start.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/State.h>

using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
using Manifest::World::Failure;
using Manifest::World::Kind;
using Manifest::World::Leg;
namespace Company = Manifest::Company;
namespace Dock = Jettison::Dock;
namespace Space = Jettison::Space;
namespace Travel = Manifest::Travel;
namespace World = Manifest::World;

namespace
{
const Actor kHost{ActorKind::Administrator, "Host"};
const Actor kAda{ActorKind::Principal, "Ada"};
const Actor kBea{ActorKind::Principal, "Bea"};
constexpr std::string_view kOre = "VenueOreHighport";
constexpr std::string_view kBrine = "VenueBrineHighport";

// Steps at 1/60 s: a journey from Ore to Brine on the power law is a few minutes of cruise and a
// minute or so at each end, so this bounds one with room to spare, failing with its leg rather than
// hanging.
constexpr std::uint64_t kJourneySteps = std::uint64_t{60} * 60 * 12;
constexpr double kStep = 1.0 / 60.0;
constexpr double kGravity = 9.80665;
constexpr double kDay = 86400.0;

// What a ship's drive cruises by: the Manifest mode's profile for its hull, as content gives it, or
// the drive's power law, the time-compressed cruise Astralis keeps (D-088), so that a journey from
// Ore to Brine through every leg is minutes rather than the profile's four and a half days.
enum class Cruise
{
	Profiled,
	Compressed,
};


// The trading scenario as the host starts it, with Ada provisioned: her company's ship on a pad at
// Ore's highport.
struct Hosted
{
	std::unique_ptr<Jettison::Host::Session> session;
	Jettison::Sim::SimWorld &world{session->World()};
	Manifest::Host::Services services{Manifest::Host::HostedServices(*session)};
	const Travel::Fleet *fleet{&services.fleet};

	// From the staged directory, or another source of the same content, such as its pack.
	explicit Hosted(
	    const std::string &content = MANIFEST_MOD_DIRECTORY, Cruise cruise = Cruise::Compressed)
	    : session{Manifest::Host::Start({content, {}, "trading"})}
	{
		REQUIRE(
		    Run(kHost, Company::kProvision, Company::kVersion,
		        Company::Provision{"Ada", "Ada", "Ada Co"}) == Result::Succeeded);
		if (cruise == Cruise::Compressed)
			world.Registry().get<Space::CruiseDrive>(Ship()).profile.reset();
	}

	template <typename T>
	Result Run(const Actor &actor, std::string_view command, std::uint32_t version, const T &payload)
	{
		const std::uint64_t sequence = services.requests.NextSequence(actor);
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{version});
		REQUIRE(
		    services.requests.Admit(world, actor, sequence, std::string{command}, version, bytes)
		        .status == Receipt::Status::Queued);
		Step();
		const auto completed = services.requests.Outcome(actor, sequence);
		REQUIRE(completed);
		return completed->result;
	}

	Result Go(const Actor &actor, std::string_view venueId, std::string shipId = "Ship1")
	{
		return Run(
		    actor, Travel::kTravel, Travel::kVersion,
		    Travel::Travel{std::move(shipId), std::string{venueId}});
	}

	Result Load(std::string_view venueId, std::uint64_t totes = 1)
	{
		return Run(
		    kAda, Company::kLoad, Company::kVersion,
		    Company::Move{"Ship1", std::string{venueId}, "ItemFurs", totes});
	}

	void Step()
	{
		world.Step();
		(void)world.Events().Take();
	}

	entt::entity Ship() const
	{
		return services.identities.Find(Kind::Ship, "Ship1");
	}

	const World::Ship &Record() const
	{
		return world.Registry().get<World::Ship>(Ship());
	}

	const World::Journey &Journey() const
	{
		return world.Registry().get<World::Journey>(Ship());
	}

	// Steps until the journey ends, each leg it passes through recorded once, in order.
	std::vector<Leg> RunJourney()
	{
		std::vector<Leg> legs{Journey().leg};
		for (std::uint64_t step = 0; step < kJourneySteps && !Journey().Ended(); ++step)
		{
			Step();
			if (Journey().leg != legs.back())
				legs.push_back(Journey().leg);
		}
		INFO("the journey stopped at leg " << static_cast<int>(Journey().leg));
		REQUIRE(Journey().Ended());
		return legs;
	}
};
} // namespace


TEST_CASE("Provisioning parks the starter on a pad of its home venue, docked as the engine has it")
{
	Hosted hosted;
	const entt::registry &registry = hosted.world.Registry();
	CHECK(hosted.Record().dockedAt == kOre);
	const auto *docking = registry.try_get<Dock::Docking>(hosted.Ship());
	REQUIRE(docking);
	CHECK(docking->stage == Dock::Stage::Docked);
	CHECK(Dock::IsDocked(registry, hosted.Ship(), docking->station));
	// The skiff takes the smallest pad it fits, a skiff pad, the lowest numbered: the thirteenth.
	CHECK(registry.get<Dock::Pad>(docking->pad).padClass == "skiff");
	CHECK(registry.get<Dock::Pad>(docking->pad).number == 13);
	CHECK(
	    registry.get<Jettison::Sim::ControlledBy>(hosted.Ship()).controller ==
	    Travel::ControllerOf("Company1"));
}


TEST_CASE("A ship travels from Ore to Brine through the engine's departure, cruise and docking")
{
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	// Fixture bootstrap: two totes of furs in the company's warehouse at Ore, loaded through the
	// command.
	registry.get<World::Stock>(hosted.services.identities.Find(Kind::Company, "Company1"))
	    .holdings.push_back({std::string{kOre}, "ItemFurs", 2, 0});
	REQUIRE(hosted.Load(kOre, 2) == Result::Succeeded);
	const World::Cargo cargo = registry.get<World::Cargo>(hosted.Ship());

	// In the step the command runs in, the step's travel lets the ship go of its pad.
	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	CHECK(hosted.Journey().leg == Leg::Departing);
	CHECK(registry.get<Dock::Docking>(hosted.Ship()).stage == Dock::Stage::Leaving);
	CHECK(hosted.Record().dockedAt.empty());
	// Off its pad, it lets go of Ore's life support and draws on its own (D-046).
	const auto &support = [&]() -> const World::LifeSupport &
	{ return registry.get<World::LifeSupport>(hosted.Ship()); };
	CHECK(support().connectedTo.empty());
	CHECK(support().tanks[0] < 10.0);
	CHECK(hosted.Load(kOre) == Result::NotDocked);
	CHECK(hosted.Go(kAda, kBrine) == Result::Travelling);
	const std::uint64_t left = hosted.world.Clock().Step();

	const std::vector<Leg> legs = hosted.RunJourney();
	CHECK(
	    legs ==
	    std::vector<Leg>{Leg::Departing, Leg::Clearing, Leg::Cruising, Leg::Arriving, Leg::Arrived});
	CHECK(hosted.Journey().failure == Failure::None);
	CHECK(hosted.Record().dockedAt == kBrine);
	CHECK(registry.get<World::Cargo>(hosted.Ship()) == cargo);
	// Docked, it is connected at Brine in the same step, having drawn a kilogram a day for the steps
	// since it let go, from its first tank, and an occupant-day a day from its cartridge.
	CHECK(support().connectedTo == kBrine);
	const double days = static_cast<double>(hosted.world.Clock().Step() - left + 1) * kStep / kDay;
	CHECK(support().tanks[0] == doctest::Approx(10.0 - days).epsilon(1e-6));
	CHECK(support().tanks[1] == 10.0);
	CHECK(support().cartridges[0] == doctest::Approx(30.0 - days).epsilon(1e-6));
	const World::LifeSupport arrived = support();
	hosted.Step();
	CHECK(support() == arrived);
	CHECK(hosted.Load(kOre) == Result::WrongVenue);
	CHECK(
	    hosted.Run(
	        kAda, Company::kUnload, Company::kVersion,
	        Company::Move{"Ship1", std::string{kBrine}, "ItemFurs", 2}) == Result::Succeeded);
	CHECK(hosted.Go(kAda, kBrine) == Result::AlreadyThere);
}


TEST_CASE(
    "The hosted step runs the market, then the engine's movement, then travel, as the plan orders")
{
	Hosted hosted;
	const Jettison::Ecs::Scheduler &scheduler = hosted.world.Scheduler();
	CHECK(
	    scheduler.Order(Jettison::Ecs::TickPhase::Simulation) ==
	    std::vector<std::string>{
	        "manifest.expiry", "manifest.economy", "manifest.brokers", "spatial", "steering", "flight",
	        "physics", "dock", "lanes", "cruise", std::string{Travel::kSystem},
	        std::string{Company::kLifeSupportSystem}, std::string{Company::kBerthSystem},
	        std::string{Manifest::Trade::kObserve}});
	CHECK(scheduler.Ambiguities(Jettison::Ecs::TickPhase::Simulation).empty());
}


TEST_CASE("Travel is refused, writing nothing, for what it cannot send")
{
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	// Fixture bootstrap: Bea, a principal with no company.
	registry.emplace<World::Principal>(
	    hosted.services.identities.Create(registry, Kind::Principal, "Bea"), "Bea");
	CHECK(hosted.Go(kAda, kBrine, "Ship9") == Result::UnknownTarget);
	CHECK(hosted.Go(kAda, "VenueNowhere") == Result::UnknownTarget);
	CHECK(hosted.Go(kBea, kBrine) == Result::NotAuthorised);
	CHECK(hosted.Go(kAda, kOre) == Result::AlreadyThere);
	CHECK_FALSE(registry.all_of<World::Journey>(hosted.Ship()));
	CHECK(hosted.Record().dockedAt == kOre);
	CHECK(registry.get<Dock::Docking>(hosted.Ship()).stage == Dock::Stage::Docked);
}


TEST_CASE("A journey whose cruise the engine refuses fails, its ship left where it is with its cargo")
{
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	// Fixture bootstrap: a tote of furs loaded, and the ship's drive taken out, which the engine's
	// cruise refuses a ship for.
	registry.get<World::Stock>(hosted.services.identities.Find(Kind::Company, "Company1"))
	    .holdings.push_back({std::string{kOre}, "ItemFurs", 1, 0});
	REQUIRE(hosted.Load(kOre) == Result::Succeeded);
	registry.remove<Space::CruiseDrive>(hosted.Ship());
	const World::Cargo cargo = registry.get<World::Cargo>(hosted.Ship());

	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	CHECK(hosted.RunJourney() == std::vector<Leg>{Leg::Departing, Leg::Clearing, Leg::Failed});
	CHECK(hosted.Journey().failure == Failure::CruiseRefused);
	CHECK(hosted.Record().dockedAt.empty());
	CHECK(registry.get<World::Cargo>(hosted.Ship()) == cargo);
	// Ended, it gives way to a new journey, back to the pads it left.
	CHECK(hosted.Go(kAda, kOre) == Result::Succeeded);
}


TEST_CASE("A ship held on its pad while cargo crosses the deck fails to depart, keeping its dock")
{
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	// Fixture bootstrap: crates crossing the deck, which Manifest's own transfers never leave.
	REQUIRE(Dock::Carry(registry, hosted.Ship(), 4));
	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	CHECK(hosted.Journey().leg == Leg::Failed);
	CHECK(hosted.Journey().failure == Failure::DepartureRefused);
	CHECK(hosted.Record().dockedAt == kOre);
	CHECK(registry.get<Dock::Docking>(hosted.Ship()).stage == Dock::Stage::Docked);
	// A failed journey is over, so another may start once the deck is clear.
	while (Dock::ToCarry(registry, hosted.Ship()) != 0)
		hosted.Step();
	CHECK(hosted.Go(kAda, kBrine) == Result::Succeeded);
	CHECK(hosted.Journey().leg == Leg::Departing);
}


TEST_CASE("At a station with every pad taken, a ship waits in its queue, cargo aboard, until one frees")
{
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	const Travel::Fleet &fleet = *hosted.fleet;
	// Fixture bootstrap: every pad at Brine held by a ship of no company's.
	std::vector<entt::entity> parked;
	for (entt::entity pad = fleet.FreePad(registry, kBrine, "HullLoneTraderStarter"); pad != entt::null;
	     pad = fleet.FreePad(registry, kBrine, "HullLoneTraderStarter"))
	{
		const entt::entity other = registry.create();
		fleet.Park(hosted.world, other, pad, "HullLoneTraderStarter", "Company9");
		parked.push_back(other);
	}
	REQUIRE(parked.size() == 20);

	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	std::uint64_t step = 0;
	for (; step < kJourneySteps && hosted.Journey().leg != Leg::Arriving; ++step)
		hosted.Step();
	REQUIRE(hosted.Journey().leg == Leg::Arriving);
	// Where its cruise left it off the station, still: nothing flies a ship that waits.
	const entt::entity brine = fleet.StationOf(registry, kBrine);
	const auto offStation = [&]
	{
		return registry.get<Jettison::Sim::Transform>(hosted.Ship()).position -
		       registry.get<Jettison::Sim::Transform>(brine).position;
	};
	const glm::dvec3 waiting = offStation();
	for (int wait = 0; wait < 600; ++wait)
		hosted.Step();
	CHECK(glm::distance(offStation(), waiting) < 5.0);
	CHECK(registry.get<Dock::Docking>(hosted.Ship()).stage == Dock::Stage::Queued);
	CHECK(hosted.Record().dockedAt.empty());
	CHECK(hosted.Load(kBrine) == Result::NotDocked);

	// One leaves, and the queue's first takes its pad.
	// One leaves, flying on and out of the way once it has given its pad up, and the queue's first
	// takes the pad.
	hosted.world.Apply(Dock::ReleasePad{{}, parked.front(), true});
	while (registry.all_of<Dock::Docking>(parked.front()))
		hosted.Step();
	registry.destroy(parked.front());
	hosted.RunJourney();
	CHECK(hosted.Journey().failure == Failure::None);
	CHECK(hosted.Journey().leg == Leg::Arrived);
	CHECK(hosted.Record().dockedAt == kBrine);
}


TEST_CASE("A world saved mid-cruise arrives as the one saved does, sending nothing twice")
{
	Hosted hosted;
	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	while (hosted.Journey().leg != Leg::Cruising)
		hosted.Step();
	for (int step = 0; step < 600; ++step)
		hosted.Step();
	const std::vector<std::byte> saved = hosted.world.Save();

	Hosted loaded;
	loaded.world.Load(saved);
	REQUIRE(loaded.Journey().leg == Leg::Cruising);
	hosted.RunJourney();
	loaded.RunJourney();
	CHECK(loaded.world.Clock().Step() == hosted.world.Clock().Step());
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.Record().dockedAt == kBrine);
}


TEST_CASE("On the Manifest mode's manned profile, Ore to Brine is days, at 2 g and then 1 g (D-090)")
{
	Hosted hosted{MANIFEST_MOD_DIRECTORY, Cruise::Profiled};
	entt::registry &registry = hosted.world.Registry();
	const Space::CruiseDrive drive = registry.get<Space::CruiseDrive>(hosted.Ship());
	REQUIRE(drive.profile);
	CHECK(drive.profile->boostAcceleration == doctest::Approx(2.0 * kGravity));
	CHECK(drive.profile->boostSeconds == doctest::Approx(4.0 * 3600.0));
	CHECK(drive.profile->sustainedAcceleration == doctest::Approx(kGravity));

	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	for (std::uint64_t step = 0; step < kJourneySteps && hosted.Journey().leg != Leg::Cruising; ++step)
		hosted.Step();
	REQUIRE(hosted.Journey().leg == Leg::Cruising);
	const Space::Cruising cruising = registry.get<Space::Cruising>(hosted.Ship());
	CHECK(cruising.profile == drive.profile);
	double metres = 0.0;
	for (std::size_t point = 1; point < cruising.course.size(); ++point)
		metres += glm::distance(cruising.course[point], cruising.course[point - 1]);
	const double seconds = static_cast<double>(cruising.arrivalStep - cruising.departureStep) * kStep;
	const double least = Space::CruiseSeconds(drive, metres);
	MESSAGE("Ore to Brine: " << metres / 1.0e9 << " million km in " << seconds / kDay << " days");
	CHECK(seconds >= least);
	CHECK(seconds < least + (4.0 * kStep));
	CHECK(seconds > 4.0 * kDay);
	CHECK(seconds < 5.0 * kDay);

	// Its first ten seconds out at 2 g, the star it is carried by still.
	const auto place = [&] { return registry.get<Jettison::Sim::Transform>(hosted.Ship()).position; };
	REQUIRE(cruising.carrier == "StarAnvil");
	for (int step = 0; step < 600; ++step)
		hosted.Step();
	const glm::dvec3 before = place();
	hosted.Step();
	const glm::dvec3 now = place();
	hosted.Step();
	const glm::dvec3 after = place();
	CHECK(glm::distance(now, after) / kStep == doctest::Approx(2.0 * kGravity * 10.0).epsilon(0.01));
	CHECK(
	    glm::length(after - (2.0 * now) + before) / (kStep * kStep) ==
	    doctest::Approx(2.0 * kGravity).epsilon(0.01));

	// And along its course, as the engine places it, 2 g until two hours out and 1 g after.
	const auto acceleration = [&](double at)
	{
		const auto covered = [&](double time)
		{ return Space::CruiseCovered(*drive.profile, metres, time * least / seconds); };
		return (covered(at + 1.0) - (2.0 * covered(at)) + covered(at - 1.0));
	};
	CHECK(acceleration(7190.0) == doctest::Approx(2.0 * kGravity).epsilon(1.0e-3));
	CHECK(acceleration(7210.0) == doctest::Approx(kGravity).epsilon(1.0e-3));
	CHECK(acceleration(seconds / 3.0) == doctest::Approx(kGravity).epsilon(1.0e-3));
	CHECK(acceleration(seconds - 3600.0) == doctest::Approx(-2.0 * kGravity).epsilon(1.0e-3));
}


TEST_CASE("A world saved mid-cruise on the manned profile flies on as the one saved does")
{
	Hosted hosted{MANIFEST_MOD_DIRECTORY, Cruise::Profiled};
	REQUIRE(hosted.Go(kAda, kBrine) == Result::Succeeded);
	while (hosted.Journey().leg != Leg::Cruising)
		hosted.Step();
	for (int step = 0; step < 600; ++step)
		hosted.Step();
	const std::vector<std::byte> saved = hosted.world.Save();

	Hosted loaded{MANIFEST_MOD_DIRECTORY, Cruise::Profiled};
	loaded.world.Load(saved);
	REQUIRE(loaded.Journey().leg == Leg::Cruising);
	for (int step = 0; step < 6000; ++step)
	{
		hosted.Step();
		loaded.Step();
	}
	const auto &flown = hosted.world.Registry().get<Space::Cruising>(hosted.Ship());
	CHECK(loaded.world.Registry().get<Space::Cruising>(loaded.Ship()) == flown);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
}


TEST_CASE("The compact single-seat takes a skiff pad too")
{
	Hosted hosted;
	const entt::registry &registry = hosted.world.Registry();
	const entt::entity pad = hosted.fleet->FreePad(registry, kOre, "HullCompactSingle");
	REQUIRE(pad != entt::null);
	CHECK(registry.get<Dock::Pad>(pad).padClass == "skiff");
	// The long-haul trader, a frigate, only a destroyer pad.
	CHECK(
	    registry.get<Dock::Pad>(hosted.fleet->FreePad(registry, kOre, "HullTraderLongHaul")).padClass ==
	    "destroyer");
}


TEST_CASE("A journey from the pack is the one from the directory, step for step")
{
	Hosted directory;
	Hosted pack{MANIFEST_MOD_PACK};
	REQUIRE(directory.world.StateHash() == pack.world.StateHash());
	REQUIRE(directory.Go(kAda, kBrine) == Result::Succeeded);
	REQUIRE(pack.Go(kAda, kBrine) == Result::Succeeded);
	directory.RunJourney();
	pack.RunJourney();
	CHECK(pack.world.Clock().Step() == directory.world.Clock().Step());
	CHECK(pack.world.StateHash() == directory.world.StateHash());
}
