#include "TestHelpers.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/company/Company.h>
#include <manifest/query/Query.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

using namespace Manifest::Query;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Result;
using Manifest::World::Kind;
namespace Company = Manifest::Company;
namespace World = Manifest::World;

namespace
{
const Actor kHost{ActorKind::Administrator, "Host"};
const Actor kAda{ActorKind::Principal, "Ada"};
const Actor kBea{ActorKind::Principal, "Bea"};


// A world as the host makes one, in the scenario's initial state, with players provisioned.
struct Hosted
{
	explicit Hosted(const std::string &scenario)
	    : identities{(Company::RegisterTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		content = Manifest::Content::LoadScenario(files, definitions, scenario);
		Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, content);
		Company::RegisterCommands(world, requests, identities, fleet, content);
		World::CreateInitialState(world, identities, content);
		Manifest::Travel::OpenStations(world, fleet);
	}


	template <typename T> Result Run(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		const std::vector<std::byte> bytes =
		    BinaryArchive::Write(payload, FormatVersion{Company::kVersion});
		REQUIRE(
		    requests.Admit(world, actor, sequence, std::string{command}, Company::kVersion, bytes)
		        .status == Manifest::Command::Receipt::Status::Queued);
		world.Step();
		return requests.Outcome(actor, sequence).value().result;
	}


	Result Provision(const std::string &principalId)
	{
		return Run(kHost, Company::kProvision, Company::Provision{principalId, principalId, "Co"});
	}


	World::Stock &Stock(const std::string &companyId)
	{
		return world.Registry().get<World::Stock>(identities.Find(Kind::Company, companyId));
	}


	Snapshot Observe(const Actor &actor) const
	{
		return Manifest::Query::Observe(world, identities, actor);
	}

	Manifest::Content::ContentSet content;
	Jettison::Sim::SimWorld world{1.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};
} // namespace


TEST_CASE("A player sees their own company as it was at the completed step, and the copy stays so")
{
	Hosted hosted{"trading"};
	CHECK(hosted.Observe(kAda).companies.empty());
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 3, 2}};
	const Company::Move move{"Ship1", "VenueOreHighport", "ItemCopper", 3};
	REQUIRE(hosted.Run(kAda, Company::kLoad, move) == Result::Succeeded);

	const std::uint64_t hash = hosted.world.StateHash();
	Snapshot loaded = hosted.Observe(kAda);
	CHECK(hosted.world.StateHash() == hash);

	// Ada's starting reports of the eleven other stations of her system, Brine's and the yards among
	// them, from the step she spawned in.
	REQUIRE(loaded.markets.size() == 11);
	CHECK(
	    std::ranges::any_of(
	        loaded.markets, [](const Manifest::Query::MarketView &market)
	        { return market.report.venueId == "VenueBrineHighport"; }));
	for (const Manifest::Query::MarketView &market : loaded.markets)
	{
		CHECK(market.report.source == World::Source::Starting);
		CHECK(market.report.observed == 0);
		CHECK_FALSE(market.live);
	}
	loaded.markets.clear();
	loaded.regions.clear();
	CHECK(
	    loaded == Snapshot{
	                  2,
	                  {{"Company1",
	                    "Co",
	                    25000,
	                    {{"VenueOreHighport", "ItemCopper", 0, 2}},
	                    {{"Ship1",
	                      "HullLoneTraderStarter",
	                      "VenueOreHighport",
	                      {{"ItemCopper", 3}},
	                      {},
	                      {},
	                      {{10.0, 10.0, 10.0}, {30.0}, "VenueOreHighport"},
	                      160,
	                      {},
	                      {},
	                      {},
	                      World::Fuel{4000.0, 12.5},
	                      World::Fuel{4000.0, 12.5}}}}}});

	REQUIRE(hosted.Run(kAda, Company::kUnload, move) == Result::Succeeded);
	CHECK(loaded.companies[0].ships[0].cargo[0].totes == 3);
	const Snapshot unloaded = hosted.Observe(kAda);
	CHECK(unloaded.step == 3);
	CHECK(unloaded.companies[0].ships[0].cargo.empty());
	CHECK(unloaded.companies[0].holdings[0].available == 3);
}


TEST_CASE("Each player sees only their own company and results, and the host sees every company")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	hosted.Stock("Company2").holdings = {{"VenueForgeHighport", "ItemCopper", 10, 0}};
	REQUIRE(
	    hosted.Run(
	        kBea, Company::kLoad, Company::Move{"Ship2", "VenueForgeHighport", "ItemCopper", 1}) ==
	    Result::Succeeded);

	const Snapshot adas = hosted.Observe(kAda);
	REQUIRE(adas.companies.size() == 1);
	CHECK(adas.companies[0].companyId == "Company1");
	CHECK(adas.companies[0].holdings.empty());
	REQUIRE(adas.companies[0].ships.size() == 1);
	CHECK(adas.companies[0].ships[0].shipId == "Ship1");
	CHECK(adas.companies[0].ships[0].cargo.empty());

	const Snapshot beas = hosted.Observe(kBea);
	REQUIRE(beas.companies.size() == 1);
	CHECK(beas.companies[0].companyId == "Company2");
	CHECK(beas.companies[0].ships[0].cargo == std::vector<World::Lot>{{"ItemCopper", 1}});

	// A principal the world does not have, and one named as the host is, see nothing.
	CHECK(hosted.Observe({ActorKind::Principal, "Cy"}).companies.empty());
	CHECK(hosted.Observe({ActorKind::Principal, "Host"}).companies.empty());
	const Snapshot all = hosted.Observe(kHost);
	REQUIRE(all.companies.size() == 2);
	CHECK(all.companies[0] == adas.companies[0]);
	CHECK(all.companies[1] == beas.companies[0]);

	// Bea's load was her first request: Ada's ledger has no such result.
	CHECK(hosted.requests.Outcome(kBea, 1));
	CHECK_FALSE(hosted.requests.Outcome(kAda, 1));
}


TEST_CASE("A player's markets are their own reports, live only as observed at the snapshot's step")
{
	Hosted hosted{"trading"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	World::Reports &adas =
	    hosted.world.Registry().get<World::Reports>(hosted.identities.Find(Kind::Principal, "Ada"));
	const std::uint64_t step = hosted.world.Clock().Step();
	adas.reports = {
	    {"VenueBrineHighport", World::Source::Docked, step - 1, 0, {}},
	    {"VenueOreHighport",
	     World::Source::Docked,
	     step,
	     0,
	     {{"ItemFurs", {{5, 1}}, {{7, 2}, {8, 3}}, 0, 0, 0}}}};

	const Snapshot seen = hosted.Observe(kAda);
	REQUIRE(seen.markets.size() == 2);
	CHECK_FALSE(seen.markets[0].live);
	CHECK(seen.markets[1] == MarketView{adas.reports[1], true});

	// Bea's are her own starting reports alone; the host, with no fleet, has none.
	const Snapshot beas = hosted.Observe(kBea);
	REQUIRE(beas.markets.size() == 11);
	CHECK(beas.markets[0].report.source == World::Source::Starting);
	CHECK(beas.markets[0].report.observed == 1);
	CHECK(hosted.Observe(kHost).markets.empty());
	CHECK(hosted.Observe({ActorKind::Principal, "Cy"}).markets.empty());
}


TEST_CASE("Every player sees each region and its governor, and its governor its treasury and queue")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	registry.get<World::Account>(hosted.identities.Find(Kind::CompanyAccount, "Company1")).balance =
	    300'000;
	REQUIRE(
	    hosted.Run(kAda, Company::kFound, Company::Found{"RegionForgeNorth", "Company1", "North"}) ==
	    Result::Succeeded);

	// Fixture bootstrap: Ada's region's policy, as SetDevelopment would set it.
	registry.emplace<World::Development>(
	    hosted.identities.Find(Kind::Region, "RegionForgeNorth"), true, 50'000);
	const auto region = [](const Snapshot &snapshot, std::string_view regionId)
	{
		const auto found = std::ranges::find(snapshot.regions, regionId, &RegionView::regionId);
		REQUIRE(found != snapshot.regions.end());
		return *found;
	};

	const Snapshot adas = hosted.Observe(kAda);
	const Snapshot beas = hosted.Observe(kBea);
	const Snapshot hosts = hosted.Observe(kHost);
	REQUIRE(adas.regions.size() == 5);
	CHECK(std::ranges::is_sorted(adas.regions, {}, &RegionView::regionId));

	// Hers, with its treasury, its empty queue and its policy.
	RegionView north = region(adas, "RegionForgeNorth");
	CHECK(north.planetId == "PlanetForge");
	CHECK(north.governorId == "Organisation1");
	CHECK(north.governorName == "North");
	CHECK(north.playerGoverned);
	CHECK(north.population == 25'000.0);
	CHECK(north.health == 100.0);
	CHECK(north.treasury == 1'200'000);
	CHECK(north.queue.empty());
	CHECK(north.development == World::Development{true, 50'000});

	// Bea sees whose it is, and none of its treasury, queue or policy.
	RegionView seen = region(beas, "RegionForgeNorth");
	CHECK(seen.governorName == "North");
	CHECK(seen.playerGoverned);
	CHECK_FALSE(seen.treasury.has_value());
	CHECK_FALSE(seen.development.has_value());

	// Open to founding, and an AI neighbour's, its figures the AI's alone, which the host sees.
	CHECK(region(beas, "RegionForgeSouth").governorId.empty());
	const RegionView east = region(adas, "RegionForgeEast");
	CHECK(east.governorName == "Forge East Council");
	CHECK_FALSE(east.playerGoverned);
	CHECK_FALSE(east.treasury.has_value());
	CHECK(east.queue.empty());
	const RegionView hostsEast = region(hosts, "RegionForgeEast");
	CHECK(hostsEast.treasury == 1'000'000);
	REQUIRE(hostsEast.queue.size() == 1);
	CHECK(hostsEast.queue[0].project.projectId == "ProjectBronzeMetallurgy");
	CHECK(hostsEast.queue[0].stock.empty());

	// Observed again, nothing has changed: a reconnecting player sees the same.
	CHECK(hosted.Observe(kAda) == adas);
}
