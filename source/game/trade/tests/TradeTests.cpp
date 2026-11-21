#include "TestHelpers.h"

#include <algorithm>
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
#include <manifest/trade/Economy.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
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
// The trading scenario with its economy taken out (EconomyTests.cpp has it), so that a case sees only
// the orders it places: no employer makes anything and no region's households want anything.
const Manifest::Content::ContentSet &Trading()
{
	static const Manifest::Content::ContentSet loaded = []
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		Manifest::Content::ContentSet set =
		    Manifest::Content::LoadScenario(files, definitions, "trading");
		Manifest::Content::ScenarioDefinition &scenario = set.scenarios.at(set.scenario);
		for (Manifest::Content::RegionDefinition &region : scenario.regions)
			region.population = 0.0;
		for (Manifest::Content::VenueDefinition &venue : scenario.venues)
			for (Manifest::Content::MarketEntry &entry : venue.market)
				entry.regionId.clear();
		return set;
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
		const Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, Trading());
		Manifest::Company::RegisterCommands(world, requests, identities, fleet, Trading());
		Register(world, requests, identities, Trading());
		World::CreateInitialState(world, identities, Trading());
		Manifest::Travel::OpenStations(world, fleet);
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
	    {Side::Buy, 1, Credits(100), "ItemBronze", kOre, Result::UnknownTarget},
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


TEST_CASE("Each broker posts a ladder about its stock and requotes each turn, its orders not piling up")
{
	Hosted hosted{true};
	// Ore makes furs and Brine wants them: Ore's native price, 7.794229, its target the 21 totes it is
	// seeded with, in rungs of 3, each priced at its middle on 7.794229 x 4 x 21 / (21 + 3h), the
	// asks 5% over it rounded up and the bids 5% under rounded down.
	const Business start = hosted.Now();
	std::map<std::tuple<std::string, std::string, Side>, std::vector<World::Order>> ladders;
	for (const auto &[id, order] : start.orders)
	{
		CHECK(order.ownerKind == Kind::Broker);
		ladders[{order.venueId, order.itemId, order.side}].push_back(order);
	}
	for (const auto &[key, ladder] : ladders)
		CHECK(ladder.size() <= kLevels);
	const auto limits = [&](Side side)
	{
		std::vector<std::pair<std::uint64_t, std::int64_t>> rungs;
		for (const World::Order &order : ladders[{std::string{kOre}, std::string{kFurs}, side}])
			rungs.emplace_back(order.totes, order.limit);
		std::ranges::sort(
		    rungs, {},
		    [&](const auto &rung) { return side == Side::Sell ? rung.second : -rung.second; });
		return rungs;
	};
	// All 21 totes it holds, climbing as they sell it down: the first at 19.5 totes, 8.647183.
	CHECK(
	    limits(Side::Sell) == std::vector<std::pair<std::uint64_t, std::int64_t>>{
	                              {3, 8'647'183},
	                              {3, 9'751'078},
	                              {3, 11'178'066},
	                              {3, 13'094'305},
	                              {3, 15'803'472},
	                              {3, 19'926'116},
	                              {3, 26'958'863}});
	// Eight rungs of bids, falling as they buy it up: the first at 22.5 totes, 7.028016.
	CHECK(
	    limits(Side::Buy) == std::vector<std::pair<std::uint64_t, std::int64_t>>{
	                             {3, 7'028'016},
	                             {3, 6'379'276},
	                             {3, 5'840'182},
	                             {3, 5'385'103},
	                             {3, 4'995'819},
	                             {3, 4'659'022},
	                             {3, 4'364'768},
	                             {3, 4'105'475}});
	CHECK(hosted.Held(Kind::Broker, kOre).reserved == 21);

	// Ada lifts six totes, walking two rungs: 18.75 furs a tote, 486 and 548 credits, 2 and 3 in fees.
	REQUIRE(hosted.Place(kAda, Side::Buy, 6, Credits(10)) == Result::Succeeded);
	CHECK(hosted.Held(Kind::Company, "CompanyA").available == 6);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == 25'000 - 486 - 2 - 548 - 3);

	// The turn's last step is 59: then each broker cancels and posts afresh.
	while (hosted.world.Clock().Step() < 59)
		hosted.world.Step();
	const Business before = hosted.Now();
	hosted.world.Step();
	const Business after = hosted.Now();
	CHECK(after.orders.size() == before.orders.size());
	for (const auto &[id, order] : after.orders)
		CHECK_FALSE(before.orders.contains(id));
	CHECK(hosted.Held(Kind::Broker, kOre).reserved == 15);

	// Selling them back walks its bids from 16.5 totes, 8.822403 and 7.823641: 496 and 440 credits, 2
	// in fees each. The round trip meets the same middles of the curve, and loses the spread.
	REQUIRE(hosted.Place(kAda, Side::Sell, 6, 1) == Result::Succeeded);
	CHECK(hosted.Held(Kind::Company, "CompanyA").available == 0);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyA") == 24'893);
}


TEST_CASE("The step runs the expiry sweep, the economy, then the brokers, ahead of the movement")
{
	Hosted hosted;
	std::vector<std::string> order =
	    hosted.world.Scheduler().Order(Jettison::Ecs::TickPhase::Simulation);
	REQUIRE(order.size() > 3);
	order.resize(4);
	CHECK(
	    order == std::vector<std::string>{
	                 std::string{kExpiry}, std::string{kEconomy}, std::string{kBrokers}, "spatial"});
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
	CHECK_THROWS_WITH_AS(
	    SalesWindowSteps(world, content), doctest::Contains("salesWindowSeconds"),
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


// ─── Observation (W07) ───

namespace
{
const Actor kHost{ActorKind::Administrator, "Host"};
constexpr std::string_view kBrine = "VenueBrineHighport";


const Manifest::Content::VenueDefinition &Venue(std::string_view venueId)
{
	const Manifest::Content::ScenarioDefinition &scenario = Trading().scenarios.at(Trading().scenario);
	const auto venue =
	    std::ranges::find(scenario.venues, venueId, &Manifest::Content::VenueDefinition::venueId);
	REQUIRE(venue != scenario.venues.end());
	return *venue;
}


// A new player, provisioned by the host in a step of its own: Company<n>, its Ship<n> docked at Ore.
void Provision(Hosted &hosted, const std::string &principal)
{
	REQUIRE(
	    hosted
	        .Admit(
	            kHost, Manifest::Company::kProvision,
	            Manifest::Company::Provision{principal, principal, principal})
	        .status == Receipt::Status::Queued);
	hosted.world.Step();
}


// Where a ship's journey would leave it: docked at the venue, or at none.
void Dock(Hosted &hosted, std::string_view ship, std::string_view venue)
{
	hosted.world.Registry()
	    .get<World::Ship>(hosted.identities.Find(Kind::Ship, std::string{ship}))
	    .dockedAt = venue;
}


const World::Reports &ReportsOf(const Hosted &hosted, std::string_view principal)
{
	const entt::entity entity = hosted.identities.Find(Kind::Principal, std::string{principal});
	REQUIRE(entity != entt::null);
	return hosted.world.Registry().get<const World::Reports>(entity);
}


World::Report ReportOf(const Hosted &hosted, std::string_view principal, std::string_view venue)
{
	const std::vector<World::Report> &reports = ReportsOf(hosted, principal).reports;
	const auto report = std::ranges::find(reports, venue, &World::Report::venueId);
	REQUIRE(report != reports.end());
	return *report;
}


World::Line LineOf(const World::Report &report, std::string_view item)
{
	const auto line = std::ranges::find(report.lines, item, &World::Line::itemId);
	REQUIRE(line != report.lines.end());
	return *line;
}


// The venue's report as the world holds it now, observed at the step.
World::Report Current(const Hosted &hosted, std::string_view venue, std::uint64_t observed)
{
	return Capture(
	    hosted.world.Registry(), hosted.identities, Venue(venue), observed,
	    SalesWindowSteps(hosted.world, Trading()));
}
} // namespace


TEST_CASE("A new player sees their own station live, and the rest of their system as it was at spawn")
{
	Hosted hosted{true};
	Provision(hosted, "Cai");
	REQUIRE(hosted.world.Clock().Step() == 1);

	// Docked at Ore, observed as the step ended; Brine a starting report from the step's start.
	CHECK(ReportOf(hosted, "Cai", kOre) == Current(hosted, kOre, 1));
	World::Report spawn = Current(hosted, kBrine, 0);
	spawn.source = World::Source::Starting;
	const World::Report brine = ReportOf(hosted, "Cai", kBrine);
	CHECK(brine == spawn);
	CHECK(ReportsOf(hosted, "Cai").reports.size() == 2);
	CHECK(LineOf(brine, "ItemSalt").Ask() > 0);
	CHECK(LineOf(brine, "ItemSalt").sold == 0);
	CHECK(LineOf(brine, "ItemSalt").lastPrice == 0);

	// A trade at Brine, unseen: the report and its observation stay as they were.
	const World::Line salt = LineOf(brine, "ItemSalt");
	REQUIRE(hosted.Place(kAda, Side::Buy, 1, salt.Ask(), "ItemSalt", kBrine) == Result::Succeeded);
	const std::uint64_t traded = hosted.world.Clock().Step() - 1;
	CHECK(ReportOf(hosted, "Cai", kBrine) == brine);
	CHECK(ReportOf(hosted, "Cai", kOre).observed == hosted.world.Clock().Step());

	// Docked at Brine it is live there, the trade in it once; Ore stays as it was when the ship left.
	Dock(hosted, "Ship1", kBrine);
	const std::uint64_t left = hosted.world.Clock().Step();
	hosted.world.Step();
	const World::Report live = ReportOf(hosted, "Cai", kBrine);
	CHECK(live.source == World::Source::Docked);
	CHECK(live.observed == hosted.world.Clock().Step());
	CHECK(LineOf(live, "ItemSalt").sold == 1);
	CHECK(LineOf(live, "ItemSalt").lastPrice == salt.Ask());
	CHECK(LineOf(live, "ItemSalt").lastStep == traded);
	const World::Report ore = ReportOf(hosted, "Cai", kOre);
	CHECK(ore.observed == left);
	REQUIRE(hosted.Place(kAda, Side::Buy, 1, LineOf(ore, "ItemFurs").Ask()) == Result::Succeeded);
	CHECK(ReportOf(hosted, "Cai", kOre) == ore);
	CHECK(LineOf(Current(hosted, kOre, 0), "ItemFurs").sold == 1);
}


TEST_CASE("A fleet's second ship keeps a station live; as the last leaves, the step's trades go unseen")
{
	Hosted hosted{true};
	Provision(hosted, "Cai");
	Provision(hosted, "Dee");
	entt::registry &registry = hosted.world.Registry();
	const entt::entity second = hosted.identities.Create(registry, Kind::Ship, "ShipX");
	registry.emplace<World::Ship>(
	    second, "Company1", Trading().scenarios.at(Trading().scenario).starterHull, std::string{kOre});

	Dock(hosted, "Ship1", "");
	hosted.world.Step();
	CHECK(ReportOf(hosted, "Cai", kOre).observed == hosted.world.Clock().Step());

	// The last of Cai's ships leaves in the step Ada buys there.
	const World::Report before = ReportOf(hosted, "Cai", kOre);
	Dock(hosted, "ShipX", "");
	REQUIRE(hosted.Place(kAda, Side::Buy, 2, LineOf(before, "ItemFurs").Ask()) == Result::Succeeded);
	CHECK(ReportOf(hosted, "Cai", kOre) == before);
	const World::Report dees = ReportOf(hosted, "Dee", kOre);
	CHECK(dees.observed == hosted.world.Clock().Step());
	CHECK(LineOf(dees, "ItemFurs").sold == 2);
	CHECK(dees != ReportOf(hosted, "Cai", kOre));
}


TEST_CASE("A report's sales are its window's fills, each once, and a frozen report keeps its window")
{
	Hosted hosted{true};
	Provision(hosted, "Cai");
	Provision(hosted, "Dee");
	const std::uint64_t window = SalesWindowSteps(hosted.world, Trading());
	REQUIRE(window == 3600);

	const World::Report quoted = ReportOf(hosted, "Dee", kOre);
	const std::int64_t furs = LineOf(quoted, "ItemFurs").Ask();
	REQUIRE(hosted.Place(kAda, Side::Buy, 2, furs) == Result::Succeeded);
	const std::uint64_t first = hosted.world.Clock().Step() - 1;
	REQUIRE(hosted.Place(kAda, Side::Buy, 1, furs) == Result::Succeeded);
	REQUIRE(
	    hosted.Place(kAda, Side::Buy, 1, LineOf(quoted, "ItemCopper").Ask(), "ItemCopper") ==
	    Result::Succeeded);
	const std::uint64_t last = hosted.world.Clock().Step() - 1;

	// The buyer's and the seller's side of a fill are one sale.
	World::Report dees = ReportOf(hosted, "Dee", kOre);
	CHECK(LineOf(dees, "ItemFurs").sold == 3);
	CHECK(LineOf(dees, "ItemCopper").sold == 1);
	CHECK(LineOf(dees, "ItemTin").sold == 0);
	CHECK(dees.since == 0);

	// Cai's ship leaves, and Cai's report stops with it.
	Dock(hosted, "Ship1", "");
	hosted.world.Step();
	const World::Report frozen = ReportOf(hosted, "Cai", kOre);

	// The window moves past the first fill, then past every fill, the last sale kept.
	while (hosted.world.Clock().Step() < first + window + 1)
		hosted.world.Step();
	dees = ReportOf(hosted, "Dee", kOre);
	CHECK(dees.since == first + 1);
	CHECK(LineOf(dees, "ItemFurs").sold == 1);
	while (hosted.world.Clock().Step() < last + window + 1)
		hosted.world.Step();
	dees = ReportOf(hosted, "Dee", kOre);
	CHECK(LineOf(dees, "ItemFurs").sold == 0);
	CHECK(LineOf(dees, "ItemFurs").lastStep == last - 1);
	CHECK(LineOf(dees, "ItemFurs").lastPrice == furs);
	CHECK(LineOf(dees, "ItemCopper").lastStep == last);

	CHECK(ReportOf(hosted, "Cai", kOre) == frozen);
	CHECK(LineOf(frozen, "ItemFurs").sold == 3);
}


TEST_CASE("A new player's starting reports are of their own system and the ones directly connected")
{
	Manifest::Content::ScenarioDefinition scenario;
	scenario.systems = {
	    {"SystemA", "A", {{"SystemB"}}},
	    {"SystemB", "B", {{"SystemD"}}},
	    {"SystemC", "C", {{"SystemA"}}},
	    {"SystemD", "D", {}}};
	for (const auto &[venue, system] :
	     {std::pair{"VenueA1", "SystemA"},
	      {"VenueB1", "SystemB"},
	      {"VenueA2", "SystemA"},
	      {"VenueC1", "SystemC"},
	      {"VenueD1", "SystemD"}})
	{
		Manifest::Content::VenueDefinition &added = scenario.venues.emplace_back();
		added.venueId = venue;
		added.systemId = system;
	}
	CHECK(
	    StartingVenues(scenario, "VenueA1") ==
	    std::vector<std::string>{"VenueB1", "VenueA2", "VenueC1"});
	CHECK(StartingVenues(scenario, "VenueC1") == std::vector<std::string>{"VenueA1", "VenueA2"});
	CHECK(StartingVenues(scenario, "VenueD1") == std::vector<std::string>{"VenueB1"});
	CHECK(StartingVenues(scenario, "VenueNowhere").empty());
}


TEST_CASE("A save keeps each player's reports and each market's sales, and refuses a report of nowhere")
{
	Hosted hosted{true};
	Provision(hosted, "Cai");
	REQUIRE(
	    hosted.Place(kAda, Side::Buy, 1, LineOf(ReportOf(hosted, "Cai", kOre), "ItemFurs").Ask()) ==
	    Result::Succeeded);
	Dock(hosted, "Ship1", "");
	hosted.world.Step();
	const std::vector<std::byte> save = hosted.world.Save();

	Hosted loaded;
	loaded.world.Load(save);
	CHECK(ReportsOf(loaded, "Cai") == ReportsOf(hosted, "Cai"));
	CHECK(Current(loaded, kOre, 0) == Current(hosted, kOre, 0));
	Dock(loaded, "Ship1", kOre);
	Dock(hosted, "Ship1", kOre);
	loaded.world.Step();
	hosted.world.Step();
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(LineOf(ReportOf(loaded, "Cai", kOre), "ItemFurs").sold == 1);

	entt::registry &registry = hosted.world.Registry();
	registry.get<World::Reports>(hosted.identities.Find(Kind::Principal, "Cai")).reports[0].venueId =
	    "VenueNowhere";
	CHECK_THROWS_WITH_AS(
	    loaded.world.Load(hosted.world.Save()), doctest::Contains("a venue the world does not hold"),
	    Jettison::Serialise::FormatError);
}
