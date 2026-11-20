#include "TestHelpers.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <jettison/ecs/Scheduler.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/company/Company.h>
#include <manifest/trade/Trade.h>
#include <manifest/world/World.h>

using namespace Manifest::Trade;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Jettison::Sim::SimWorld;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
using Manifest::World::Kind;
using Manifest::World::Side;
namespace World = Manifest::World;

namespace
{
const Manifest::Content::ContentSet &Trading()
{
	static const Manifest::Content::ContentSet loaded = []
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		return Manifest::Content::LoadScenario(files, definitions, "trading");
	}();
	return loaded;
}


const Actor kAda{ActorKind::Principal, "Ada"};
const Actor kBea{ActorKind::Principal, "Bea"};
constexpr std::string_view kOre = "VenueOreHighport";
constexpr std::string_view kFurs = "ItemFurs";

// Raw microcredits for whole credits a unit.
constexpr std::int64_t Credits(std::int64_t whole)
{
	return whole * 1'000'000;
}


// Every balance, holding and order: what a refused command must leave as it was.
struct Business
{
	std::map<std::pair<Kind, std::string>, std::int64_t> balances;
	std::map<std::pair<Kind, std::string>, World::Stock> stocks;
	std::map<std::string, World::Order> orders;

	bool operator==(const Business &) const = default;
};


// The trading scenario as the host starts it, with the brokers' first quotes only when asked for, so
// a case trades against its own companies: Ada and Bea each with a company of their own, bootstrapped
// with 25,000 credits.
struct Hosted
{
	explicit Hosted(bool quoted = false)
	    : identities{(
	          Manifest::Company::RegisterTypes(), RegisterTypes(), World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		Manifest::Company::RegisterCommands(requests, identities, Trading());
		Register(world, requests, identities, Trading());
		World::CreateInitialState(world, identities, Trading());
		AddCompany("Ada", "CompanyA");
		AddCompany("Bea", "CompanyB");
		if (quoted)
			Quote(world, identities, Trading());
	}

	void AddCompany(const std::string &principal, const std::string &company)
	{
		entt::registry &registry = world.Registry();
		registry.emplace<World::Principal>(
		    identities.Create(registry, Kind::Principal, principal), principal);
		const entt::entity entity = identities.Create(registry, Kind::Company, company);
		registry.emplace<World::Company>(entity, principal, company);
		registry.emplace<World::Stock>(entity);
		registry.emplace<World::Account>(
		    identities.Create(registry, Kind::CompanyAccount, company), 25'000);
	}

	template <typename T> Receipt Admit(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{kVersion});
		return requests.Admit(
		    world, actor, requests.NextSequence(actor), std::string{command}, kVersion, bytes);
	}

	Result Outcome(const Actor &actor, std::uint64_t sequence) const
	{
		const auto completed = requests.Outcome(actor, sequence);
		REQUIRE(completed);
		return completed->result;
	}

	// The order admitted as the actor's next and run in the next step.
	Result Place(
	    const Actor &actor, Side side, std::uint64_t totes, std::int64_t limit,
	    std::string_view item = kFurs, std::string_view venue = kOre)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		REQUIRE(
		    Admit(
		        actor, kPlace,
		        Manifest::Trade::Place{std::string{venue}, std::string{item}, side, totes, limit})
		        .status == Receipt::Status::Queued);
		world.Step();
		return Outcome(actor, sequence);
	}

	// As many one-tote orders, admitted as many to a step as an actor may queue.
	void Rest(const Actor &actor, Side side, std::size_t orders, std::int64_t limit)
	{
		for (std::size_t order = 0; order < orders; ++order)
		{
			REQUIRE(
			    Admit(
			        actor, kPlace,
			        Manifest::Trade::Place{std::string{kOre}, std::string{kFurs}, side, 1, limit})
			        .status == Receipt::Status::Queued);
			if ((order + 1) % Manifest::Command::kPendingPerActor == 0)
				world.Step();
		}
		world.Step();
	}

	Result Cancel(const Actor &actor, const std::string &orderId)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		REQUIRE(
		    Admit(actor, kCancel, Manifest::Trade::Cancel{orderId}).status == Receipt::Status::Queued);
		world.Step();
		return Outcome(actor, sequence);
	}

	std::int64_t Balance(Kind kind, std::string_view id) const
	{
		const entt::entity entity = identities.Find(kind, std::string{id});
		REQUIRE(entity != entt::null);
		return world.Registry().get<const World::Account>(entity).balance;
	}

	// The owner's holding of the item at the venue, empty where it holds none.
	World::Holding Held(Kind owner, std::string_view id, std::string_view item = kFurs) const
	{
		const entt::entity entity = identities.Find(owner, std::string{id});
		REQUIRE(entity != entt::null);
		for (const World::Holding &holding : world.Registry().get<const World::Stock>(entity).holdings)
			if (holding.venueId == kOre && holding.itemId == item)
				return holding;
		return {std::string{kOre}, std::string{item}, 0, 0};
	}

	void Give(std::string_view company, std::uint64_t totes, std::string_view item = kFurs)
	{
		world.Registry()
		    .get<World::Stock>(identities.Find(Kind::Company, std::string{company}))
		    .holdings = {{std::string{kOre}, std::string{item}, totes, 0}};
	}

	// The order an actor's request placed, if it rests, by ID.
	std::optional<std::pair<std::string, World::Order>> Placed(
	    std::string_view company, std::uint64_t request) const
	{
		for (const auto [entity, identity, order] :
		     world.Registry().view<const World::Identity, const World::Order>().each())
			if (order.ownerId == company && order.request == request)
				return std::pair{identity.id, order};
		return std::nullopt;
	}

	Business Now() const
	{
		Business now;
		const entt::registry &registry = world.Registry();
		for (const auto [entity, identity, account] :
		     registry.view<const World::Identity, const World::Account>().each())
			now.balances[{identity.kind, identity.id}] = account.balance;
		for (const auto [entity, identity, stock] :
		     registry.view<const World::Identity, const World::Stock>().each())
			now.stocks[{identity.kind, identity.id}] = stock;
		for (const auto [entity, identity, order] :
		     registry.view<const World::Identity, const World::Order>().each())
			now.orders[identity.id] = order;
		return now;
	}

	// Credits anywhere: in accounts and in buy orders' escrow.
	std::int64_t Credits() const
	{
		std::int64_t total = 0;
		for (const auto [entity, account] : world.Registry().view<const World::Account>().each())
			total += account.balance;
		for (const auto [entity, order] : world.Registry().view<const World::Order>().each())
			total += order.escrow;
		return total;
	}

	// Totes of the item anywhere at the venue, available and reserved.
	std::uint64_t Totes(std::string_view item = kFurs) const
	{
		std::uint64_t total = 0;
		for (const auto [entity, stock] : world.Registry().view<const World::Stock>().each())
			for (const World::Holding &holding : stock.holdings)
				if (holding.itemId == item)
					total += holding.available + holding.reserved;
		return total;
	}

	SimWorld world{1.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};
} // namespace


TEST_CASE(
    "A buy fills against a resting sell at the maker's price, fees to the venue, the rest resting")
{
	Hosted hosted;
	hosted.Give("CompanyB", 2);
	// A tote of furs is 18.75 of them: two at 100 credits are 3,750, a fee of 15 on each side.
	REQUIRE(hosted.Place(kBea, Side::Sell, 2, Credits(100)) == Result::Succeeded);
	CHECK(
	    hosted.Held(Kind::Company, "CompanyB") == World::Holding{"VenueOreHighport", "ItemFurs", 0, 2});
	const std::int64_t credits = hosted.Credits();
	const std::uint64_t totes = hosted.Totes();

	// Ada bids 120 for three: 2,250 a tote and a fee of 9, 6,777 in escrow.
	REQUIRE(hosted.Place(kAda, Side::Buy, 3, Credits(120)) == Result::Succeeded);
	CHECK(
	    hosted.Held(Kind::Company, "CompanyA") == World::Holding{"VenueOreHighport", "ItemFurs", 2, 0});
	CHECK(hosted.Held(Kind::Company, "CompanyB").reserved == 0);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyB") == 25'000 + 3'750 - 15);
	CHECK(hosted.Balance(Kind::VenueAccount, kOre) == 30);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == 25'000 - 6'777);
	CHECK_FALSE(hosted.Placed("CompanyB", 1));
	const auto rest = hosted.Placed("CompanyA", 1);
	REQUIRE(rest);
	CHECK(rest->second.totes == 3);
	CHECK(rest->second.remaining == 1);
	CHECK(rest->second.escrow == 6'777 - 3'765);
	CHECK(hosted.Credits() == credits);
	CHECK(hosted.Totes() == totes);

	// Cancelled, the escrow left comes back: Ada paid for two totes and the fee, nothing more.
	REQUIRE(hosted.Cancel(kBea, rest->first) == Result::NotAuthorised);
	REQUIRE(hosted.Cancel(kAda, rest->first) == Result::Succeeded);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == 25'000 - 3'765);
	CHECK_FALSE(hosted.Placed("CompanyA", 1));
	CHECK(hosted.Cancel(kAda, rest->first) == Result::UnknownTarget);
	CHECK(hosted.Credits() == credits);
}


TEST_CASE("A sell's reserved totes cannot load, and come back when it is cancelled")
{
	Hosted hosted;
	hosted.Give("CompanyA", 4);
	entt::registry &registry = hosted.world.Registry();
	const entt::entity ship = hosted.identities.Create(registry, Kind::Ship, "Ship1");
	registry.emplace<World::Ship>(ship, "CompanyA", "HullLoneTraderStarter", std::string{kOre});
	registry.emplace<World::Cargo>(ship);
	REQUIRE(hosted.Place(kAda, Side::Sell, 3, Credits(100)) == Result::Succeeded);
	const auto load = [&](std::uint64_t totes)
	{
		const std::uint64_t sequence = hosted.requests.NextSequence(kAda);
		hosted.Admit(
		    kAda, Manifest::Company::kLoad,
		    Manifest::Company::Move{"Ship1", std::string{kOre}, std::string{kFurs}, totes});
		hosted.world.Step();
		return hosted.Outcome(kAda, sequence);
	};
	CHECK(load(2) == Result::InsufficientStock);
	CHECK(load(1) == Result::Succeeded);
	REQUIRE(hosted.Cancel(kAda, hosted.Placed("CompanyA", 1)->first) == Result::Succeeded);
	CHECK(
	    hosted.Held(Kind::Company, "CompanyA") == World::Holding{"VenueOreHighport", "ItemFurs", 3, 0});
}


TEST_CASE("Within a price the older order fills first, and a better price before both")
{
	Hosted hosted;
	hosted.AddCompany("Cy", "CompanyC");
	const Actor cy{ActorKind::Principal, "Cy"};
	hosted.Give("CompanyA", 1);
	hosted.Give("CompanyB", 1);
	hosted.world.Registry()
	    .get<World::Stock>(hosted.identities.Find(Kind::Company, "CompanyC"))
	    .holdings = {{std::string{kOre}, std::string{kFurs}, 1, 0}};
	// Bea's first at 100, then Ada's at 100, then Cy's under both at 90.
	REQUIRE(hosted.Place(kBea, Side::Sell, 1, Credits(100)) == Result::Succeeded);
	REQUIRE(hosted.Place(kAda, Side::Sell, 1, Credits(100)) == Result::Succeeded);
	REQUIRE(hosted.Place(cy, Side::Sell, 1, Credits(90)) == Result::Succeeded);

	hosted.AddCompany("Di", "CompanyD");
	const Actor di{ActorKind::Principal, "Di"};
	REQUIRE(hosted.Place(di, Side::Buy, 2, Credits(100)) == Result::Succeeded);
	CHECK_FALSE(hosted.Placed("CompanyC", 1));
	CHECK_FALSE(hosted.Placed("CompanyB", 1));
	CHECK(hosted.Placed("CompanyA", 1));
	// 90 for one tote then 100 for the other: 1,688 and 1,875, a fee of 7 and 8.
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyD") == 25'000 - 1'688 - 7 - 1'875 - 8);
}


TEST_CASE("An order matches until the step it expires at, and that step's sweep gives it back")
{
	Hosted hosted;
	hosted.Give("CompanyB", 2);
	REQUIRE(hosted.Place(kBea, Side::Sell, 2, Credits(100)) == Result::Succeeded);
	const auto placed = hosted.Placed("CompanyB", 1);
	REQUIRE(placed);
	const std::string id = placed->first;
	const World::Order order = placed->second;
	// Placed in step 0 with a day's life at one-second steps.
	CHECK(order.expiresStep == 86'400);
	CHECK(order.sequence == 1);

	// Brought to the step before: it still trades.
	auto at = [&](std::uint64_t expires)
	{
		hosted.world.Registry().get<World::Order>(hosted.identities.Find(Kind::Order, id)).expiresStep =
		    expires;
	};
	at(hosted.world.Clock().Step() + 1);
	REQUIRE(hosted.Place(kAda, Side::Buy, 1, Credits(100)) == Result::Succeeded);
	CHECK(hosted.Held(Kind::Company, "CompanyA").available == 1);

	// At its step a buy runs before the sweep, and finds nothing to fill: it rests instead.
	REQUIRE(hosted.Placed("CompanyB", 1));
	at(hosted.world.Clock().Step());
	const std::int64_t before = hosted.Balance(Kind::CompanyAccount, "CompanyA");
	REQUIRE(hosted.Place(kAda, Side::Buy, 1, Credits(100)) == Result::Succeeded);
	CHECK(hosted.Held(Kind::Company, "CompanyA").available == 1);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == before - 1'883);
	CHECK_FALSE(hosted.Placed("CompanyB", 1));
	CHECK(
	    hosted.Held(Kind::Company, "CompanyB") == World::Holding{"VenueOreHighport", "ItemFurs", 1, 0});
}


TEST_CASE("A refused order writes nothing a trade would")
{
	Hosted hosted;
	hosted.Give("CompanyA", 5);
	hosted.Give("CompanyB", 5);
	REQUIRE(hosted.Place(kBea, Side::Sell, 1, Credits(100)) == Result::Succeeded);
	const Business before = hosted.Now();
	const std::int64_t huge = std::numeric_limits<std::int64_t>::max();
	const struct
	{
		Side side;
		std::uint64_t totes;
		std::int64_t limit;
		std::string_view item;
		std::string_view venue;
		Result expected;
	} refused[]{
	    {Side::Buy, 1, Credits(100), "ItemRice", kOre, Result::UnknownTarget},
	    {Side::Buy, 1, Credits(100), kFurs, "VenueNowhere", Result::UnknownTarget},
	    {Side::Buy, 0, Credits(100), kFurs, kOre, Result::InvalidQuantity},
	    {Side::Buy, Manifest::Core::kMaxWarehouseTotes + 1, Credits(100), kFurs, kOre,
	     Result::InvalidQuantity},
	    {Side::Buy, 1, 0, kFurs, kOre, Result::InvalidPrice},
	    {Side::Sell, 1, -1, kFurs, kOre, Result::InvalidPrice},
	    {Side::Buy, 100'000, huge, kFurs, kOre, Result::InvalidPrice},
	    {Side::Buy, 14, Credits(100), kFurs, kOre, Result::InsufficientFunds},
	    {Side::Sell, 6, Credits(100), kFurs, kOre, Result::InsufficientStock},
	};
	for (const auto &[side, totes, limit, item, venue, expected] : refused)
	{
		CAPTURE(totes);
		CAPTURE(limit);
		CHECK(hosted.Place(kAda, side, totes, limit, item, venue) == expected);
		CHECK(hosted.Now() == before);
	}
	const Actor nobody{ActorKind::Principal, "Nobody"};
	hosted.world.Registry().emplace<World::Principal>(
	    hosted.identities.Create(hosted.world.Registry(), Kind::Principal, "Nobody"), "Nobody");
	CHECK(hosted.Place(nobody, Side::Buy, 1, Credits(100)) == Result::NotAuthorised);
	CHECK(hosted.Now() == before);
}


TEST_CASE("Two buys in one step compete for one account's funds, in admission order")
{
	Hosted hosted;
	// 13 totes at 100 are 24,479 with fees, of Ada's 25,000.
	const Manifest::Trade::Place thirteen{
	    std::string{kOre}, std::string{kFurs}, Side::Buy, 13, Credits(100)};
	REQUIRE(hosted.Admit(kAda, kPlace, thirteen).status == Receipt::Status::Queued);
	REQUIRE(hosted.Admit(kAda, kPlace, thirteen).status == Receipt::Status::Queued);
	hosted.world.Step();
	CHECK(hosted.Outcome(kAda, 1) == Result::Succeeded);
	CHECK(hosted.Outcome(kAda, 2) == Result::InsufficientFunds);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == 25'000 - 24'479);
}


TEST_CASE("A side of the book holds 64 resting orders; one past them that would rest is BookFull")
{
	Hosted hosted;
	hosted.Give("CompanyB", 100);
	hosted.Rest(kBea, Side::Sell, kMaxRestingOrders, Credits(200));
	const Business full = hosted.Now();
	CHECK(full.orders.size() == kMaxRestingOrders);
	CHECK(hosted.Place(kBea, Side::Sell, 1, Credits(200)) == Result::BookFull);
	CHECK(hosted.Now() == full);
	// One that fills whole rests nothing, so the full side does not refuse it; nor does the other.
	CHECK(hosted.Place(kAda, Side::Buy, 1, Credits(200)) == Result::Succeeded);
	CHECK(hosted.Place(kAda, Side::Buy, 1, Credits(100)) == Result::Succeeded);
}


TEST_CASE("One pass settles at most 64 fills, the remainder resting")
{
	Hosted hosted;
	hosted.Give("CompanyB", 70);
	hosted.world.Registry()
	    .get<World::Account>(hosted.identities.Find(Kind::CompanyAccount, "CompanyA"))
	    .balance = 1'000'000;
	hosted.Rest(kBea, Side::Sell, kMaxRestingOrders, Credits(100));
	REQUIRE(hosted.Place(kAda, Side::Buy, 65, Credits(100)) == Result::Succeeded);
	CHECK(hosted.Held(Kind::Company, "CompanyA").available == 64);
	CHECK(hosted.Placed("CompanyA", 1)->second.remaining == 1);
}


TEST_CASE(
    "Each broker quotes around its native price at the start and requotes each turn, its orders not "
    "piling up")
{
	Hosted hosted{true};
	// Ore makes furs and Brine wants them: Ore's native price, 7.794229, Brine's, 138.034...
	const Business start = hosted.Now();
	std::size_t ore = 0;
	std::optional<World::Order> ask;
	for (const auto &[id, order] : start.orders)
	{
		CHECK(order.ownerKind == Kind::Broker);
		ore += order.venueId == kOre ? 1 : 0;
		if (order.venueId == kOre && order.itemId == kFurs && order.side == Side::Sell)
			ask = order;
	}
	REQUIRE(ask);
	// All 21 totes it holds, 5% over the native price.
	CHECK(ask->totes == 21);
	CHECK(ask->limit == 8'183'941);
	CHECK(hosted.Held(Kind::Broker, kOre).reserved == 21);
	// An ask and a bid for each of its eight goods, gold's bid beyond its share of the funds.
	CHECK(ore == 15);

	// Ada lifts two totes of the ask: 18.75 furs a tote at 8.183941, 307 credits, 2 in fees.
	REQUIRE(hosted.Place(kAda, Side::Buy, 2, Credits(9)) == Result::Succeeded);
	CHECK(hosted.Held(Kind::Company, "CompanyA").available == 2);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == 25'000 - 307 - 2);

	// The turn's last step is 59: then each broker cancels and posts afresh.
	while (hosted.world.Clock().Step() < 59)
		hosted.world.Step();
	const Business before = hosted.Now();
	hosted.world.Step();
	const Business after = hosted.Now();
	CHECK(after.orders.size() == before.orders.size());
	for (const auto &[id, order] : after.orders)
		CHECK_FALSE(before.orders.contains(id));
	CHECK(hosted.Held(Kind::Broker, kOre).reserved == 19);
}


TEST_CASE("The step runs the expiry sweep, then the brokers")
{
	Hosted hosted;
	const auto order = hosted.world.Scheduler().Order(Jettison::Ecs::TickPhase::Simulation);
	CHECK(order == std::vector<std::string>{std::string{kExpiry}, std::string{kBrokers}});
}


TEST_CASE("An order life or a turn that is not whole steps refuses to start")
{
	const Manifest::Content::ContentSet &content = Trading();
	RegisterTypes();
	SimWorld world{0.7, 0};
	World::Identities &identities = World::RegisterWorld(world);
	Manifest::Command::Requests &requests = Manifest::Command::RegisterRequests(world, identities);
	CHECK_THROWS_WITH_AS(
	    Register(world, requests, identities, content), doctest::Contains("orderLifeSeconds"),
	    std::invalid_argument);
}


TEST_CASE("A save with an open order loads and carries on as the world it came from")
{
	Hosted hosted{true};
	hosted.Give("CompanyB", 3);
	REQUIRE(hosted.Place(kBea, Side::Sell, 3, Credits(500)) == Result::Succeeded);
	const std::vector<std::byte> save = hosted.world.Save();
	Hosted loaded;
	loaded.world.Load(save);
	CHECK(loaded.Now() == hosted.Now());
	REQUIRE(loaded.Place(kAda, Side::Buy, 1, Credits(500)) == Result::Succeeded);
	REQUIRE(hosted.Place(kAda, Side::Buy, 1, Credits(500)) == Result::Succeeded);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	// A holding that reserves more than its sell orders do is refused.
	hosted.world.Registry()
	    .get<World::Stock>(hosted.identities.Find(Kind::Company, "CompanyB"))
	    .holdings[0]
	    .reserved += 1;
	CHECK_THROWS_WITH_AS(
	    loaded.world.Load(hosted.world.Save()), doctest::Contains("for sell orders of"),
	    Jettison::Serialise::FormatError);
	hosted.world.Registry()
	    .get<World::Stock>(hosted.identities.Find(Kind::Company, "CompanyB"))
	    .holdings[0]
	    .reserved -= 1;

	// So is an order of an owner the world does not hold.
	for (const auto [entity, order] : hosted.world.Registry().view<World::Order>().each())
		if (order.ownerId == "CompanyB")
			order.ownerId = "CompanyZ";
	CHECK_THROWS_WITH_AS(
	    loaded.world.Load(hosted.world.Save()), doctest::Contains("names an owner or venue"),
	    Jettison::Serialise::FormatError);
	CHECK(loaded.Now() != Business{});
}
