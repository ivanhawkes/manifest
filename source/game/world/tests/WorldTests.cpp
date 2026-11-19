#include "TestHelpers.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/content/Content.h>
#include <manifest/world/World.h>

using namespace Manifest::World;
using Jettison::Sim::SimWorld;

namespace
{
const Manifest::Content::ContentSet &Trading()
{
	static const Manifest::Content::ContentSet content = []
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		return Manifest::Content::LoadScenario(files, definitions, "trading");
	}();
	return content;
}


// A world with the hosted registrations, as the host makes one.
struct Hosted
{
	Hosted() : identities{(RegisterTypes(), RegisterWorld(world))}
	{
	}

	SimWorld world{1.0, 0};
	Identities &identities;
};


std::int64_t Balance(const Hosted &hosted, Kind kind, const std::string &id)
{
	const entt::entity account = hosted.identities.Find(kind, id);
	REQUIRE(account != entt::null);
	return hosted.world.Registry().get<Account>(account).balance;
}
} // namespace


TEST_CASE("The scenario seeds its organisations, its regions and each region's two accounts")
{
	Hosted hosted;
	CreateInitialState(hosted.world, hosted.identities, Trading());
	const entt::registry &registry = hosted.world.Registry();
	CHECK(registry.view<const Organisation>().size() == 2);
	CHECK(registry.view<const Region>().size() == 2);
	CHECK(registry.view<const Account>().size() == 4);

	const entt::entity home = hosted.identities.Find(Kind::Region, "RegionOreHighlands");
	REQUIRE(home != entt::null);
	CHECK(registry.get<Region>(home).playerHome);
	CHECK(registry.get<Region>(home).governorId.empty());
	CHECK(registry.get<Region>(home).population == 25000.0);
	const entt::entity council = hosted.identities.Find(Kind::Organisation, "OrganisationBrineCouncil");
	REQUIRE(council != entt::null);
	CHECK(registry.get<Organisation>(council).principalId.empty());

	// One region's ID names two accounts, told apart by their kind.
	CHECK(Balance(hosted, Kind::PublicAccount, "RegionOreHighlands") == 1000000);
	CHECK(Balance(hosted, Kind::HouseholdAccount, "RegionOreHighlands") == 5000000);
	CHECK(hosted.identities.Find(Kind::CompanyAccount, "RegionOreHighlands") == entt::null);

	CHECK_THROWS_AS(
	    CreateInitialState(hosted.world, hosted.identities, Trading()), std::invalid_argument);
}


TEST_CASE("The same content seeds the same world")
{
	Hosted first;
	Hosted second;
	CreateInitialState(first.world, first.identities, Trading());
	CreateInitialState(second.world, second.identities, Trading());
	CHECK(first.world.StateHash() == second.world.StateHash());
}


TEST_CASE("An ID is unique within its kind, and the same text in two kinds is two entities")
{
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	const entt::entity company = hosted.identities.Create(registry, Kind::Company, "Shared");
	const entt::entity organisation = hosted.identities.Create(registry, Kind::Organisation, "Shared");
	REQUIRE(company != entt::null);
	REQUIRE(organisation != entt::null);
	CHECK(company != organisation);
	CHECK(hosted.identities.Find(Kind::Company, "Shared") == company);
	CHECK(hosted.identities.Find(Kind::Organisation, "Shared") == organisation);

	CHECK(hosted.identities.Create(registry, Kind::Company, "Shared") == entt::null);
	CHECK(hosted.identities.Create(registry, Kind::Company, "") == entt::null);
	CHECK(registry.view<const Identity>().size() == 2);
}


TEST_CASE("Issued IDs count up, skip one already held and are never issued twice across a save")
{
	Hosted hosted;
	REQUIRE(hosted.identities.Create(hosted.world.Registry(), Kind::Company, "Company2") != entt::null);
	CHECK(hosted.identities.Issue(Kind::Company) == "Company1");
	CHECK(hosted.identities.Issue(Kind::Company) == "Company3");
	CHECK(hosted.identities.Issue(Kind::Principal) == "Principal1");

	const std::vector<std::byte> save = hosted.world.Save();
	Hosted loaded;
	loaded.world.Load(save);
	CHECK(loaded.identities.Issue(Kind::Company) == "Company4");
	CHECK(loaded.identities.Issue(Kind::Organisation) == "Organisation1");
}


TEST_CASE("A load rebuilds the lookup from the saved entities")
{
	Hosted hosted;
	CreateInitialState(hosted.world, hosted.identities, Trading());
	const std::vector<std::byte> save = hosted.world.Save();

	Hosted loaded;
	REQUIRE(loaded.identities.Create(loaded.world.Registry(), Kind::Company, "Stale") != entt::null);
	loaded.world.Load(save);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.identities.Find(Kind::Company, "Stale") == entt::null);
	for (const auto [entity, identity] : hosted.world.Registry().view<const Identity>().each())
	{
		CAPTURE(identity.id);
		const entt::entity found = loaded.identities.Find(identity.kind, identity.id);
		REQUIRE(found != entt::null);
		CHECK(loaded.world.Registry().get<Identity>(found).id == identity.id);
	}
	CHECK(Balance(loaded, Kind::HouseholdAccount, "RegionBrineCoast") == 5000000);
}


TEST_CASE("A save naming two entities alike does not load, and the world is as it was")
{
	Hosted broken;
	entt::registry &registry = broken.world.Registry();
	for (int copy = 0; copy < 2; ++copy)
		registry.emplace<Identity>(registry.create(), Kind::Region, "RegionTwice");
	const std::vector<std::byte> save = broken.world.Save();

	Hosted hosted;
	CreateInitialState(hosted.world, hosted.identities, Trading());
	const std::uint64_t before = hosted.world.StateHash();
	CHECK_THROWS_AS(hosted.world.Load(save), Jettison::Serialise::FormatError);
	CHECK(hosted.world.StateHash() == before);
	CHECK(hosted.identities.Find(Kind::Region, "RegionOreHighlands") != entt::null);
	CHECK(hosted.identities.Find(Kind::Region, "RegionTwice") == entt::null);
}


TEST_CASE("A save whose stock or cargo breaks its rules does not load, and the world is as it was")
{
	Hosted hosted;
	CreateInitialState(hosted.world, hosted.identities, Trading());
	const std::uint64_t before = hosted.world.StateHash();
	constexpr std::uint64_t kFull = Manifest::Core::kMaxWarehouseIbcs;
	const std::vector<Stock> stocks{
	    {{{"VenueA", "ItemB", 1, 0}, {"VenueA", "ItemA", 1, 0}}},
	    {{{"VenueA", "ItemA", 1, 0}, {"VenueA", "ItemA", 2, 0}}},
	    {{{"VenueA", "ItemA", kFull + 1, 0}}},
	    {{{"VenueA", "ItemA", kFull, 1}}},
	    {{{"VenueA", "ItemA", 1, std::numeric_limits<std::uint64_t>::max()}}},
	    {{{"VenueA", "ItemA", 0, 0}}},
	    {{{"", "ItemA", 1, 0}}}};
	for (const Stock &stock : stocks)
	{
		Hosted broken;
		broken.world.Registry().emplace<Stock>(broken.world.Registry().create(), stock);
		CHECK_THROWS_AS(hosted.world.Load(broken.world.Save()), Jettison::Serialise::FormatError);
	}
	const std::vector<Cargo> holds{
	    {{{"ItemB", 1}, {"ItemA", 1}}},
	    {{{"ItemA", 1}, {"ItemA", 1}}},
	    {{{"ItemA", 0}}},
	    {{{"", 1}}},
	    {{{"ItemA", Manifest::Core::kMaxHoldIbcs + 1}}},
	    {{{"ItemA", Manifest::Core::kMaxHoldIbcs}, {"ItemB", 1}}}};
	for (const Cargo &cargo : holds)
	{
		Hosted broken;
		broken.world.Registry().emplace<Cargo>(broken.world.Registry().create(), cargo);
		CHECK_THROWS_AS(hosted.world.Load(broken.world.Save()), Jettison::Serialise::FormatError);
	}
	CHECK(hosted.world.StateHash() == before);
}
