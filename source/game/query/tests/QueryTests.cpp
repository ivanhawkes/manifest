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
		Company::RegisterCommands(requests, identities, content);
		World::CreateInitialState(world, identities, content);
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
	CHECK(hosted.Observe(kAda) == Snapshot{0, {}});
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 10.0, 2.0}};
	const Company::Move move{"Ship1", "VenueOreHighport", "ItemCopper", 3.0};
	REQUIRE(hosted.Run(kAda, Company::kLoad, move) == Result::Succeeded);

	const std::uint64_t hash = hosted.world.StateHash();
	const Snapshot loaded = hosted.Observe(kAda);
	CHECK(hosted.world.StateHash() == hash);
	CHECK(
	    loaded ==
	    Snapshot{
	        2,
	        {{"Company1",
	          "Co",
	          25000,
	          {{"VenueOreHighport", "ItemCopper", 7.0, 2.0}},
	          {{"Ship1", "HullLoneTraderStarter", "VenueOreHighport", {{"ItemCopper", 3.0}}}}}}});

	REQUIRE(hosted.Run(kAda, Company::kUnload, move) == Result::Succeeded);
	CHECK(loaded.companies[0].ships[0].cargo[0].quantity == 3.0);
	const Snapshot unloaded = hosted.Observe(kAda);
	CHECK(unloaded.step == 3);
	CHECK(unloaded.companies[0].ships[0].cargo.empty());
	CHECK(unloaded.companies[0].holdings[0].available == 10.0);
}


TEST_CASE("Each player sees only their own company and results, and the host sees every company")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	hosted.Stock("Company2").holdings = {{"VenueForgeHighport", "ItemCopper", 5.0, 0.0}};
	REQUIRE(
	    hosted.Run(
	        kBea, Company::kLoad, Company::Move{"Ship2", "VenueForgeHighport", "ItemCopper", 1.0}) ==
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
	CHECK(beas.companies[0].ships[0].cargo == std::vector<World::Lot>{{"ItemCopper", 1.0}});

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
