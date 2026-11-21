#include "TestHelpers.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/company/Company.h>
#include <manifest/query/Query.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

using namespace Manifest::Company;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Jettison::Sim::SimWorld;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
using Manifest::Query::CompanyView;
using Manifest::Query::Snapshot;
using Manifest::World::Kind;
namespace World = Manifest::World;

namespace
{
const Manifest::Content::ContentSet &Scenario(const std::string &name)
{
	static std::map<std::string, Manifest::Content::ContentSet> loaded;
	auto found = loaded.find(name);
	if (found == loaded.end())
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		found = loaded.emplace(name, Manifest::Content::LoadScenario(files, definitions, name)).first;
	}
	return found->second;
}


const Actor kHost{ActorKind::Administrator, "Host"};
const Actor kAda{ActorKind::Principal, "Ada"};
const Actor kBea{ActorKind::Principal, "Bea"};


// A world as the host makes one, in its scenario's initial state. What a player can see is checked
// through their query; the registry is read only for what a snapshot leaves out, and written only
// as fixture bootstrap.
struct Hosted
{
	explicit Hosted(const std::string &scenario = "trading") : Hosted{Scenario(scenario)}
	{
	}

	// On content a test has changed, which must outlive it.
	explicit Hosted(const Manifest::Content::ContentSet &set)
	    : content{set}, identities{(RegisterTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		const Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, content);
		RegisterCommands(world, requests, identities, fleet, content);
		World::CreateInitialState(world, identities, content);
		Manifest::Travel::OpenStations(world, fleet);
	}

	// The request admitted as the actor's next.
	template <typename T> Receipt Admit(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{kVersion});
		return requests.Admit(
		    world, actor, requests.NextSequence(actor), std::string{command}, kVersion, bytes);
	}

	// The request admitted, as the actor's next, and run in the next step.
	template <typename T> Result Run(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		REQUIRE(Admit(actor, command, payload).status == Receipt::Status::Queued);
		world.Step();
		return Outcome(actor, sequence);
	}

	Result Outcome(const Actor &actor, std::uint64_t sequence) const
	{
		const auto completed = requests.Outcome(actor, sequence);
		REQUIRE(completed);
		return completed->result;
	}

	Result Provision(const std::string &principalId)
	{
		return Run(kHost, kProvision, Manifest::Company::Provision{principalId, principalId, "Co"});
	}

	Result Load(const Actor &actor, const Move &move)
	{
		return Run(actor, kLoad, move);
	}

	Result Unload(const Actor &actor, const Move &move)
	{
		return Run(actor, kUnload, move);
	}

	Snapshot Observe(const Actor &actor) const
	{
		return Manifest::Query::Observe(world, identities, actor);
	}

	// The player's own company, as their query shows it.
	CompanyView Seen(const Actor &actor) const
	{
		const Snapshot snapshot = Observe(actor);
		REQUIRE(snapshot.companies.size() == 1);
		return snapshot.companies[0];
	}

	entt::entity Find(Kind kind, const std::string &id) const
	{
		const entt::entity found = identities.Find(kind, id);
		REQUIRE_MESSAGE(found != entt::null, id);
		return found;
	}

	World::Stock &Stock(const std::string &companyId)
	{
		return world.Registry().get<World::Stock>(Find(Kind::Company, companyId));
	}

	World::LifeSupport &Support(const std::string &shipId)
	{
		return world.Registry().get<World::LifeSupport>(Find(Kind::Ship, shipId));
	}

	World::Cargo &Hold(const std::string &shipId)
	{
		return world.Registry().get<World::Cargo>(Find(Kind::Ship, shipId));
	}

	World::Ship &Record(const std::string &shipId)
	{
		return world.Registry().get<World::Ship>(Find(Kind::Ship, shipId));
	}

	Result Service(const Actor &actor, std::string_view command, const std::string &shipId)
	{
		return Run(actor, command, Manifest::Company::Service{shipId});
	}

	const Manifest::Content::ContentSet &content;
	SimWorld world{1.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};


// The company's available totes of the item at the venue, 0 where it holds none.
std::uint64_t AtVenue(const CompanyView &company, std::string_view venueId, std::string_view itemId)
{
	for (const World::Holding &holding : company.holdings)
		if (holding.venueId == venueId && holding.itemId == itemId)
			return holding.available;
	return 0;
}


// The totes of the item aboard the company's ship, 0 where it carries none.
std::uint64_t Aboard(const CompanyView &company, std::string_view shipId, std::string_view itemId)
{
	for (const Manifest::Query::ShipView &ship : company.ships)
		if (ship.shipId == shipId)
			for (const World::Lot &lot : ship.cargo)
				if (lot.itemId == itemId)
					return lot.totes;
	return 0;
}


// The fixture's stock, written between the first two steps as bootstrap: no player command makes it.
void Seed(Hosted &hosted)
{
	hosted.Stock("Company1").holdings = {{"VenueForgeHighport", "ItemCopper", 10, 0}};
	hosted.Stock("Company2").holdings = {{"VenueForgeHighport", "ItemTin", 3, 0}};
}
} // namespace


TEST_CASE("Provisioning gives a player a company, its credits and a ship docked at the starter venue")
{
	Hosted hosted;
	CHECK(hosted.Observe(kAda).companies.empty());
	CHECK(hosted.Provision("Ada") == Result::Succeeded);
	// Its tanks full and its cartridge fresh, once (D-045, D-050), and connected to Ore's life support.
	const World::LifeSupport granted{{10.0, 10.0, 10.0}, {30.0}, "VenueOreHighport"};
	CHECK(
	    hosted.Seen(kAda) ==
	    CompanyView{
	        "Company1",
	        "Co",
	        25000,
	        {},
	        {{"Ship1", "HullLoneTraderStarter", "VenueOreHighport", {}, {}, {}, granted, 160}}});

	// What the snapshot leaves out: the principal. No polity is made and no region governed (D-089).
	const entt::registry &registry = hosted.world.Registry();
	CHECK(registry.get<World::Principal>(hosted.Find(Kind::Principal, "Ada")).displayName == "Ada");
	CHECK(hosted.identities.Find(Kind::Organisation, "Organisation1") == entt::null);
	CHECK(registry.get<World::Region>(hosted.Find(Kind::Region, "RegionOreHighlands"))
	          .governorId.empty());

	// Again under a new sequence, it makes nothing: the ledger answers a retry, this the principal.
	const std::size_t entities = registry.view<const World::Identity>().size();
	CHECK(hosted.Provision("Ada") == Result::AlreadyProvisioned);
	CHECK(hosted.Run(kHost, kProvision, Provision{"", "", ""}) == Result::MalformedPayload);
	CHECK(registry.view<const World::Identity>().size() == entities);

	// Every player starts at the one venue, connected, until its life support's four connections are
	// taken (D-047), and the rejection spends no ID.
	int players = 1;
	while (hosted.Provision(std::format("Player{}", players)) == Result::Succeeded)
		++players;
	CHECK(players == 4);
	CHECK(hosted.Provision(std::format("Player{}", players)) == Result::NoServiceFree);
	CHECK(hosted.Observe(kHost).companies.size() == static_cast<std::size_t>(players));
	CHECK(hosted.identities.Issue(Kind::Company) == std::format("Company{}", players + 1));

	// With connections to spare, until its pads that fit the starter are taken.
	Manifest::Content::ContentSet roomy = Scenario("trading");
	roomy.scenarios.at("trading").venues[0].lifeSupportCapacity = 1000;
	Hosted padded{roomy};
	players = 0;
	while (padded.Provision(std::format("Player{}", players)) == Result::Succeeded)
		++players;
	CHECK(players > 4);
	CHECK(padded.Provision(std::format("Player{}", players)) == Result::NoPadFree);
}


TEST_CASE("A ship on station life support uses none of its own, and off it draws its own in order")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	const World::LifeSupport granted = hosted.Support("Ship1");
	for (int step = 0; step < 10; ++step)
		hosted.world.Step();
	CHECK(hosted.Support("Ship1") == granted);
	// A retried provisioning is refused and restores nothing.
	hosted.Support("Ship1").tanks[0] = 5.0;
	CHECK(hosted.Provision("Ada") == Result::AlreadyProvisioned);
	CHECK(hosted.Support("Ship1").tanks[0] == 5.0);
	hosted.Support("Ship1").tanks[0] = 10.0;

	// Fixture bootstrap: Ada's ship off its pad, as travel leaves it when it lets go.
	hosted.Record("Ship1").dockedAt.clear();
	// An hour of 1 s steps at one occupant: 1/24 kg of oxygen and 1/24 of an occupant-day.
	for (int step = 0; step < 3600; ++step)
		hosted.world.Step();
	const World::LifeSupport &used = hosted.Support("Ship1");
	CHECK(used.connectedTo.empty());
	CHECK(used.tanks[0] == doctest::Approx(10.0 - 1.0 / 24.0).epsilon(1e-12));
	CHECK(used.tanks[1] == 10.0);
	CHECK(used.tanks[2] == 10.0);
	CHECK(used.cartridges[0] == doctest::Approx(30.0 - 1.0 / 24.0).epsilon(1e-12));

	// The first tank's last drop, then the second's; never below nothing, and an exhausted
	// cartridge stays at 0.
	hosted.Support("Ship1").tanks = {0.25 / 86400.0, 10.0, 10.0};
	hosted.Support("Ship1").cartridges = {0.5 / 86400.0};
	hosted.world.Step();
	CHECK(hosted.Support("Ship1").tanks[0] == 0.0);
	CHECK(hosted.Support("Ship1").tanks[1] == doctest::Approx(10.0 - 0.75 / 86400.0).epsilon(1e-15));
	CHECK(hosted.Support("Ship1").cartridges[0] == 0.0);
	hosted.world.Step();
	CHECK(hosted.Support("Ship1").cartridges[0] == 0.0);
}


TEST_CASE("A ship docked with every connection taken draws its own until one is let go")
{
	Hosted hosted;
	for (const std::string principal : {"Ada", "Bea", "Cy", "Di"})
		REQUIRE(hosted.Provision(principal) == Result::Succeeded);
	// Fixture bootstrap, as travel's undocking and docking would leave them: Ada's ship leaves, Eve
	// takes the connection it let go of, and Ada's docks again, at a venue with none free.
	hosted.Record("Ship1").dockedAt.clear();
	hosted.world.Step();
	CHECK(hosted.Support("Ship1").connectedTo.empty());
	REQUIRE(hosted.Provision("Eve") == Result::Succeeded);
	CHECK(hosted.Support("Ship5").connectedTo == "VenueOreHighport");
	hosted.Record("Ship1").dockedAt = "VenueOreHighport";
	const double before = hosted.Support("Ship1").tanks[0];
	hosted.world.Step();
	CHECK(hosted.Support("Ship1").connectedTo.empty());
	CHECK(hosted.Support("Ship1").tanks[0] < before);

	// Bea's ship leaves: in the same step Ada's is connected, and draws nothing.
	hosted.Record("Ship2").dockedAt.clear();
	const World::LifeSupport waiting = hosted.Support("Ship1");
	hosted.world.Step();
	CHECK(hosted.Support("Ship2").connectedTo.empty());
	CHECK(hosted.Support("Ship1").connectedTo == "VenueOreHighport");
	CHECK(hosted.Support("Ship1").tanks == waiting.tanks);
	CHECK(hosted.Support("Ship1").cartridges == waiting.cartridges);
}


TEST_CASE("Refill and Replace take the life-support goods from the hold, opening a tote at a time")
{
	// Governance's highport trades both goods, which a ship could unload there.
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	CHECK(hosted.Service(kAda, kRefill, "Ship1") == Result::CapacityExceeded);
	// Fixture bootstrap: tanks used to 2, 10 and 0 kg and a cartridge to 12 occupant-days. Docked and
	// connected, the ship uses none of them while the commands run.
	hosted.Support("Ship1").tanks = {2.0, 10.0, 0.0};
	hosted.Support("Ship1").cartridges = {12.0};
	const World::LifeSupport used = hosted.Support("Ship1");

	// What is refused writes nothing.
	CHECK(hosted.Service(kAda, kRefill, "Ship9") == Result::UnknownTarget);
	CHECK(hosted.Service(kBea, kRefill, "Ship1") == Result::NotAuthorised);
	CHECK(hosted.Service(kAda, kRefill, "Ship1") == Result::InsufficientStock);
	CHECK(hosted.Service(kAda, kReplace, "Ship1") == Result::InsufficientStock);
	World::LifeSupport &support = hosted.Support("Ship1");
	CHECK(support == used);

	// Fixture bootstrap: a tote each of oxygen (142.5 kg) and of cartridges (two) aboard.
	hosted.Hold("Ship1").lots = {{"ItemCarbonScrubberCartridge", 1}, {"ItemOxygen", 1}};
	CHECK(hosted.Service(kAda, kRefill, "Ship1") == Result::Succeeded);
	CHECK(support.tanks == std::vector<double>{10.0, 10.0, 10.0});
	CHECK(hosted.Hold("Ship1").lots == std::vector<World::Lot>{{"ItemCarbonScrubberCartridge", 1}});
	CHECK(hosted.Hold("Ship1").opened == std::vector<World::Opened>{{"ItemOxygen", 124.5}});
	CHECK(hosted.Service(kAda, kRefill, "Ship1") == Result::CapacityExceeded);
	// The opened tote is drawn on first; it is no tote to unload.
	support.tanks[1] = 9.0;
	CHECK(hosted.Service(kAda, kRefill, "Ship1") == Result::Succeeded);
	CHECK(hosted.Hold("Ship1").opened == std::vector<World::Opened>{{"ItemOxygen", 123.5}});
	CHECK(
	    hosted.Unload(kAda, Move{"Ship1", "VenueForgeHighport", "ItemOxygen", 1}) ==
	    Result::InsufficientStock);

	// A fresh cartridge in place of the used one, which is thrown away; then the tote's second.
	CHECK(hosted.Service(kAda, kReplace, "Ship1") == Result::Succeeded);
	CHECK(support.cartridges == std::vector<double>{30.0});
	CHECK(hosted.Hold("Ship1").lots.empty());
	CHECK(
	    hosted.Hold("Ship1").opened ==
	    std::vector<World::Opened>{{"ItemCarbonScrubberCartridge", 1.0}, {"ItemOxygen", 123.5}});
	CHECK(hosted.Service(kAda, kReplace, "Ship1") == Result::Succeeded);
	CHECK(hosted.Hold("Ship1").opened == std::vector<World::Opened>{{"ItemOxygen", 123.5}});
	CHECK(hosted.Service(kAda, kReplace, "Ship1") == Result::InsufficientStock);

	// The opened tote still takes its tote of the 160-tote hold.
	hosted.Stock("Company1").holdings = {{"VenueForgeHighport", "ItemCopper", 160, 0}};
	const Move copper{"Ship1", "VenueForgeHighport", "ItemCopper", 160};
	CHECK(hosted.Load(kAda, copper) == Result::CapacityExceeded);
	CHECK(
	    hosted.Load(kAda, Move{"Ship1", "VenueForgeHighport", "ItemCopper", 159}) == Result::Succeeded);

	// Anywhere: the hold is aboard. Fixture bootstrap: the ship off its pad, its last tank used.
	hosted.Record("Ship1").dockedAt.clear();
	support.tanks[2] = 0.0;
	CHECK(hosted.Service(kAda, kRefill, "Ship1") == Result::Succeeded);
	CHECK(support.tanks[2] == 10.0);
	CHECK(hosted.Hold("Ship1").opened == std::vector<World::Opened>{{"ItemOxygen", 113.5}});
}


TEST_CASE("A save whose life support its hull cannot hold, or connected to no venue, does not load")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	World::LifeSupport &support = hosted.Support("Ship1");
	const World::LifeSupport granted = support;
	support.tanks[1] = 10.5;
	const std::vector<std::byte> overfull = hosted.world.Save();
	support = granted;
	support.cartridges.push_back(30.0);
	const std::vector<std::byte> extra = hosted.world.Save();
	support = granted;
	support.tanks[0] = -1.0;
	const std::vector<std::byte> negative = hosted.world.Save();
	support = granted;
	support.connectedTo = "VenueNowhere";
	const std::vector<std::byte> nowhere = hosted.world.Save();
	support = granted;
	World::Cargo &cargo = hosted.Hold("Ship1");
	cargo.lots = {{"ItemCopper", 160}};
	cargo.opened = {{"ItemOxygen", 1.0}};
	const std::vector<std::byte> crowded = hosted.world.Save();
	cargo.lots = {{"ItemCopper", 159}};
	const std::vector<std::byte> full = hosted.world.Save();

	Hosted loaded;
	for (const auto &[save, problem] :
	     {std::pair{&overfull, "holds tanks or cartridges"},
	      {&extra, "holds tanks or cartridges"},
	      {&negative, "less than nothing"},
	      {&nowhere, "a venue the world does not hold"},
	      {&crowded, "more than its hold"}})
	{
		CAPTURE(problem);
		CHECK_THROWS_WITH_AS(
		    loaded.world.Load(*save), doctest::Contains(problem), Jettison::Serialise::FormatError);
	}
	loaded.world.Load(full);
	CHECK(loaded.Hold("Ship1").opened == std::vector<World::Opened>{{"ItemOxygen", 1.0}});
	CHECK(loaded.Support("Ship1") == granted);
}


TEST_CASE("A principal's ID can be a company's, and the two stay apart")
{
	Hosted hosted;
	CHECK(hosted.Provision("Company1") == Result::Succeeded);
	CHECK(hosted.Find(Kind::Principal, "Company1") != hosted.Find(Kind::Company, "Company1"));
	CHECK(hosted.Seen({ActorKind::Principal, "Company1"}).companyId == "Company1");
}


TEST_CASE("Loading and unloading conserve the goods between the venue and the hold")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 10, 0}};
	auto move = [](std::uint64_t totes)
	{ return Move{"Ship1", "VenueOreHighport", "ItemCopper", totes}; };
	auto split = [&](std::uint64_t atVenue, std::uint64_t aboard)
	{
		const CompanyView ada = hosted.Seen(kAda);
		CHECK(AtVenue(ada, "VenueOreHighport", "ItemCopper") == atVenue);
		CHECK(Aboard(ada, "Ship1", "ItemCopper") == aboard);
		// Nothing is kept as a record of zero.
		CHECK(ada.holdings.empty() == (atVenue == 0));
		CHECK(ada.ships[0].cargo.empty() == (aboard == 0));
	};

	CHECK(hosted.Load(kAda, move(4)) == Result::Succeeded);
	split(6, 4);
	CHECK(hosted.Unload(kAda, move(1)) == Result::Succeeded);
	split(7, 3);
	CHECK(hosted.Unload(kAda, move(3)) == Result::Succeeded);
	CHECK(hosted.Seen(kAda).ships[0].cargo.empty());
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 10, 0}};
	CHECK(hosted.Load(kAda, move(10)) == Result::Succeeded);
	split(0, 10);
	CHECK(hosted.Seen(kAda).credits == 25000);
	// Unloading at a venue holding none of the item makes its holding, in its place.
	hosted.Stock("Company1").holdings = {
	    {"VenueA", "ItemCopper", 1, 0}, {"VenueZ", "ItemCopper", 1, 0}};
	CHECK(hosted.Unload(kAda, move(10)) == Result::Succeeded);
	const CompanyView ada = hosted.Seen(kAda);
	REQUIRE(ada.holdings.size() == 3);
	CHECK(ada.holdings[1] == World::Holding{"VenueOreHighport", "ItemCopper", 10, 0});
}


TEST_CASE("A rejected move writes nothing")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {
	    {"VenueBrineHighport", "ItemSalt", 50, 0},
	    {"VenueOreHighport", "ItemCopper", 200, 5},
	    {"VenueOreHighport", "ItemTin", 1, 0}};
	REQUIRE(hosted.Load(kAda, {"Ship1", "VenueOreHighport", "ItemTin", 1}) == Result::Succeeded);
	const CompanyView before = hosted.Seen(kAda);

	auto copper = [](std::uint64_t totes, std::string venue = "VenueOreHighport")
	{ return Move{"Ship1", std::move(venue), "ItemCopper", totes}; };
	const std::uint64_t huge = std::numeric_limits<std::uint64_t>::max();
	const struct
	{
		bool aboard;
		Move move;
		Result expected;
	} rejected[]{
	    {true, {"Ship9", "VenueOreHighport", "ItemCopper", 1}, Result::UnknownTarget},
	    {true, copper(1, "VenueBrineHighport"), Result::WrongVenue},
	    {true, {"Ship1", "VenueBrineHighport", "ItemSalt", 1}, Result::WrongVenue},
	    {true, {"Ship1", "VenueOreHighport", "ItemBronze", 1}, Result::UnknownTarget},
	    {true, copper(0), Result::InvalidQuantity},
	    // 200 available and 5 reserved: the reserved do not load.
	    {true, copper(203), Result::InsufficientStock},
	    {true, copper(huge), Result::InsufficientStock},
	    // The starter holds 160 totes, one of them tin.
	    {true, copper(160), Result::CapacityExceeded},
	    {false, copper(1), Result::InsufficientStock},
	    {false, {"Ship1", "VenueOreHighport", "ItemTin", 2}, Result::InsufficientStock},
	    {false, {"Ship1", "VenueOreHighport", "ItemTin", huge}, Result::InsufficientStock},
	};
	for (const auto &[aboard, move, expected] : rejected)
	{
		CAPTURE(move.itemId);
		CAPTURE(move.totes);
		CHECK((aboard ? hosted.Load(kAda, move) : hosted.Unload(kAda, move)) == expected);
		CHECK(hosted.Seen(kAda) == before);
	}
	// A player with no company of their own, or a ship of someone else's.
	entt::registry &registry = hosted.world.Registry();
	registry.emplace<World::Principal>(
	    hosted.identities.Create(registry, Kind::Principal, "Bea"), "Bea");
	CHECK(hosted.Load(kBea, copper(1)) == Result::NotAuthorised);
	CHECK(hosted.Observe(kBea).companies.empty());
	CHECK(hosted.Seen(kAda) == before);

	registry.get<World::Ship>(hosted.Find(Kind::Ship, "Ship1")).dockedAt.clear();
	const CompanyView undocked = hosted.Seen(kAda);
	CHECK(hosted.Load(kAda, copper(1)) == Result::NotDocked);
	CHECK(hosted.Unload(kAda, {"Ship1", "VenueOreHighport", "ItemTin", 1}) == Result::NotDocked);
	// Off its pad it draws on its own life support, which the moves leave alone.
	CHECK(hosted.Seen(kAda).holdings == undocked.holdings);
	CHECK(hosted.Seen(kAda).ships[0].cargo == undocked.ships[0].cargo);
}


TEST_CASE("The commands refuse a hull whose hold is not a whole number of totes up to the largest")
{
	for (const double volume :
	     {0x1p64, static_cast<double>(Manifest::Core::kMaxHoldTotes) + 1.0, 160.5, -1.0,
	      std::numeric_limits<double>::quiet_NaN()})
	{
		CAPTURE(volume);
		Manifest::Content::ContentSet content = Scenario("trading");
		for (Manifest::Core::ShipClassSpec &hull : content.hulls)
			hull.baseStats[Manifest::Core::EffectStat::CargoVolume] = volume;
		RegisterTypes();
		SimWorld world{1.0, 0};
		World::Identities &identities = World::RegisterWorld(world);
		Manifest::Command::Requests &requests = Manifest::Command::RegisterRequests(world, identities);
		const Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, content);
		CHECK_THROWS_AS(
		    RegisterCommands(world, requests, identities, fleet, content), std::invalid_argument);
	}
}


TEST_CASE("A save whose ship carries more than its hull's hold does not load")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	World::Cargo &cargo = hosted.world.Registry().get<World::Cargo>(hosted.Find(Kind::Ship, "Ship1"));
	cargo.lots = {{"ItemCopper", 150}, {"ItemTin", 11}};
	const std::vector<std::byte> over = hosted.world.Save();
	cargo.lots = {{"ItemCopper", 150}, {"ItemTin", 10}};
	const std::vector<std::byte> full = hosted.world.Save();

	Hosted loaded;
	const std::uint64_t before = loaded.world.StateHash();
	CHECK_THROWS_WITH_AS(
	    loaded.world.Load(over), doctest::Contains("more than its hold"),
	    Jettison::Serialise::FormatError);
	CHECK(loaded.world.StateHash() == before);
	loaded.world.Load(full);
	CHECK(Aboard(loaded.Seen(kAda), "Ship1", "ItemTin") == 10);

	// A hull with no CargoVolume carries nothing, though no hull has a hold to name.
	Manifest::Content::ContentSet holdless = Scenario("trading");
	for (Manifest::Core::ShipClassSpec &hull : holdless.hulls)
		hull.baseStats.erase(Manifest::Core::EffectStat::CargoVolume);
	SimWorld world{1.0, 0};
	World::Identities &identities = World::RegisterWorld(world);
	Manifest::Command::Requests &requests = Manifest::Command::RegisterRequests(world, identities);
	RegisterCommands(
	    world, requests, identities, Manifest::Travel::Register(world, requests, identities, holdless),
	    holdless);
	CHECK_THROWS_WITH_AS(
	    world.Load(full), doctest::Contains("more than its hold"), Jettison::Serialise::FormatError);
}


TEST_CASE("A warehouse holds at most its limit of one item for one owner")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	const Move one{"Ship1", "VenueOreHighport", "ItemCopper", 1};
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 1, 0}};
	REQUIRE(hosted.Load(kAda, one) == Result::Succeeded);
	// Full with what is available and what is reserved together.
	hosted.Stock("Company1").holdings = {
	    {"VenueOreHighport", "ItemCopper", Manifest::Core::kMaxWarehouseTotes - 1, 1}};
	const CompanyView full = hosted.Seen(kAda);
	CHECK(hosted.Unload(kAda, one) == Result::CapacityExceeded);
	CHECK(hosted.Seen(kAda) == full);
	hosted.Stock("Company1").holdings = {
	    {"VenueOreHighport", "ItemCopper", Manifest::Core::kMaxWarehouseTotes - 2, 1}};
	CHECK(hosted.Unload(kAda, one) == Result::Succeeded);
	CHECK(
	    AtVenue(hosted.Seen(kAda), "VenueOreHighport", "ItemCopper") ==
	    Manifest::Core::kMaxWarehouseTotes - 1);
}


TEST_CASE("A hold carries whole totes, up to its hull's")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {
	    {"VenueOreHighport", "ItemCopper", 1000, 0}, {"VenueOreHighport", "ItemTin", 1000, 0}};
	auto move = [](std::string item, std::uint64_t totes)
	{ return Move{"Ship1", "VenueOreHighport", std::move(item), totes}; };
	REQUIRE(hosted.Load(kAda, move("ItemCopper", 150)) == Result::Succeeded);
	REQUIRE(hosted.Load(kAda, move("ItemTin", 10)) == Result::Succeeded);
	const CompanyView full = hosted.Seen(kAda);
	CHECK(hosted.Load(kAda, move("ItemCopper", 1)) == Result::CapacityExceeded);
	CHECK(hosted.Load(kAda, move("ItemTin", 1)) == Result::CapacityExceeded);
	CHECK(hosted.Seen(kAda) == full);
	REQUIRE(hosted.Unload(kAda, move("ItemTin", 1)) == Result::Succeeded);
	CHECK(hosted.Load(kAda, move("ItemCopper", 1)) == Result::Succeeded);
	CHECK(Aboard(hosted.Seen(kAda), "Ship1", "ItemCopper") == 151);
}


TEST_CASE(
    "Two players start at the governance scenario's highport, and neither moves the other's goods")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	const entt::registry &registry = hosted.world.Registry();
	// Two regions of one planet, each with its own two accounts: a player governs one, not the planet.
	CHECK(
	    registry.get<World::Region>(hosted.Find(Kind::Region, "RegionForgeNorth")).planetId ==
	    registry.get<World::Region>(hosted.Find(Kind::Region, "RegionForgeSouth")).planetId);
	for (const Kind kind : {Kind::PublicAccount, Kind::HouseholdAccount})
		CHECK(hosted.Find(kind, "RegionForgeNorth") != hosted.Find(kind, "RegionForgeSouth"));
	const CompanyView bea = hosted.Seen(kBea);
	CHECK(bea.companyId == "Company2");
	REQUIRE(bea.ships.size() == 1);
	CHECK(bea.ships[0].shipId == "Ship2");
	CHECK(bea.ships[0].dockedAt == "VenueForgeHighport");

	hosted.Stock("Company1").holdings = {{"VenueForgeHighport", "ItemCopper", 2, 0}};
	const Move adas{"Ship1", "VenueForgeHighport", "ItemCopper", 1};
	const Move beas{"Ship2", "VenueForgeHighport", "ItemCopper", 1};
	CHECK(hosted.Load(kBea, adas) == Result::NotAuthorised);
	CHECK(hosted.Load(kBea, beas) == Result::InsufficientStock);
	CHECK(hosted.Load(kAda, beas) == Result::NotAuthorised);
	CHECK(hosted.Load(kAda, adas) == Result::Succeeded);
	CHECK(hosted.Unload(kBea, adas) == Result::NotAuthorised);
	CHECK(AtVenue(hosted.Seen(kAda), "VenueForgeHighport", "ItemCopper") == 1);
	CHECK(Aboard(hosted.Seen(kAda), "Ship1", "ItemCopper") == 1);
	CHECK(hosted.Seen(kBea) == bea);
}


namespace
{
// The company's account, written as fixture bootstrap.
World::Account &Credits(Hosted &hosted, const std::string &companyId)
{
	return hosted.world.Registry().get<World::Account>(hosted.Find(Kind::CompanyAccount, companyId));
}
} // namespace


TEST_CASE("A company's credits buy its ship a tier of cargo holds, once, and its hold grows by it")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	const Upgrade holds{"Ship1", "OfferCargoHolds", 1};
	// A new company's 25,000 credits are short of the offer's 27,000 (D-096).
	const CompanyView start = hosted.Seen(kAda);
	CHECK(start.ships[0].hold == 160);
	CHECK(hosted.Run(kAda, kUpgrade, holds) == Result::InsufficientFunds);
	CHECK(hosted.Seen(kAda) == start);

	// What W15's furs run leaves it with, which W09's fixture earns by trading.
	Credits(hosted, "Company1").balance = 29590;
	const std::uint64_t sequence = hosted.requests.NextSequence(kAda);
	CHECK(hosted.Run(kAda, kUpgrade, holds) == Result::Succeeded);
	const CompanyView bought = hosted.Seen(kAda);
	CHECK(bought.credits == 2590);
	CHECK(bought.ships[0].hold == 2560);
	CHECK(bought.ships[0].systems == std::vector<World::Fitting>{{"SkillShipCargoHolds", 1}});

	// The request retried is answered from the ledger, and a new one for the tier it has is refused:
	// neither pays or fits again.
	const Receipt retried = hosted.requests.Admit(
	    hosted.world, kAda, sequence, std::string{kUpgrade}, kVersion,
	    BinaryArchive::Write(holds, FormatVersion{kVersion}));
	CHECK(retried.status == Receipt::Status::Completed);
	CHECK(retried.result == Result::Succeeded);
	hosted.world.Step();
	CHECK(hosted.Run(kAda, kUpgrade, holds) == Result::AlreadyFitted);
	CHECK(hosted.Seen(kAda) == bought);

	// The hold takes what the hull's alone would not, and no more.
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 2561, 0}};
	const Move copper{"Ship1", "VenueOreHighport", "ItemCopper", 2561};
	CHECK(hosted.Load(kAda, copper) == Result::CapacityExceeded);
	CHECK(hosted.Load(kAda, {"Ship1", "VenueOreHighport", "ItemCopper", 2560}) == Result::Succeeded);
	CHECK(Aboard(hosted.Seen(kAda), "Ship1", "ItemCopper") == 2560);
}


TEST_CASE("A refused upgrade pays nothing and fits nothing")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	Credits(hosted, "Company1").balance = 100000;
	const CompanyView before = hosted.Seen(kAda);
	const struct
	{
		Actor actor;
		Upgrade upgrade;
		Result expected;
	} refused[]{
	    {kAda, {"Ship9", "OfferCargoHolds", 1}, Result::UnknownTarget},
	    {kBea, {"Ship1", "OfferCargoHolds", 1}, Result::NotAuthorised},
	    {kAda, {"Ship1", "OfferNothing", 1}, Result::UnknownTarget},
	    {kAda, {"Ship1", "OfferCargoHolds", 0}, Result::InvalidQuantity},
	    // A tier at a time, and the offer sells only the first.
	    {kAda, {"Ship1", "OfferCargoHolds", 2}, Result::NotFittable},
	};
	for (const auto &[actor, upgrade, expected] : refused)
	{
		CAPTURE(upgrade.shipId);
		CAPTURE(upgrade.offerId);
		CAPTURE(upgrade.tier);
		CHECK(hosted.Run(actor, kUpgrade, upgrade) == expected);
		CHECK(hosted.Seen(kAda) == before);
	}
	World::Ship &ship = hosted.Record("Ship1");
	ship.dockedAt.clear();
	CHECK(hosted.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferCargoHolds", 1}) == Result::NotDocked);
	CHECK(hosted.Seen(kAda).credits == before.credits);
	CHECK_FALSE(hosted.world.Registry().all_of<World::Systems>(hosted.Find(Kind::Ship, "Ship1")));

	// A hull with no slot for the system, a prerequisite the ship lacks, and a hold past the largest.
	using Manifest::Content::ContentSet;
	const auto starter = [](ContentSet &set) -> Manifest::Core::ShipClassSpec &
	{
		return *std::ranges::find(
		    set.hulls, "HullLoneTraderStarter", &Manifest::Core::ShipClassSpec::hullId);
	};
	ContentSet slotless = Scenario("trading");
	starter(slotless).fitableEffects.clear();
	ContentSet unprepared = Scenario("trading");
	std::ranges::find(
	    unprepared.gameMode.skills, "SkillShipCargoHolds", &Manifest::Core::SkillNode::skillId)
	    ->requiredSkills = {{"SkillShipFuelBunkers", 1}};
	// A second skill of the same effect, which would take the first's place in the hold.
	ContentSet twin = Scenario("trading");
	Manifest::Core::SkillNode second = *std::ranges::find(
	    twin.gameMode.skills, "SkillShipCargoHolds", &Manifest::Core::SkillNode::skillId);
	second.skillId = "SkillShipCargoHoldsTwin";
	twin.gameMode.skills.push_back(second);
	Manifest::Content::OfferDefinition twinOffer = twin.offers.at(0);
	twinOffer.offerId = "OfferTwin";
	twinOffer.skillId = second.skillId;
	twin.offers.push_back(twinOffer);
	{
		Hosted other{twin};
		REQUIRE(other.Provision("Ada") == Result::Succeeded);
		Credits(other, "Company1").balance = 100000;
		REQUIRE(other.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferCargoHolds", 1}) == Result::Succeeded);
		CHECK(other.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferTwin", 1}) == Result::NotFittable);
		CHECK(other.Seen(kAda).credits == 73000);
		CHECK(other.Seen(kAda).ships[0].hold == 2560);
	}
	ContentSet vast = Scenario("trading");
	starter(vast).baseStats[Manifest::Core::EffectStat::CargoVolume] =
	    static_cast<double>(Manifest::Core::kMaxHoldTotes - 100);
	for (const auto &[set, expected] :
	     {std::pair<const ContentSet *, Result>{&slotless, Result::NotFittable},
	      {&unprepared, Result::NotFittable},
	      {&vast, Result::CapacityExceeded}})
	{
		Hosted other{*set};
		REQUIRE(other.Provision("Ada") == Result::Succeeded);
		Credits(other, "Company1").balance = 100000;
		CHECK(other.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferCargoHolds", 1}) == expected);
		CHECK(other.Seen(kAda).credits == 100000);
		CHECK(other.Seen(kAda).ships[0].systems.empty());
	}
}


TEST_CASE(
    "A save keeps a ship's systems, and one whose systems break their rules or its hold does not load")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	Credits(hosted, "Company1").balance = 27000;
	REQUIRE(hosted.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferCargoHolds", 1}) == Result::Succeeded);
	hosted.Hold("Ship1").lots = {{"ItemCopper", 2560}};
	Hosted loaded;
	loaded.world.Load(hosted.world.Save());
	CHECK(loaded.Seen(kAda) == hosted.Seen(kAda));
	CHECK(loaded.Seen(kAda).ships[0].hold == 2560);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	// A technology is the civilisation's, never a ship's system, though its effect is the holds'.
	Manifest::Content::ContentSet known = Scenario("trading");
	Manifest::Core::SkillNode technique = *std::ranges::find(
	    known.gameMode.skills, "SkillShipCargoHolds", &Manifest::Core::SkillNode::skillId);
	technique.skillId = "TechCargoHolds";
	technique.nodeType = Manifest::Core::NodeType::Tech;
	known.gameMode.skills.push_back(technique);
	{
		Hosted taught{known};
		REQUIRE(taught.Provision("Ada") == Result::Succeeded);
		taught.world.Registry().emplace<World::Systems>(
		    taught.Find(Kind::Ship, "Ship1"), World::Systems{{{"TechCargoHolds", 1}}});
		Hosted refused{known};
		CHECK_THROWS_WITH_AS(
		    refused.world.Load(taught.world.Save()), doctest::Contains("has systems it cannot carry"),
		    Jettison::Serialise::FormatError);
	}

	World::Systems &systems =
	    hosted.world.Registry().get<World::Systems>(hosted.Find(Kind::Ship, "Ship1"));
	const struct
	{
		std::vector<World::Fitting> fitted;
		const char *problem;
	} broken[]{
	    {{{"SkillShipCargoHolds", 0}}, "fitted at tier 0"},
	    {{{"SkillShipCargoHolds", 1}, {"SkillShipCargoHolds", 1}}, "out of order or fitted twice"},
	    {{{"SkillNothing", 1}}, "has systems it cannot carry"},
	    // Past the skill's three tiers, which no purchase reaches.
	    {{{"SkillShipCargoHolds", 4}}, "has systems it cannot carry"},
	    // Without its holds, the copper is more than the hull's 160.
	    {{}, "more than its hold"},
	};
	for (const auto &[fitted, problem] : broken)
	{
		CAPTURE(problem);
		systems.fitted = fitted;
		Hosted refused;
		const std::uint64_t before = refused.world.StateHash();
		CHECK_THROWS_WITH_AS(
		    refused.world.Load(hosted.world.Save()), doctest::Contains(problem),
		    Jettison::Serialise::FormatError);
		CHECK(refused.world.StateHash() == before);
	}
}

TEST_CASE("Two systems of one exclusive group are neither bought nor loaded together")
{
	// Cargo holds and a stealth coating made rivals, the starter given a slot for each.
	Manifest::Content::ContentSet rivals = Scenario("trading");
	for (Manifest::Core::SkillNode &skill : rivals.gameMode.skills)
		if (skill.skillId == "SkillShipCargoHolds" || skill.skillId == "SkillShipStealthCoating")
			skill.exclusiveGroupId = "GroupHull";
	std::ranges::find(rivals.hulls, "HullLoneTraderStarter", &Manifest::Core::ShipClassSpec::hullId)
	    ->fitableEffects.emplace_back("StealthCoating");
	Manifest::Content::OfferDefinition coating = rivals.offers.at(0);
	coating.offerId = "OfferStealthCoating";
	coating.skillId = "SkillShipStealthCoating";
	rivals.offers.push_back(coating);

	Hosted hosted{rivals};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	Credits(hosted, "Company1").balance = 100000;
	REQUIRE(hosted.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferCargoHolds", 1}) == Result::Succeeded);
	CHECK(
	    hosted.Run(kAda, kUpgrade, Upgrade{"Ship1", "OfferStealthCoating", 1}) == Result::NotFittable);
	CHECK(hosted.Seen(kAda).credits == 73000);
	CHECK(
	    hosted.Seen(kAda).ships[0].systems == std::vector<World::Fitting>{{"SkillShipCargoHolds", 1}});

	hosted.world.Registry().get<World::Systems>(hosted.Find(Kind::Ship, "Ship1")).fitted = {
	    {"SkillShipCargoHolds", 1}, {"SkillShipStealthCoating", 1}};
	Hosted refused{rivals};
	CHECK_THROWS_WITH_AS(
	    refused.world.Load(hosted.world.Save()), doctest::Contains("has systems it cannot carry"),
	    Jettison::Serialise::FormatError);
}


TEST_CASE("An upgrade saved pending, retried after the load and replayed from the start, pays once")
{
	const Upgrade holds{"Ship1", "OfferCargoHolds", 1};
	Hosted hosted;
	const std::vector<std::byte> start = hosted.world.Save();
	hosted.world.SetRecording(true);
	hosted.Admit(kHost, kProvision, Provision{"Ada", "Ada", "Co"});
	hosted.world.Step();
	// What W15's furs run leaves it with, written as bootstrap between the first two steps.
	Credits(hosted, "Company1").balance = 29590;
	const std::uint64_t sequence = hosted.requests.NextSequence(kAda);
	hosted.Admit(kAda, kUpgrade, holds);
	const std::vector<std::byte> pending = hosted.world.Save();
	hosted.world.Step();
	CHECK(hosted.Outcome(kAda, sequence) == Result::Succeeded);
	const CompanyView bought = hosted.Seen(kAda);
	CHECK(bought.credits == 2590);
	CHECK(bought.ships[0].hold == 2560);

	Hosted loaded;
	loaded.world.Load(pending);
	CHECK(loaded.world.Commands().Pending().size() == 1);
	loaded.world.Step();
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.Seen(kAda) == bought);
	const Receipt retried = loaded.requests.Admit(
	    loaded.world, kAda, sequence, std::string{kUpgrade}, kVersion,
	    BinaryArchive::Write(holds, FormatVersion{kVersion}));
	CHECK(retried.status == Receipt::Status::Completed);
	CHECK(retried.result == Result::Succeeded);
	loaded.world.Step();
	CHECK(loaded.Seen(kAda) == bought);

	Hosted replayed;
	replayed.world.Load(start);
	for (std::uint64_t step = 0; step < 2; ++step)
	{
		if (step == 1)
			Credits(replayed, "Company1").balance = 29590;
		for (const Jettison::Sim::AppliedCommand &applied : hosted.world.Recorded())
			if (applied.step == step)
				replayed.world.Commands().Push(applied.command);
		replayed.world.Step();
	}
	CHECK(replayed.world.StateHash() == hosted.world.StateHash());
	CHECK(replayed.Seen(kAda) == bought);
}

TEST_CASE("Requests run in their step in admission order, the later seeing what the earlier did")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueForgeHighport", "ItemCopper", 10, 0}};
	const CompanyView before = hosted.Seen(kAda);
	const Move six{"Ship1", "VenueForgeHighport", "ItemCopper", 6};
	const Receipt first = hosted.Admit(kAda, kLoad, six);
	const Receipt second = hosted.Admit(kAda, kLoad, six);
	// Two players provisioned in one step.
	const Receipt bea = hosted.Admit(kHost, kProvision, Provision{"Bea", "Bea", "Co"});
	const Receipt cy = hosted.Admit(kHost, kProvision, Provision{"Cy", "Cy", "Co"});
	CHECK(second.order == first.order + 1);
	CHECK(cy.order == bea.order + 1);
	CHECK(hosted.Seen(kAda) == before);
	CHECK(hosted.Observe(kBea).companies.empty());

	hosted.world.Step();
	CHECK(hosted.Outcome(kAda, 1) == Result::Succeeded);
	CHECK(hosted.Outcome(kAda, 2) == Result::InsufficientStock);
	CHECK(hosted.requests.Outcome(kAda, 2)->step == hosted.requests.Outcome(kAda, 1)->step);
	CHECK(AtVenue(hosted.Seen(kAda), "VenueForgeHighport", "ItemCopper") == 4);
	CHECK(Aboard(hosted.Seen(kAda), "Ship1", "ItemCopper") == 6);
	CHECK(hosted.Outcome(kHost, 2) == Result::Succeeded);
	CHECK(hosted.Outcome(kHost, 3) == Result::Succeeded);
	CHECK(hosted.Seen(kBea).companyId == "Company2");
	CHECK(hosted.Seen({ActorKind::Principal, "Cy"}).companyId == "Company3");
}


TEST_CASE("A retried command runs once, and the next stage's commands are refused as unsupported")
{
	Hosted hosted;
	const std::vector<std::byte> ada =
	    BinaryArchive::Write(Provision{"Ada", "Ada", "Co"}, FormatVersion{kVersion});
	const std::vector<std::byte> bea =
	    BinaryArchive::Write(Provision{"Bea", "Bea", "Co"}, FormatVersion{kVersion});
	auto provision = [&](const std::vector<std::byte> &payload)
	{
		return hosted.requests.Admit(
		    hosted.world, kHost, 1, std::string{kProvision}, kVersion, payload);
	};

	const Receipt queued = provision(ada);
	const Receipt pending = provision(ada);
	CHECK(pending.status == Receipt::Status::Queued);
	CHECK(pending.order == queued.order);
	CHECK(hosted.world.Commands().Pending().size() == 1);
	hosted.world.Step();
	const Receipt completed = provision(ada);
	CHECK(completed.status == Receipt::Status::Completed);
	CHECK(completed.result == Result::Succeeded);
	CHECK(provision(bea).result == Result::RequestConflict);
	CHECK(hosted.world.Commands().Pending().empty());
	CHECK(hosted.Observe(kHost).companies.size() == 1);

	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 10, 0}};
	const std::vector<std::byte> load = BinaryArchive::Write(
	    Move{"Ship1", "VenueOreHighport", "ItemCopper", 4}, FormatVersion{kVersion});
	auto send = [&](std::uint64_t sequence, std::string_view command)
	{
		return hosted.requests.Admit(
		    hosted.world, kAda, sequence, std::string{command}, kVersion, load);
	};
	REQUIRE(send(1, kLoad).status == Receipt::Status::Queued);
	hosted.world.Step();
	CHECK(send(1, kLoad).status == Receipt::Status::Completed);
	CHECK(send(1, kUnload).result == Result::RequestConflict);
	hosted.world.Step();
	CHECK(Aboard(hosted.Seen(kAda), "Ship1", "ItemCopper") == 4);

	// A command no rules registered is refused, and refusing one consumes no sequence.
	for (const std::string_view command :
	     {"Manifest.Market.PlaceOrder", "Manifest.Market.CancelOrder", "Manifest.Travel.Depart"})
	{
		CAPTURE(command);
		const Receipt refused = send(2, command);
		CHECK(refused.status == Receipt::Status::Refused);
		CHECK(refused.result == Result::UnsupportedCommand);
	}
	CHECK(hosted.requests.NextSequence(kAda) == 2);
}


TEST_CASE("A save with work pending, and a replay from the start, carry the companies on alike")
{
	const Move adasCopper{"Ship1", "VenueForgeHighport", "ItemCopper", 4};
	const Move beasTin{"Ship2", "VenueForgeHighport", "ItemTin", 1};

	Hosted hosted{"governance"};
	const std::vector<std::byte> start = hosted.world.Save();
	hosted.world.SetRecording(true);
	hosted.Admit(kHost, kProvision, Provision{"Ada", "Ada", "Ada's"});
	hosted.Admit(kHost, kProvision, Provision{"Bea", "Bea", "Bea's"});
	hosted.world.Step();
	Seed(hosted);
	hosted.Admit(kAda, kLoad, adasCopper);
	hosted.Admit(kBea, kLoad, beasTin);
	hosted.Admit(kBea, kLoad, adasCopper);
	hosted.world.Step();
	hosted.Admit(kAda, kUnload, Move{"Ship1", "VenueForgeHighport", "ItemCopper", 1});
	hosted.Admit(kAda, kLoad, Move{"Ship1", "VenueForgeHighport", "ItemTin", 1});
	hosted.world.Step();
	hosted.Admit(kBea, kUnload, beasTin);
	const std::vector<std::byte> save = hosted.world.Save();
	hosted.world.Step();

	CHECK(hosted.Outcome(kBea, 2) == Result::NotAuthorised);
	CHECK(hosted.Outcome(kAda, 3) == Result::InsufficientStock);
	CHECK(hosted.Outcome(kBea, 3) == Result::Succeeded);
	const CompanyView adas = hosted.Seen(kAda);
	CHECK(adas.displayName == "Ada's");
	CHECK(AtVenue(adas, "VenueForgeHighport", "ItemCopper") == 7);
	CHECK(Aboard(adas, "Ship1", "ItemCopper") == 3);
	CHECK(
	    hosted.Seen(kBea).holdings ==
	    std::vector<World::Holding>{{"VenueForgeHighport", "ItemTin", 3, 0}});

	Hosted loaded{"governance"};
	loaded.world.Load(save);
	CHECK(loaded.world.Commands().Pending().size() == 1);
	CHECK(loaded.requests.NextSequence(kBea) == 4);
	loaded.world.Step();
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.Observe(kHost) == hosted.Observe(kHost));

	Hosted replayed{"governance"};
	replayed.world.Load(start);
	for (std::uint64_t step = 0; step < 4; ++step)
	{
		if (step == 1)
			Seed(replayed);
		for (const Jettison::Sim::AppliedCommand &applied : hosted.world.Recorded())
			if (applied.step == step)
				replayed.world.Commands().Push(applied.command);
		replayed.world.Step();
	}
	CHECK(replayed.world.StateHash() == hosted.world.StateHash());
	CHECK(replayed.Observe(kHost) == hosted.Observe(kHost));
	CHECK(replayed.Outcome(kBea, 2) == Result::NotAuthorised);
	CHECK(replayed.requests.NextSequence(kAda) == 4);

	// The same in every build the gate runs: Debug, Release, sanitised and the TUI off. It moves only
	// with what a save holds or the engine's save format, and a change that moves it says so.
	CHECK(hosted.world.StateHash() == 0x52ea7bc62b961041ULL);
}


TEST_CASE(
    "A docked ship pays its berth fee each hour, owes what it cannot, with interest, and pays all to "
    "leave")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	// Bea directs the venues' operator, so the day's end leaves their fees as this test sets them.
	registry.get<World::Organisation>(hosted.Find(Kind::Organisation, "OrganisationAnvilPortAuthority"))
	    .principalId = "Bea";
	const entt::entity ship = hosted.Find(Kind::Ship, "Ship1");
	World::Account &account =
	    registry.get<World::Account>(hosted.Find(Kind::CompanyAccount, "Company1"));
	World::Account &ore =
	    registry.get<World::Account>(hosted.Find(Kind::VenueAccount, "VenueOreHighport"));
	World::Account &brine =
	    registry.get<World::Account>(hosted.Find(Kind::VenueAccount, "VenueBrineHighport"));
	const World::Fees &posted =
	    registry.get<World::Fees>(hosted.Find(Kind::Market, "VenueOreHighport"));
	CHECK(posted.classes.size() == 5);
	REQUIRE(World::Find(posted, "skiff"));
	CHECK(*World::Find(posted, "skiff") == World::ClassFees{"skiff", 100, 20});
	CHECK(posted.berthPerOccupantHour == 2);
	CHECK(posted.arrearsRateBps == 1000);
	const auto stepTo = [&hosted](std::uint64_t step)
	{
		while (hosted.world.Clock().Step() < step)
			hosted.world.Step();
	};
	constexpr std::uint64_t kHour = 3600;

	// The hour's fee as its last step ends, and no sooner: the skiff's 20 and 2 for its occupant, whom
	// Ore's life support keeps.
	stepTo(kHour - 1);
	CHECK(account.balance == 25'000);
	stepTo(kHour);
	CHECK(account.balance == 24'978);
	CHECK(ore.balance == 22);

	// With no life support connected, the skiff's 20 alone.
	registry.remove<World::LifeSupport>(ship);
	stepTo(2 * kHour);
	CHECK(account.balance == 24'958);
	CHECK(ore.balance == 42);

	// What the account cannot pay is owed.
	account.balance = 1;
	stepTo(3 * kHour);
	CHECK(account.balance == 0);
	CHECK(ore.balance == 43);
	CHECK(registry.get<World::Arrears>(ship) == World::Arrears{19, 0, 3 * kHour - 1});

	// A day's interest as the day ends, before the hour's fee: at a rate above the usury limit, the
	// limit's 35% a year, 350 credits a day on 365,000.
	stepTo(23 * kHour);
	registry.replace<World::Arrears>(ship, 365'000, 0);
	registry.get<World::Fees>(hosted.Find(Kind::Market, "VenueOreHighport")).arrearsRateBps = 100'000;
	stepTo(24 * kHour);
	CHECK(registry.get<World::Arrears>(ship) == World::Arrears{365'370, 0, 0});

	// Leaving pays the arrears to Ore and the skiff's docking fee to Brine, not another class's, or the
	// ship does not leave.
	for (World::ClassFees &fees :
	     registry.get<World::Fees>(hosted.Find(Kind::Market, "VenueBrineHighport")).classes)
		if (fees.padClass != "skiff")
			fees.docking = 1;
	const Manifest::Travel::Travel toBrine{"Ship1", "VenueBrineHighport"};
	CHECK(hosted.Run(kAda, Manifest::Travel::kTravel, toBrine) == Result::InsufficientFunds);
	account.balance = 365'370 + 99;
	CHECK(hosted.Run(kAda, Manifest::Travel::kTravel, toBrine) == Result::InsufficientFunds);
	const std::int64_t oreBefore = ore.balance;
	account.balance = 365'370 + 100;
	CHECK(hosted.Run(kAda, Manifest::Travel::kTravel, toBrine) == Result::Succeeded);
	CHECK(account.balance == 0);
	CHECK(ore.balance == oreBefore + 365'370);
	CHECK(brine.balance == 100);
	CHECK_FALSE(registry.all_of<World::Arrears>(ship));
}


TEST_CASE("The principal directing a venue's operator sets its fees, charged from the next charge")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	registry.emplace<World::Principal>(
	    hosted.identities.Create(registry, Kind::Principal, "Bea"), "Bea");
	World::Account &ore =
	    registry.get<World::Account>(hosted.Find(Kind::VenueAccount, "VenueOreHighport"));
	World::Account &brine =
	    registry.get<World::Account>(hosted.Find(Kind::VenueAccount, "VenueBrineHighport"));
	const World::Fees &posted =
	    registry.get<World::Fees>(hosted.Find(Kind::Market, "VenueOreHighport"));
	const World::Fees defaults = posted;
	const auto stepTo = [&hosted](std::uint64_t step)
	{
		while (hosted.world.Clock().Step() < step)
			hosted.world.Step();
	};
	constexpr std::uint64_t kHour = 3600;

	World::Fees raised = defaults;
	for (World::ClassFees &each : raised.classes)
		each = {each.padClass, each.docking * 5, each.berthPerHour + 30};
	raised.berthPerOccupantHour = 3;
	const auto set = [&hosted](const Actor &actor, std::string venueId, const World::Fees &fees)
	{ return hosted.Run(actor, kSetFees, SetFees{std::move(venueId), fees}); };

	// The Anvil Port Authority runs both venues, and the AI runs it until Bea directs it.
	CHECK(set(kBea, "VenueOreHighport", raised) == Result::NotAuthorised);
	registry.get<World::Organisation>(hosted.Find(Kind::Organisation, "OrganisationAnvilPortAuthority"))
	    .principalId = "Bea";
	World::Fees missing = raised;
	missing.classes.pop_back();
	World::Fees unknown = raised;
	unknown.classes[0].padClass = "tender";
	World::Fees negative = raised;
	negative.classes[1].berthPerHour = -1;
	World::Fees owedNegative = raised;
	owedNegative.berthPerOccupantHour = -1;
	World::Fees usurious = raised;
	usurious.arrearsRateBps = 3501;
	const struct
	{
		Actor actor;
		std::string venueId;
		World::Fees fees;
		Result expected;
	} refused[]{
	    {kAda, "VenueOreHighport", raised, Result::NotAuthorised},
	    {kBea, "VenueNowhere", raised, Result::UnknownTarget},
	    {kBea, "VenueOreHighport", missing, Result::InvalidQuantity},
	    {kBea, "VenueOreHighport", unknown, Result::InvalidQuantity},
	    {kBea, "VenueOreHighport", negative, Result::InvalidQuantity},
	    {kBea, "VenueOreHighport", owedNegative, Result::InvalidQuantity},
	    {kBea, "VenueOreHighport", usurious, Result::RateTooHigh},
	};
	for (const auto &[actor, venueId, fees, expected] : refused)
	{
		CAPTURE(expected);
		CHECK(set(actor, venueId, fees) == expected);
		CHECK(posted == defaults);
	}

	// An hour charged at the defaults, 20 and 2 for the occupant, stays charged; set as the next hour
	// runs, the new fees are its fee: 50 and 3.
	stepTo(kHour);
	REQUIRE(ore.balance == 22);
	REQUIRE(set(kBea, "VenueOreHighport", raised) == Result::Succeeded);
	CHECK(posted == raised);
	stepTo(2 * kHour);
	CHECK(ore.balance == 22 + 53);

	// The docking fee is paid as the journey starts, and a change after it is not.
	REQUIRE(set(kBea, "VenueBrineHighport", defaults) == Result::Succeeded);
	REQUIRE(
	    hosted.Run(
	        kAda, Manifest::Travel::kTravel, Manifest::Travel::Travel{"Ship1", "VenueBrineHighport"}) ==
	    Result::Succeeded);
	CHECK(brine.balance == 100);
	usurious.arrearsRateBps = 3500;
	CHECK(set(kBea, "VenueBrineHighport", usurious) == Result::Succeeded);
	CHECK(brine.balance == 100);
}


TEST_CASE("Saved fees and arrears load, and broken ones do not")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	const entt::entity ship = hosted.Find(Kind::Ship, "Ship1");
	registry.emplace<World::Arrears>(ship, 5, 7, 42);
	Hosted loaded;
	loaded.world.Load(hosted.world.Save());
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.world.Registry().get<World::Arrears>(ship) == World::Arrears{5, 7, 42});

	for (const World::Arrears broken : {World::Arrears{0, 0}, World::Arrears{5, World::kRateYear}})
	{
		registry.replace<World::Arrears>(ship, broken);
		Hosted refused;
		CHECK_THROWS(refused.world.Load(hosted.world.Save()));
	}
	registry.replace<World::Arrears>(ship, 5, 0);
	registry.get<World::Ship>(ship).dockedAt.clear();
	Hosted undocked;
	CHECK_THROWS(undocked.world.Load(hosted.world.Save()));
	registry.get<World::Ship>(ship).dockedAt = "VenueOreHighport";
	registry.remove<World::Arrears>(ship);
	registry.get<World::Fees>(hosted.Find(Kind::Market, "VenueOreHighport")).classes[0].docking = -1;
	Hosted negative;
	CHECK_THROWS(negative.world.Load(hosted.world.Save()));
	World::Fees &fees = registry.get<World::Fees>(hosted.Find(Kind::Market, "VenueOreHighport"));
	fees.classes[0].docking = 0;
	std::swap(fees.classes[0], fees.classes[1]);
	Hosted unordered;
	CHECK_THROWS(unordered.world.Load(hosted.world.Save()));
	std::swap(fees.classes[0], fees.classes[1]);
	registry.remove<World::Pricing>(hosted.Find(Kind::Market, "VenueOreHighport"));
	Hosted unpriced;
	CHECK_THROWS(unpriced.world.Load(hosted.world.Save()));
}


TEST_CASE("An AI operator's fees follow its station's occupancy each day, within their bounds")
{
	// Ore's pads that fit the starter all taken, with connections to spare; Brine's all free.
	Manifest::Content::ContentSet roomy = Scenario("trading");
	roomy.scenarios.at("trading").venues[0].lifeSupportCapacity = 1000;
	Hosted stepped{roomy};
	Hosted forwarded{roomy};
	for (Hosted *hosted : {&stepped, &forwarded})
		for (int players = 0; hosted->Provision(std::format("Player{}", players)) == Result::Succeeded;
		     ++players)
		{
		}
	REQUIRE(stepped.world.Save() == forwarded.world.Save());
	const std::string ore = "VenueOreHighport";
	const std::string brine = "VenueBrineHighport";
	const auto level = [](Hosted &hosted, const std::string &venueId) -> int &
	{
		return hosted.world.Registry().get<World::Pricing>(hosted.Find(Kind::Market, venueId)).levelBps;
	};
	const auto posted = [](Hosted &hosted, const std::string &venueId) -> const World::Fees &
	{ return hosted.world.Registry().get<World::Fees>(hosted.Find(Kind::Market, venueId)); };
	const auto skiff = [&posted](Hosted &hosted, const std::string &venueId)
	{ return *World::Find(posted(hosted, venueId), "skiff"); };
	const auto forward = [](Hosted &hosted, std::uint64_t until)
	{
		while (hosted.world.Clock().Step() < until)
			hosted.world.FastForward(until);
	};
	constexpr std::uint64_t kDay = 86'400;

	// Moved as the day's last step ends, not before: Ore's up a tenth of the defaults, Brine's down,
	// each to the nearest credit, the arrears rate the default's.
	while (stepped.world.Clock().Step() < kDay - 1)
		stepped.world.Step();
	CHECK(level(stepped, ore) == 10'000);
	stepped.world.Step();
	CHECK(level(stepped, ore) == 11'000);
	CHECK(level(stepped, brine) == 9'000);
	CHECK(skiff(stepped, ore) == World::ClassFees{"skiff", 110, 22});
	CHECK(*World::Find(posted(stepped, ore), "cruiser") == World::ClassFees{"cruiser", 59'400, 11'880});
	CHECK(posted(stepped, ore).berthPerOccupantHour == 2);
	CHECK(posted(stepped, ore).arrearsRateBps == 1000);
	CHECK(skiff(stepped, brine) == World::ClassFees{"skiff", 90, 18});
	CHECK(posted(stepped, brine).berthPerOccupantHour == 2);

	// Stepped and passed over to the second day's end alike, to the byte.
	while (stepped.world.Clock().Step() < 2 * kDay)
		stepped.world.Step();
	forward(forwarded, 2 * kDay);
	CHECK(forwarded.world.Save() == stepped.world.Save());
	CHECK(level(forwarded, ore) == 12'000);
	CHECK(level(forwarded, brine) == 8'000);
	CHECK(skiff(forwarded, brine) == World::ClassFees{"skiff", 80, 16});

	// Held within half to twice the defaults.
	level(forwarded, ore) = 20'000;
	level(forwarded, brine) = 5'000;
	forward(forwarded, 3 * kDay);
	CHECK(level(forwarded, ore) == 20'000);
	CHECK(level(forwarded, brine) == 5'000);
	CHECK(skiff(forwarded, ore) == World::ClassFees{"skiff", 200, 40});
	CHECK(skiff(forwarded, brine) == World::ClassFees{"skiff", 50, 10});
	CHECK(posted(forwarded, brine).berthPerOccupantHour == 1);

	// The AI leaves the fees of a venue whose operator a player directs.
	forwarded.world.Registry()
	    .get<World::Organisation>(forwarded.Find(Kind::Organisation, "OrganisationAnvilPortAuthority"))
	    .principalId = "Bea";
	level(forwarded, ore) = 15'000;
	const World::ClassFees held = skiff(forwarded, ore);
	forward(forwarded, 4 * kDay);
	CHECK(level(forwarded, ore) == 15'000);
	CHECK(skiff(forwarded, ore) == held);
}
