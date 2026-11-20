#include "TestHelpers.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
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
	explicit Hosted(const std::string &scenario = "trading")
	    : content{Scenario(scenario)}, identities{(RegisterTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		RegisterCommands(requests, identities, content);
		World::CreateInitialState(world, identities, content);
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


TEST_CASE("Provisioning gives a player a company, its credits, a home and a docked ship, once")
{
	Hosted hosted;
	CHECK(hosted.Observe(kAda).companies.empty());
	CHECK(hosted.Provision("Ada") == Result::Succeeded);
	CHECK(
	    hosted.Seen(kAda) ==
	    CompanyView{
	        "Company1", "Co", 25000, {}, {{"Ship1", "HullLoneTraderStarter", "VenueOreHighport", {}}}});

	// What the snapshot leaves out: the principal, and the polity governing the player's home.
	const entt::registry &registry = hosted.world.Registry();
	CHECK(registry.get<World::Principal>(hosted.Find(Kind::Principal, "Ada")).displayName == "Ada");
	const auto &polity =
	    registry.get<World::Organisation>(hosted.Find(Kind::Organisation, "Organisation1"));
	CHECK(polity.kind == Manifest::Content::OrganisationKind::Polity);
	CHECK(polity.principalId == "Ada");
	CHECK(
	    registry.get<World::Region>(hosted.Find(Kind::Region, "RegionOreHighlands")).governorId ==
	    "Organisation1");

	// Again under a new sequence, it makes nothing: the ledger answers a retry, this the principal.
	const std::size_t entities = registry.view<const World::Identity>().size();
	CHECK(hosted.Provision("Ada") == Result::AlreadyProvisioned);
	// The only home is taken, and the rejection spends no ID.
	CHECK(hosted.Provision("Bea") == Result::NoHomeRegion);
	CHECK(hosted.Run(kHost, kProvision, Provision{"", "", ""}) == Result::MalformedPayload);
	CHECK(hosted.Observe(kBea).companies.empty());
	CHECK(hosted.Observe(kHost).companies.size() == 1);
	CHECK(registry.view<const World::Identity>().size() == entities);
	CHECK(hosted.identities.Issue(Kind::Company) == "Company2");
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
	    {true, {"Ship1", "VenueOreHighport", "ItemRice", 1}, Result::UnknownTarget},
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
	CHECK(hosted.Seen(kAda) == undocked);
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
		CHECK_THROWS_AS(RegisterCommands(requests, identities, content), std::invalid_argument);
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
	RegisterCommands(requests, identities, holdless);
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


TEST_CASE("Requests run in their step in admission order, the later seeing what the earlier did")
{
	Hosted hosted{"governance"};
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Stock("Company1").holdings = {{"VenueForgeHighport", "ItemCopper", 10, 0}};
	const CompanyView before = hosted.Seen(kAda);
	const Move six{"Ship1", "VenueForgeHighport", "ItemCopper", 6};
	const Receipt first = hosted.Admit(kAda, kLoad, six);
	const Receipt second = hosted.Admit(kAda, kLoad, six);
	// Two players for the one home left, in one step.
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
	CHECK(hosted.Outcome(kHost, 3) == Result::NoHomeRegion);
	CHECK(hosted.Seen(kBea).companyId == "Company2");
	CHECK(hosted.Observe({ActorKind::Principal, "Cy"}).companies.empty());
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

	// Whatever Stage 4 names its order, travel and upgrade commands, none is registered before its
	// rules are, and refusing one consumes no sequence.
	for (const std::string_view command :
	     {"Manifest.Market.PlaceOrder", "Manifest.Market.CancelOrder", "Manifest.Travel.Depart",
	      "Manifest.Company.Upgrade"})
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
	CHECK(hosted.world.StateHash() == 0xf1fc7aa932f63cf4ULL);
}
