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
#include <manifest/policy/Policy.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

using namespace Manifest::Policy;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Jettison::Sim::SimWorld;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
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


void RegisterAllTypes()
{
	Manifest::Travel::RegisterTypes();
	Manifest::Company::RegisterTypes();
	Manifest::Trade::RegisterTypes();
	RegisterTypes();
}


// A world as the host makes one, in its scenario's initial state.
struct Hosted
{
	explicit Hosted(const std::string &scenario = "trading")
	    : content{Scenario(scenario)}, identities{(RegisterAllTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		const Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, content);
		Manifest::Company::RegisterCommands(world, requests, identities, fleet, content);
		Manifest::Trade::Register(world, requests, identities, content);
		Register(world, requests, identities, content);
		World::CreateInitialState(world, identities, content);
		Manifest::Travel::OpenStations(world, fleet);
		Manifest::Trade::Quote(world, identities, content);
	}

	// The request admitted as the actor's next, and run in the next step.
	template <typename T>
	Result Run(const Actor &actor, std::string_view command, std::uint32_t version, const T &payload)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{version});
		REQUIRE(
		    requests.Admit(world, actor, sequence, std::string{command}, version, bytes).status ==
		    Receipt::Status::Queued);
		world.Step();
		const auto completed = requests.Outcome(actor, sequence);
		REQUIRE(completed);
		return completed->result;
	}

	Result Provision(const std::string &principalId)
	{
		return Run(
		    kHost, Manifest::Company::kProvision, Manifest::Company::kVersion,
		    Manifest::Company::Provision{principalId, principalId, "Co"});
	}

	Result SetPolicy(const Actor &actor, const Set &set)
	{
		return Run(actor, kSet, kVersion, set);
	}

	entt::entity Ship(const std::string &shipId) const
	{
		return identities.Find(Kind::Ship, shipId);
	}

	const Manifest::Content::ContentSet &content;
	SimWorld world{1.0 / 60.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};
} // namespace


TEST_CASE("A ship's owner sets its policy, which gives it a job, and no one else may")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	const entt::entity ship = hosted.Ship("Ship1");
	REQUIRE(ship != entt::null);
	const entt::registry &registry = hosted.world.Registry();
	CHECK_FALSE(registry.all_of<World::Policy>(ship));
	CHECK_FALSE(registry.all_of<World::Job>(ship));

	CHECK(hosted.SetPolicy(kBea, {"Ship1", true, 0, 0}) == Result::NotAuthorised);
	CHECK(hosted.SetPolicy(kAda, {"Ship9", true, 0, 0}) == Result::UnknownTarget);
	CHECK(hosted.SetPolicy(kAda, {"Ship1", true, -1, 0}) == Result::InvalidQuantity);
	CHECK(hosted.SetPolicy(kAda, {"Ship1", true, 0, -1}) == Result::InvalidQuantity);
	CHECK_FALSE(registry.all_of<World::Policy>(ship));

	CHECK(hosted.SetPolicy(kAda, {"Ship1", true, 250, 100}) == Result::Succeeded);
	CHECK(registry.get<World::Policy>(ship) == World::Policy{true, 250, 100});
	CHECK(registry.get<World::Job>(ship) == World::Job{});

	// An edit keeps the job where it is, the allowance it has spent with it.
	World::Job held;
	held.phase = World::Phase::Selling;
	held.itemId = "ItemFurs";
	held.destinationId = "VenueBrineHighport";
	held.totes = 10;
	held.cost = 900;
	held.allowanceSpent = 40;
	hosted.world.Registry().replace<World::Job>(ship, held);
	CHECK(hosted.SetPolicy(kAda, {"Ship1", false, 0, 50}) == Result::Succeeded);
	CHECK(registry.get<World::Policy>(ship) == World::Policy{false, 0, 50});
	CHECK(registry.get<World::Job>(ship) == held);
}


TEST_CASE("A saved policy and job load, and a broken one does not")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", true, 5, 7}) == Result::Succeeded);
	const entt::entity ship = hosted.Ship("Ship1");
	const std::vector<std::byte> save = hosted.world.Save();

	Hosted loaded;
	loaded.world.Load(save);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.world.Registry().get<World::Policy>(ship) == World::Policy{true, 5, 7});

	World::Job broken;
	broken.cost = 3;
	hosted.world.Registry().replace<World::Job>(ship, broken);
	Hosted refused;
	CHECK_THROWS(refused.world.Load(hosted.world.Save()));
}
