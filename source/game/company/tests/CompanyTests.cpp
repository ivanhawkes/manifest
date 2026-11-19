#include "TestHelpers.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/company/Company.h>
#include <manifest/world/World.h>

using namespace Manifest::Company;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Jettison::Sim::SimWorld;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
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


// A world as the host makes one, in its scenario's initial state.
struct Hosted
{
	explicit Hosted(const std::string &scenario = "trading")
	    : content{Scenario(scenario)}, identities{(RegisterTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		RegisterCommands(requests, identities, content);
		World::CreateInitialState(world, identities, content);
	}

	// The request admitted, as the actor's next, and run in the next step.
	template <typename T> Result Run(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{kVersion});
		const auto receipt =
		    requests.Admit(world, actor, sequence, std::string{command}, kVersion, bytes);
		REQUIRE(receipt.status == Manifest::Command::Receipt::Status::Queued);
		world.Step();
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

	World::Cargo &Cargo(const std::string &shipId)
	{
		return world.Registry().get<World::Cargo>(Find(Kind::Ship, shipId));
	}

	const Manifest::Content::ContentSet &content;
	SimWorld world{1.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};


} // namespace


TEST_CASE("Provisioning gives a player a company, its credits, a home and a docked ship, once")
{
	Hosted hosted;
	CHECK(hosted.Provision("Ada") == Result::Succeeded);
	const entt::registry &registry = hosted.world.Registry();

	CHECK(registry.get<World::Principal>(hosted.Find(Kind::Principal, "Ada")).displayName == "Ada");
	const auto &company = registry.get<World::Company>(hosted.Find(Kind::Company, "Company1"));
	CHECK(company.principalId == "Ada");
	CHECK(registry.get<World::Account>(hosted.Find(Kind::CompanyAccount, "Company1")).balance == 25000);
	const auto &polity =
	    registry.get<World::Organisation>(hosted.Find(Kind::Organisation, "Organisation1"));
	CHECK(polity.kind == Manifest::Content::OrganisationKind::Polity);
	CHECK(polity.principalId == "Ada");
	CHECK(
	    registry.get<World::Region>(hosted.Find(Kind::Region, "RegionOreHighlands")).governorId ==
	    "Organisation1");
	CHECK(
	    registry.get<World::Ship>(hosted.Find(Kind::Ship, "Ship1")) ==
	    World::Ship{"Company1", "HullLoneTraderStarter", "VenueOreHighport"});
	CHECK(hosted.Cargo("Ship1").lots.empty());
	CHECK(hosted.Stock("Company1").holdings.empty());

	// Again under a new sequence, it makes nothing: the ledger answers a retry, this the principal.
	const std::size_t entities = registry.view<const World::Identity>().size();
	CHECK(hosted.Provision("Ada") == Result::AlreadyProvisioned);
	// The only home is taken, and the rejection spends no ID.
	CHECK(hosted.Provision("Bea") == Result::NoHomeRegion);
	CHECK(hosted.Run(kHost, kProvision, Provision{"", "", ""}) == Result::MalformedPayload);
	CHECK(registry.view<const World::Identity>().size() == entities);
	CHECK(hosted.identities.Issue(Kind::Company) == "Company2");
}


TEST_CASE("A principal's ID can be a company's, and the two stay apart")
{
	Hosted hosted;
	CHECK(hosted.Provision("Company1") == Result::Succeeded);
	CHECK(hosted.Find(Kind::Principal, "Company1") != hosted.Find(Kind::Company, "Company1"));
	CHECK(
	    hosted.world.Registry()
	        .get<World::Company>(hosted.Find(Kind::Company, "Company1"))
	        .principalId == "Company1");
}


TEST_CASE("Loading and unloading conserve the goods between the venue and the hold")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 10.0, 0.0}};
	auto move = [](double quantity)
	{ return Move{"Ship1", "VenueOreHighport", "ItemCopper", quantity}; };

	CHECK(hosted.Load(kAda, move(4.0)) == Result::Succeeded);
	CHECK(hosted.Stock("Company1").holdings[0].available == 6.0);
	CHECK(hosted.Cargo("Ship1").lots == std::vector<World::Lot>{{"ItemCopper", 4.0}});
	CHECK(hosted.Unload(kAda, move(1.5)) == Result::Succeeded);
	CHECK(hosted.Stock("Company1").holdings[0].available == 7.5);
	CHECK(hosted.Cargo("Ship1").lots[0].quantity == 2.5);
	CHECK(hosted.Unload(kAda, move(2.5)) == Result::Succeeded);
	CHECK(hosted.Cargo("Ship1").lots.empty());
	CHECK(hosted.Load(kAda, move(10.0)) == Result::Succeeded);
	CHECK(hosted.Stock("Company1").holdings.empty());
	// Unloading at a venue holding none of the item makes its holding, in its place.
	hosted.Stock("Company1").holdings = {
	    {"VenueA", "ItemCopper", 1.0, 0.0}, {"VenueZ", "ItemCopper", 1.0, 0.0}};
	CHECK(hosted.Unload(kAda, move(10.0)) == Result::Succeeded);
	REQUIRE(hosted.Stock("Company1").holdings.size() == 3);
	CHECK(
	    hosted.Stock("Company1").holdings[1] ==
	    World::Holding{"VenueOreHighport", "ItemCopper", 10.0, 0.0});
}


TEST_CASE("A rejected move writes nothing")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {
	    {"VenueBrineHighport", "ItemSalt", 50.0, 0.0},
	    {"VenueOreHighport", "ItemCopper", 1000.0, 5.0},
	    {"VenueOreHighport", "ItemTin", 1.0, 0.0}};
	REQUIRE(hosted.Load(kAda, {"Ship1", "VenueOreHighport", "ItemTin", 1.0}) == Result::Succeeded);
	const World::Stock stock = hosted.Stock("Company1");
	const World::Cargo cargo = hosted.Cargo("Ship1");
	const std::int64_t credits = hosted.world.Registry()
	                                 .get<World::Account>(hosted.Find(Kind::CompanyAccount, "Company1"))
	                                 .balance;

	auto copper = [](double quantity, std::string venue = "VenueOreHighport")
	{ return Move{"Ship1", std::move(venue), "ItemCopper", quantity}; };
	const double huge = std::numeric_limits<double>::max();
	const struct
	{
		bool aboard;
		Move move;
		Result expected;
	} rejected[]{
	    {true, {"Ship9", "VenueOreHighport", "ItemCopper", 1.0}, Result::UnknownTarget},
	    {true, copper(1.0, "VenueBrineHighport"), Result::WrongVenue},
	    {true, {"Ship1", "VenueBrineHighport", "ItemSalt", 1.0}, Result::WrongVenue},
	    {true, {"Ship1", "VenueOreHighport", "ItemRice", 1.0}, Result::UnknownTarget},
	    {true, copper(0.0), Result::InvalidQuantity},
	    {true, copper(-1.0), Result::InvalidQuantity},
	    {true, copper(std::numeric_limits<double>::quiet_NaN()), Result::InvalidQuantity},
	    {true, copper(std::numeric_limits<double>::infinity()), Result::InvalidQuantity},
	    // 1,000 available and 5 reserved: the reserved do not load.
	    {true, copper(1003.0), Result::InsufficientStock},
	    {true, copper(huge), Result::InsufficientStock},
	    // 200 t of copper is 22.3 m^3, over the starter's 20.
	    {true, copper(200.0), Result::CapacityExceeded},
	    {false, copper(1.0), Result::InsufficientStock},
	    {false, {"Ship1", "VenueOreHighport", "ItemTin", 1.5}, Result::InsufficientStock},
	    {false, {"Ship1", "VenueOreHighport", "ItemTin", huge}, Result::InsufficientStock},
	};
	for (const auto &[aboard, move, expected] : rejected)
	{
		CAPTURE(move.itemId);
		CAPTURE(move.quantity);
		CHECK((aboard ? hosted.Load(kAda, move) : hosted.Unload(kAda, move)) == expected);
		CHECK(hosted.Stock("Company1") == stock);
		CHECK(hosted.Cargo("Ship1") == cargo);
	}
	// A player with no company of their own, or a ship of someone else's.
	entt::registry &registry = hosted.world.Registry();
	registry.emplace<World::Principal>(
	    hosted.identities.Create(registry, Kind::Principal, "Bea"), "Bea");
	CHECK(hosted.Load(kBea, copper(1.0)) == Result::NotAuthorised);
	CHECK(hosted.Stock("Company1") == stock);

	registry.get<World::Ship>(hosted.Find(Kind::Ship, "Ship1")).dockedAt.clear();
	CHECK(hosted.Load(kAda, copper(1.0)) == Result::NotDocked);
	CHECK(hosted.Unload(kAda, {"Ship1", "VenueOreHighport", "ItemTin", 1.0}) == Result::NotDocked);
	CHECK(hosted.Stock("Company1") == stock);
	CHECK(hosted.Cargo("Ship1") == cargo);
	CHECK(
	    registry.get<World::Account>(hosted.Find(Kind::CompanyAccount, "Company1")).balance == credits);
}


TEST_CASE("Two players take the governance scenario's homes, and neither moves the other's goods")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	const entt::registry &registry = hosted.world.Registry();
	CHECK(
	    registry.get<World::Region>(hosted.Find(Kind::Region, "RegionForgeNorth")).governorId ==
	    "Organisation1");
	CHECK(
	    registry.get<World::Region>(hosted.Find(Kind::Region, "RegionForgeSouth")).governorId ==
	    "Organisation2");
	CHECK(registry.get<World::Ship>(hosted.Find(Kind::Ship, "Ship2")).dockedAt == "VenueForgeHighport");
	CHECK(registry.get<World::Ship>(hosted.Find(Kind::Ship, "Ship2")).companyId == "Company2");

	hosted.Stock("Company1").holdings = {{"VenueForgeHighport", "ItemCopper", 5.0, 0.0}};
	const Move adas{"Ship1", "VenueForgeHighport", "ItemCopper", 1.0};
	const Move beas{"Ship2", "VenueForgeHighport", "ItemCopper", 1.0};
	CHECK(hosted.Load(kBea, adas) == Result::NotAuthorised);
	CHECK(hosted.Load(kBea, beas) == Result::InsufficientStock);
	CHECK(hosted.Load(kAda, beas) == Result::NotAuthorised);
	CHECK(hosted.Load(kAda, adas) == Result::Succeeded);
	CHECK(hosted.Unload(kBea, adas) == Result::NotAuthorised);
	CHECK(hosted.Stock("Company1").holdings[0].available == 4.0);
	CHECK(hosted.Stock("Company2").holdings.empty());
}


TEST_CASE("A save keeps the companies, their stock and holds, and the loaded world carries on alike")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueOreHighport", "ItemCopper", 10.0, 0.0}};
	const Move move{"Ship1", "VenueOreHighport", "ItemCopper", 3.0};
	REQUIRE(hosted.Load(kAda, move) == Result::Succeeded);
	const std::vector<std::byte> save = hosted.world.Save();

	Hosted loaded;
	loaded.world.Load(save);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.Cargo("Ship1") == hosted.Cargo("Ship1"));
	CHECK(loaded.Provision("Ada") == Result::AlreadyProvisioned);
	CHECK(hosted.Provision("Ada") == Result::AlreadyProvisioned);
	CHECK(loaded.Unload(kAda, move) == Result::Succeeded);
	CHECK(hosted.Unload(kAda, move) == Result::Succeeded);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.Stock("Company1").holdings[0].available == 10.0);
}
