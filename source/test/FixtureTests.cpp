// The trading fixture's whole loop through the public command and query boundary (W09,
// docs/stage-4-trading-plan.md, "Acceptance scenario and delivery"): bought at Ore, loaded, flown to
// Brine through the engine's departure, cruise and docking, resupplied from the hold, unloaded, sold
// and the ship upgraded, with the economy, life support and observations running throughout. Nothing
// after the bootstrap writes the world but a command.

#include "TestHelpers.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/space/Cruise.h>

#include <manifest/company/Company.h>
#include <manifest/host/Start.h>
#include <manifest/policy/Policy.h>
#include <manifest/query/Query.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/State.h>

using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
using Manifest::Query::CompanyView;
using Manifest::Query::MarketView;
using Manifest::Query::ShipView;
using Manifest::World::Kind;
using Manifest::World::Leg;
using Manifest::World::Side;
namespace Company = Manifest::Company;
namespace Trade = Manifest::Trade;
namespace Travel = Manifest::Travel;
namespace World = Manifest::World;

namespace
{
const Actor kHost{ActorKind::Administrator, "Host"};
const Actor kAda{ActorKind::Principal, "Ada"};
constexpr std::string_view kOre = "VenueOreHighport";
constexpr std::string_view kBrine = "VenueBrineHighport";
// The loop's good, made at Ore and wanted at Brine, its best run for a starter (W19's balance).
constexpr std::string_view kGood = "ItemCopper";
constexpr std::string_view kOxygen = "ItemOxygen";
constexpr std::string_view kCartridges = "ItemCarbonScrubberCartridge";
// OfferCargoHolds' tier 1 (D-096), the one credit sink of the loop.
constexpr std::int64_t kHoldsCost = 27000;
// A journey from Ore to Brine on the power law is minutes, so this bounds one with room to spare.
constexpr std::uint64_t kJourneySteps = std::uint64_t{60} * 60 * 12;

// What a command of the stream did: the step it ran in, its result and the world's hash after it.
struct Outcome
{
	std::uint64_t step{0};
	std::string command;
	Result result{Result::Succeeded};
	std::uint64_t hash{0};

	bool operator==(const Outcome &) const = default;
};

// The credits the world holds: every account, a buy order's escrow and a batch's wages not yet paid.
std::int64_t Credits(const entt::registry &registry)
{
	std::int64_t total = 0;
	for (const auto &[entity, account] : registry.view<const World::Account>().each())
		total += account.balance;
	for (const auto &[entity, order] : registry.view<const World::Order>().each())
		total += order.escrow;
	for (const auto &[entity, production] : registry.view<const World::Production>().each())
		for (const World::Batch &batch : production.batches)
			total += batch.wages - batch.paid;
	return total;
}

// The whole totes of each item the world holds: every owner's holdings, reserved ones among them, and
// every hold's lots and the totes opened for a ship's life support, each still a tote.
std::map<std::string, std::int64_t> Totes(const entt::registry &registry)
{
	std::map<std::string, std::int64_t> totes;
	for (const auto &[entity, stock] : registry.view<const World::Stock>().each())
		for (const World::Holding &holding : stock.holdings)
			totes[holding.itemId] += static_cast<std::int64_t>(holding.available + holding.reserved);
	for (const auto &[entity, cargo] : registry.view<const World::Cargo>().each())
	{
		for (const World::Lot &lot : cargo.lots)
			totes[lot.itemId] += static_cast<std::int64_t>(lot.totes);
		for (const World::Opened &opened : cargo.opened)
			totes[opened.itemId] += 1;
	}
	return totes;
}

using Batches = std::map<std::pair<std::string, std::string>, World::Batch>;

Batches Underway(const entt::registry &registry)
{
	Batches batches;
	for (const auto &[entity, production] : registry.view<const World::Production>().each())
		for (const World::Batch &batch : production.batches)
			batches.emplace(std::pair{batch.venueId, batch.itemId}, batch);
	return batches;
}

// The totes of each item the batches before a step delivered in it: a batch that has gone, or been
// followed by the next, its work begun again.
std::map<std::string, std::int64_t> Delivered(const Batches &before, const Batches &after)
{
	std::map<std::string, std::int64_t> made;
	for (const auto &[key, batch] : before)
	{
		const auto next = after.find(key);
		if (next == after.end() || next->second.progress < batch.progress)
			made[batch.itemId] += static_cast<std::int64_t>(batch.totes);
	}
	return made;
}

std::vector<World::Pantry> Pantries(const entt::registry &registry)
{
	std::vector<World::Pantry> pantries;
	for (const auto &[entity, pantry] : registry.view<const World::Pantry>().each())
		pantries.push_back(pantry);
	return pantries;
}


// The trading scenario as the host starts it, from the staged directory or its pack under the overlay
// whose batches are minutes, stepped only through Step, which checks the ledger as it goes.
struct Hosted
{
	std::unique_ptr<Jettison::Host::Session> session;
	Jettison::Sim::SimWorld &world{session->World()};
	Manifest::Host::Services services{Manifest::Host::HostedServices(*session)};
	std::vector<Outcome> trace;
	// What the loop has spent on purpose: the credits it holds and these make what it started with.
	std::int64_t sunk{0};
	std::int64_t credits{0};
	// The first step at which credits or goods appeared or went unaccounted for.
	std::optional<std::uint64_t> creditBreach;
	std::optional<std::uint64_t> goodsBreach;
	// The totes batches delivered, and the steps households used goods up in: each source and sink the
	// ledger allows for, there to be seen happening.
	std::int64_t made{0};
	std::uint64_t consumptions{0};

	explicit Hosted(
	    const std::string &content,
	    std::vector<std::filesystem::path> overlays = {MANIFEST_FIXTURE_OVERLAY})
	    : session{Manifest::Host::Start({content, std::move(overlays), "trading"})},
	      credits{Credits(world.Registry())}
	{
	}

	// The fixture's bootstrap: Ada provisioned at Ore by the host, and her ship's cruise the drive's
	// power law Astralis keeps (D-088), so that Ore to Brine is minutes rather than the Manifest
	// mode's days (D-090).
	void Bootstrap()
	{
		REQUIRE(
		    Run(kHost, Company::kProvision, Company::kVersion,
		        Company::Provision{"Ada", "Ada", "Ada Co"}) == Result::Succeeded);
		world.Registry().get<Jettison::Space::CruiseDrive>(Ship()).profile.reset();
		// The company's starting credits are made by provisioning, so the ledger starts after it.
		credits = Credits(world.Registry());
		creditBreach.reset();
	}

	// After a load, the ledger is checked from the state loaded.
	void Load(const std::vector<std::byte> &save)
	{
		world.Load(save);
		credits = Credits(world.Registry());
	}

	void Step()
	{
		const entt::registry &registry = world.Registry();
		const std::map<std::string, std::int64_t> totes = Totes(registry);
		const Batches batches = Underway(registry);
		const std::vector<World::Pantry> pantries = Pantries(registry);

		world.Step();
		(void)world.Events().Take();

		const std::uint64_t step = world.Clock().Step();
		if (Credits(registry) + sunk != credits && !creditBreach)
			creditBreach = step;
		// Only a batch makes goods, and only the households' turn uses them up, when their pantries
		// take what they open.
		const std::map<std::string, std::int64_t> delivered = Delivered(batches, Underway(registry));
		const bool consumed = Pantries(registry) != pantries;
		consumptions += consumed ? 1 : 0;
		for (const auto &[item, count] : delivered)
			made += count;
		std::map<std::string, std::int64_t> now = Totes(registry);
		for (const auto &[item, count] : totes)
			now.try_emplace(item, 0);
		for (const auto &[item, count] : now)
		{
			const auto was = totes.find(item);
			const std::int64_t change = count - (was == totes.end() ? 0 : was->second);
			const auto found = delivered.find(item);
			const std::int64_t expected = found == delivered.end() ? 0 : found->second;
			if ((consumed ? change > expected : change != expected) && !goodsBreach)
				goodsBreach = step;
		}
	}

	// The request admitted as the actor's next and run in the next step, its outcome traced.
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
		trace.push_back(
		    {world.Clock().Step(), std::string{command}, completed->result, world.StateHash()});
		return completed->result;
	}

	Result Place(
	    std::string_view venueId, std::string_view itemId, Side side, std::uint64_t totes,
	    std::int64_t limit)
	{
		return Run(
		    kAda, Trade::kPlace, Trade::kVersion,
		    Trade::Place{std::string{venueId}, std::string{itemId}, side, totes, limit});
	}

	Result Move(
	    std::string_view command, std::string_view venueId, std::string_view itemId,
	    std::uint64_t totes)
	{
		return Run(
		    kAda, command, Company::kVersion,
		    Company::Move{"Ship1", std::string{venueId}, std::string{itemId}, totes});
	}

	Result Service(std::string_view command)
	{
		return Run(kAda, command, Company::kVersion, Company::Service{"Ship1"});
	}

	// Ada's company and her markets, as her query shows them.
	CompanyView Seen() const
	{
		const Manifest::Query::Snapshot snapshot =
		    Manifest::Query::Observe(world, services.identities, kAda);
		REQUIRE(snapshot.companies.size() == 1);
		return snapshot.companies[0];
	}

	ShipView Ship1() const
	{
		const CompanyView company = Seen();
		REQUIRE(company.ships.size() == 1);
		return company.ships[0];
	}

	MarketView Market(std::string_view venueId) const
	{
		for (const MarketView &market :
		     Manifest::Query::Observe(world, services.identities, kAda).markets)
			if (market.report.venueId == venueId)
				return market;
		FAIL("no report of " << venueId);
		return {};
	}

	// The good's line of Ada's report of the venue.
	World::Line Quote(std::string_view venueId, std::string_view itemId) const
	{
		for (const World::Line &line : Market(venueId).report.lines)
			if (line.itemId == itemId)
				return line;
		FAIL("no line of " << itemId << " at " << venueId);
		return {};
	}

	entt::entity Ship() const
	{
		return services.identities.Find(Kind::Ship, "Ship1");
	}

	// Steps until the ship's journey ends.
	void RunJourney()
	{
		for (std::uint64_t step = 0; step < kJourneySteps && !Ship1().journey->Ended(); ++step)
			Step();
		REQUIRE(Ship1().journey->Ended());
	}
};

// The company's totes of the item at the venue, available and reserved, 0 where it holds none.
World::Holding Held(const CompanyView &company, std::string_view venueId, std::string_view itemId)
{
	for (const World::Holding &holding : company.holdings)
		if (holding.venueId == venueId && holding.itemId == itemId)
			return holding;
	return {std::string{venueId}, std::string{itemId}, 0, 0};
}

std::uint64_t Aboard(const ShipView &ship, std::string_view itemId)
{
	for (const World::Lot &lot : ship.cargo)
		if (lot.itemId == itemId)
			return lot.totes;
	return 0;
}

// What a refused command must leave as it was: the company's credits, goods and orders and its ship's
// hold. Its clock, journey and life support move on with the step.
struct Business
{
	std::int64_t credits{0};
	std::vector<World::Holding> holdings;
	std::vector<World::Lot> cargo;
	std::vector<World::Opened> opened;
	std::size_t orders{0};

	explicit Business(const CompanyView &company)
	    : credits{company.credits}, holdings{company.holdings}, cargo{company.ships.at(0).cargo},
	      opened{company.ships.at(0).opened}, orders{company.orders.size()}
	{
	}

	bool operator==(const Business &) const = default;
};


// At Ore: bought at the asks of Ada's live report, loaded, a bid left resting at Brine, and the ship
// sent off, run until its cruise begins.
void ToCruise(Hosted &hosted)
{
	// At the start Ore is live, where the ship is docked, and Brine her starting report.
	CHECK(hosted.Market(kOre).live);
	CHECK(hosted.Market(kBrine).report.source == World::Source::Starting);
	CHECK_FALSE(hosted.Market(kBrine).live);

	// Bought: the goods are the company's at Ore, and the hold is as it was.
	const CompanyView start = hosted.Seen();
	CHECK(start.credits == 25000);
	REQUIRE(
	    hosted.Place(kOre, kGood, Side::Buy, 2, hosted.Quote(kOre, kGood).Ask()) == Result::Succeeded);
	CHECK(hosted.Seen().credits == 4804);
	REQUIRE(
	    hosted.Place(kOre, kOxygen, Side::Buy, 1, hosted.Quote(kOre, kOxygen).Ask()) ==
	    Result::Succeeded);
	REQUIRE(
	    hosted.Place(kOre, kCartridges, Side::Buy, 1, hosted.Quote(kOre, kCartridges).Ask()) ==
	    Result::Succeeded);
	const CompanyView bought = hosted.Seen();
	CHECK(Held(bought, kOre, kGood).available == 2);
	CHECK(Held(bought, kOre, kOxygen).available == 1);
	CHECK(Held(bought, kOre, kCartridges).available == 1);
	CHECK(bought.ships[0].cargo.empty());
	CHECK(bought.orders.empty());

	// Loaded, within the hold, from what the company has there.
	CHECK(hosted.Move(Company::kLoad, kOre, kGood, 3) == Result::InsufficientStock);
	for (const std::string_view item : {kGood, kOxygen, kCartridges})
		REQUIRE(hosted.Move(Company::kLoad, kOre, item, item == kGood ? 2 : 1) == Result::Succeeded);
	const CompanyView loaded = hosted.Seen();
	CHECK(loaded.holdings.empty());
	CHECK(Aboard(loaded.ships[0], kGood) == 2);
	CHECK(Aboard(loaded.ships[0], kOxygen) == 1);
	CHECK(Aboard(loaded.ships[0], kCartridges) == 1);

	// A bid under Brine's market rests, its escrow held, over the journey and the save.
	REQUIRE(hosted.Place(kBrine, kGood, Side::Buy, 1, 1'000'000) == Result::Succeeded);
	const CompanyView resting = hosted.Seen();
	REQUIRE(resting.orders.size() == 1);
	CHECK(resting.orders[0].order.escrow > 0);
	CHECK(resting.credits + resting.orders[0].order.escrow == loaded.credits);

	REQUIRE(
	    hosted.Run(
	        kAda, Travel::kTravel, Travel::kVersion, Travel::Travel{"Ship1", std::string{kBrine}}) ==
	    Result::Succeeded);
	CHECK(hosted.Ship1().journey->leg == Leg::Departing);
	CHECK(hosted.Ship1().dockedAt.empty());
	const std::uint64_t left = hosted.world.Clock().Step();

	// Refused in transit, each leaving the business as it was.
	const Business before{hosted.Seen()};
	CHECK(hosted.Move(Company::kLoad, kOre, kGood, 1) == Result::NotDocked);
	CHECK(hosted.Move(Company::kUnload, kBrine, kGood, 1) == Result::NotDocked);
	CHECK(
	    hosted.Run(
	        kAda, Company::kUpgrade, Company::kVersion,
	        Company::Upgrade{"Ship1", "OfferCargoHolds", 1}) == Result::NotDocked);
	CHECK(Business{hosted.Seen()} == before);

	// Ore's report froze at the step before the ship left its pad.
	CHECK_FALSE(hosted.Market(kOre).live);
	CHECK(hosted.Market(kOre).report.observed == left - 1);

	for (std::uint64_t step = 0; step < kJourneySteps && hosted.Ship1().journey->leg != Leg::Cruising;
	     ++step)
		hosted.Step();
	REQUIRE(hosted.Ship1().journey->leg == Leg::Cruising);
}

// Cruising: the tanks refilled from the hold in flight, then docked at Brine, the cartridge replaced,
// the copper unloaded and sold, the resting bid cancelled and the holds bought.
void FromCruise(Hosted &hosted)
{
	const ShipView cruising = hosted.Ship1();
	CHECK(cruising.lifeSupport.connectedTo.empty());
	REQUIRE(cruising.lifeSupport.tanks[0] < 10.0);
	CHECK(hosted.Service(Company::kRefill) == Result::Succeeded);
	const ShipView refilled = hosted.Ship1();
	// Full, less the step's use drawn after the command.
	MANIFEST_CHECK_NEAR(refilled.lifeSupport.tanks[0], 10.0, 1e-6);
	CHECK(refilled.lifeSupport.tanks[1] == 10.0);
	CHECK(refilled.lifeSupport.tanks[2] == 10.0);
	CHECK(Aboard(refilled, kOxygen) == 0);
	REQUIRE(refilled.opened.size() == 1);
	CHECK(refilled.opened[0].itemId == kOxygen);
	MANIFEST_CHECK_NEAR(
	    refilled.opened[0].quantity, 142.5 - (10.0 - cruising.lifeSupport.tanks[0]), 1e-6);

	hosted.RunJourney();
	const ShipView arrived = hosted.Ship1();
	CHECK(arrived.journey->leg == Leg::Arrived);
	CHECK(arrived.dockedAt == kBrine);
	CHECK(arrived.lifeSupport.connectedTo == kBrine);
	// The cargo stayed with the ship.
	CHECK(Aboard(arrived, kGood) == 2);
	CHECK(Aboard(arrived, kCartridges) == 1);
	// Brine is live now, and Ore as it froze.
	CHECK(hosted.Market(kBrine).live);
	CHECK(hosted.Market(kBrine).report.source == World::Source::Docked);
	CHECK_FALSE(hosted.Market(kOre).live);

	const Business before{hosted.Seen()};
	CHECK(hosted.Move(Company::kUnload, kOre, kGood, 2) == Result::WrongVenue);
	CHECK(Business{hosted.Seen()} == before);

	// A fresh cartridge from the hold's tote, the other kept opened in it.
	CHECK(hosted.Service(Company::kReplace) == Result::Succeeded);
	const ShipView replaced = hosted.Ship1();
	CHECK(replaced.lifeSupport.cartridges[0] == 30.0);
	CHECK(Aboard(replaced, kCartridges) == 0);

	// Unloaded and sold down the bids of the live report to the level its second tote takes: the
	// proceeds are the company's, the copper the broker's, and no sell rests.
	REQUIRE(hosted.Move(Company::kUnload, kBrine, kGood, 2) == Result::Succeeded);
	CHECK(Held(hosted.Seen(), kBrine, kGood).available == 2);
	const std::int64_t unloaded = hosted.Seen().credits;
	std::uint64_t reach = 0;
	std::int64_t limit = 0;
	for (const World::Level &level : hosted.Quote(kBrine, kGood).bids)
		if (reach < 2)
		{
			reach += level.totes;
			limit = level.price;
		}
	REQUIRE(reach >= 2);
	REQUIRE(hosted.Place(kBrine, kGood, Side::Sell, 2, limit) == Result::Succeeded);
	const CompanyView sold = hosted.Seen();
	CHECK(sold.credits > unloaded);
	CHECK(Held(sold, kBrine, kGood) == World::Holding{std::string{kBrine}, std::string{kGood}, 0, 0});
	REQUIRE(sold.orders.size() == 1);
	CHECK(sold.orders[0].order.side == Side::Buy);

	// The resting bid cancelled, its escrow given back.
	const std::int64_t escrow = sold.orders[0].order.escrow;
	REQUIRE(
	    hosted.Run(kAda, Trade::kCancel, Trade::kVersion, Trade::Cancel{sold.orders[0].orderId}) ==
	    Result::Succeeded);
	const CompanyView cancelled = hosted.Seen();
	CHECK(cancelled.orders.empty());
	CHECK(cancelled.credits == sold.credits + escrow);

	// The holds, bought with what the run earned, their capacity the ship's.
	const Company::Upgrade holds{"Ship1", "OfferCargoHolds", 1};
	const std::uint64_t sequence = hosted.services.requests.NextSequence(kAda);
	hosted.sunk += kHoldsCost;
	REQUIRE(hosted.Run(kAda, Company::kUpgrade, Company::kVersion, holds) == Result::Succeeded);
	const CompanyView upgraded = hosted.Seen();
	CHECK(upgraded.credits == cancelled.credits - kHoldsCost);
	CHECK(upgraded.ships[0].hold == 2560);
	CHECK(upgraded.ships[0].systems == std::vector<World::Fitting>{{"SkillShipCargoHolds", 1}});

	// Retried, it is answered as it ran; asked again, refused; neither pays or fits.
	const Receipt retried = hosted.services.requests.Admit(
	    hosted.world, kAda, sequence, std::string{Company::kUpgrade}, Company::kVersion,
	    BinaryArchive::Write(holds, FormatVersion{Company::kVersion}));
	CHECK(retried.status == Receipt::Status::Completed);
	CHECK(retried.result == Result::Succeeded);
	CHECK(hosted.Run(kAda, Company::kUpgrade, Company::kVersion, holds) == Result::AlreadyFitted);
	CHECK(hosted.Seen() == upgraded);
}

void CheckLedger(const Hosted &hosted)
{
	CHECK_MESSAGE(
	    !hosted.creditBreach, "credits unaccounted for from step ", hosted.creditBreach.value_or(0));
	CHECK_MESSAGE(
	    !hosted.goodsBreach, "goods unaccounted for from step ", hosted.goodsBreach.value_or(0));
	// Each source and sink the ledger allows for was seen at work.
	CHECK(hosted.made > 0);
	CHECK(hosted.consumptions > 0);
}
} // namespace


TEST_CASE("The trading fixture's loop, saved in flight with a bid resting and replayed, ends alike")
{
	Hosted hosted{MANIFEST_MOD_DIRECTORY};
	hosted.Bootstrap();
	const std::vector<std::byte> start = hosted.world.Save();
	const std::uint64_t first = hosted.world.Clock().Step();
	hosted.world.SetRecording(true);
	ToCruise(hosted);
	const std::vector<std::byte> inFlight = hosted.world.Save();
	const std::size_t traced = hosted.trace.size();
	FromCruise(hosted);
	CheckLedger(hosted);

	// Loaded in flight, the bid resting, it goes on to the same outcomes and world.
	Hosted loaded{MANIFEST_MOD_DIRECTORY};
	loaded.Load(inFlight);
	REQUIRE(loaded.Seen().orders.size() == 1);
	REQUIRE(loaded.Ship1().journey->leg == Leg::Cruising);
	FromCruise(loaded);
	CheckLedger(loaded);
	CHECK(
	    loaded.trace ==
	    std::vector<Outcome>(
	        hosted.trace.begin() + static_cast<std::ptrdiff_t>(traced), hosted.trace.end()));
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	// Replayed from the bootstrap, the recorded commands reach the same world, the holds bought in the
	// step they were.
	const auto bought = std::ranges::find_if(
	    hosted.trace, [](const Outcome &outcome)
	    { return outcome.command == Company::kUpgrade && outcome.result == Result::Succeeded; });
	REQUIRE(bought != hosted.trace.end());
	Hosted replayed{MANIFEST_MOD_DIRECTORY};
	replayed.Load(start);
	for (std::uint64_t step = first; step < hosted.world.Clock().Step(); ++step)
	{
		for (const Jettison::Sim::AppliedCommand &applied : hosted.world.Recorded())
			if (applied.step == step)
				replayed.world.Commands().Push(applied.command);
		if (step + 1 == bought->step)
			replayed.sunk += kHoldsCost;
		replayed.Step();
	}
	CheckLedger(replayed);
	CHECK(replayed.world.StateHash() == hosted.world.StateHash());

	// The same in every build the gate runs: Debug, Release, sanitised and the TUI off. It moves with
	// what a save holds, the fixture's content or stream, or the engine's movement or save format.
	CHECK(hosted.world.StateHash() == 0x33019a190e7efb60ULL);
}


TEST_CASE("The trading fixture's loop from the pack is the one from the directory, step for step")
{
	const auto identity = [](const std::string &content)
	{
		return Manifest::Host::Validate({content, {MANIFEST_FIXTURE_OVERLAY}, "trading"})
		    .identity.manifest;
	};
	CHECK(identity(MANIFEST_MOD_PACK) == identity(MANIFEST_MOD_DIRECTORY));

	Hosted directory{MANIFEST_MOD_DIRECTORY};
	Hosted pack{MANIFEST_MOD_PACK};
	for (Hosted *hosted : {&directory, &pack})
	{
		hosted->Bootstrap();
		ToCruise(*hosted);
		FromCruise(*hosted);
		CheckLedger(*hosted);
	}
	CHECK(pack.trace == directory.trace);
	CHECK(pack.world.Clock().Step() == directory.world.Clock().Step());
	CHECK(pack.world.StateHash() == directory.world.StateHash());
}


TEST_CASE("The trading fixture's cruise fast-forwarded is the one stepped, at every stop")
{
	Hosted forwarded{MANIFEST_MOD_DIRECTORY};
	Hosted stepped{MANIFEST_MOD_DIRECTORY};
	for (Hosted *hosted : {&forwarded, &stepped})
	{
		hosted->Bootstrap();
		ToCruise(*hosted);
	}
	REQUIRE(forwarded.world.Save() == stepped.world.Save());

	// The cruise passed over between what wakes a system (its arrival, a batch finishing, the bid's
	// lapse, a turn), the economy working and the crew breathing throughout, then docking stepped; and
	// each stop the world stepping reaches.
	std::uint64_t calls = 0;
	const auto forward = [&](std::uint64_t until)
	{
		forwarded.world.FastForward(until);
		(void)forwarded.world.Events().Take();
		++calls;
		while (stepped.world.Clock().Step() < forwarded.world.Clock().Step())
		{
			stepped.world.Step();
			(void)stepped.world.Events().Take();
		}
		REQUIRE(forwarded.world.Save() == stepped.world.Save());
	};
	const std::uint64_t end = forwarded.world.Clock().Step() + kJourneySteps;
	while (!forwarded.Ship1().journey->Ended() && forwarded.world.Clock().Step() < end)
		forward(end);
	REQUIRE(forwarded.Ship1().journey->leg == Leg::Arrived);
	const std::uint64_t steps = forwarded.world.Clock().Step() - (end - kJourneySteps);
	MESSAGE(steps << " steps to Brine in " << calls << " calls");
	CHECK(calls * 4 < steps);

	// An hour docked at Brine passed over too, its venue's report the one stepping leaves, observed
	// at the clock (L28).
	REQUIRE_FALSE(forwarded.Ship1().dockedAt.empty());
	calls = 0;
	const std::uint64_t hour = forwarded.world.Clock().Step() + std::uint64_t{60} * 60;
	while (forwarded.world.Clock().Step() < hour)
		forward(hour);
	MESSAGE("an hour docked in " << calls << " calls");
	CHECK(calls * 4 < std::uint64_t{60} * 60);
}


// Nia's ship and Ada's, the trader the overlay adds and the player.
const std::vector<std::string> kTraderShips{"Ship1", "Ship2"};

// With no client connected, a scenario's NPC trader and a player whose ship has its standing policy
// trade as the server's loop steps the world (D-097), passing over what it may (Jettison::Sim::Quiet).
TEST_CASE("An NPC and a player with no client trade alone, saved mid-job, loaded and replayed alike")
{
	const std::vector<std::filesystem::path> overlays{
	    MANIFEST_FIXTURE_OVERLAY, MANIFEST_TRADER_OVERLAY};
	Hosted hosted{MANIFEST_MOD_DIRECTORY, overlays};
	REQUIRE(
	    hosted.Run(
	        kHost, Company::kProvision, Company::kVersion,
	        Company::Provision{"Ada", "Ada", "Ada Co"}) == Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	const auto ship = [&](const Hosted &on, const std::string &id)
	{ return on.services.identities.Find(Kind::Ship, id); };
	// Both cruises on the drive's power law, as the bootstrap's, so that a trade is minutes (D-090).
	for (const std::string &id : kTraderShips)
		registry.get<Jettison::Space::CruiseDrive>(ship(hosted, id)).profile.reset();
	const std::vector<std::byte> start = hosted.world.Save();
	const std::uint64_t first = hosted.world.Clock().Step();
	hosted.world.SetRecording(true);
	REQUIRE(
	    hosted.Run(
	        kAda, Manifest::Policy::kSet, Manifest::Policy::kVersion,
	        Manifest::Policy::Set{"Ship2", true, 0, 0}) == Result::Succeeded);
	CHECK(registry.get<const World::Principal>(hosted.services.identities.Find(Kind::Principal, "Nia"))
	          .npc);

	// Each lot a job sells or keeps after selling, and its company's credits as the run ends.
	const auto balance = [&](const Hosted &on, const std::string &id)
	{
		const entt::entity company = on.services.identities.Find(
		    Kind::CompanyAccount, on.world.Registry().get<const World::Ship>(ship(on, id)).companyId);
		return on.world.Registry().get<const World::Account>(company).balance;
	};
	std::map<std::string, int> sales;
	std::map<std::string, World::Job> jobs;
	std::optional<std::vector<std::byte>> midJob;
	std::uint64_t saved = 0;
	std::uint64_t calls = 0;
	bool padLost = false;
	const std::uint64_t end = first + std::uint64_t{60} * 60 * 25;
	while (hosted.world.Clock().Step() < end)
	{
		hosted.world.FastForward(end);
		(void)hosted.world.Events().Take();
		++calls;
		for (const std::string &id : kTraderShips)
		{
			const World::Job &job = registry.get<const World::Job>(ship(hosted, id));
			const World::Job &was = jobs[id];
			sales[id] += was.totes > 0 && job.totes < was.totes && job.phase != World::Phase::Buying;
			jobs[id] = job;
			const auto *journey = registry.try_get<const World::Journey>(ship(hosted, id));
			padLost = padLost || (journey && journey->failure == World::Failure::PadLost);
		}
		// Saved with Ada's lot aboard and under way to its buyer.
		const auto *pJourney = registry.try_get<const World::Journey>(ship(hosted, "Ship2"));
		if (!midJob && jobs["Ship2"].phase == World::Phase::ToDestination && pJourney &&
		    pJourney->leg == Leg::Cruising)
		{
			midJob = hosted.world.Save();
			saved = hosted.world.Clock().Step();
		}
	}
	for (const std::string &id : kTraderShips)
	{
		CHECK_MESSAGE(sales[id] > 0, id);
		// What it holds counted at what it paid, since the run may end with a lot aboard.
		const World::Job &job = registry.get<const World::Job>(ship(hosted, id));
		CHECK_MESSAGE(balance(hosted, id) + (job.totes > 0 ? job.cost : 0) > 25000, id);
	}
	// Every arrival docked: none circled the point above its pad until the pad lapsed, as one did
	// before Jettison's docks stopped a ship out along its pad's normal first.
	CHECK_FALSE(padLost);
	REQUIRE(midJob);
	const std::uint64_t steps = end - first;
	MESSAGE(steps << " steps in " << calls << " calls");
	// The cruises passed over. A ship flown in a station's frame is steered every step, and here each
	// docked wait has the other ship so flown.
	CHECK(calls * 3 < steps * 2);

	// Loaded mid-job, it goes on to the same world.
	Hosted loaded{MANIFEST_MOD_DIRECTORY, overlays};
	loaded.world.Load(*midJob);
	REQUIRE(loaded.world.Clock().Step() == saved);
	while (loaded.world.Clock().Step() < end)
	{
		loaded.world.FastForward(end);
		(void)loaded.world.Events().Take();
	}
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	// Replayed from the start through every step, the one command recorded, it reaches the same world
	// with the ledger kept: nothing the fast-forward passed over was a decision.
	Hosted replayed{MANIFEST_MOD_DIRECTORY, overlays};
	replayed.Load(start);
	for (std::uint64_t step = first; step < end; ++step)
	{
		for (const Jettison::Sim::AppliedCommand &applied : hosted.world.Recorded())
			if (applied.step == step)
				replayed.world.Commands().Push(applied.command);
		replayed.Step();
	}
	CheckLedger(replayed);
	CHECK(replayed.world.StateHash() == hosted.world.StateHash());

	// The same in every build the gate runs; it moves as the fixture's does.
	CHECK(hosted.world.StateHash() == 0x71af5a338eb15ba8ULL);
}


// Bo, a player whose company is given a million credits for the purpose, buys every ask resting at
// the venue for the item, a turn at a time, until its broker has none left to post: a shortage made
// through the book, the broker's next quotes at an empty stock.
void Hoard(
    Hosted &hosted, const Manifest::Content::ContentSet &content, std::string_view venueId,
    std::string_view itemId)
{
	const Actor bo{ActorKind::Principal, "Bo"};
	REQUIRE(
	    hosted.Run(
	        kHost, Company::kProvision, Company::kVersion, Company::Provision{"Bo", "Bo", "Bo Co"}) ==
	    Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	const entt::entity account = hosted.services.identities.Find(
	    Kind::CompanyAccount,
	    registry.get<const World::Ship>(hosted.services.identities.Find(Kind::Ship, "Ship1"))
	        .companyId);
	registry.patch<World::Account>(account, [](World::Account &funds) { funds.balance = 1'000'000; });
	const auto turn = static_cast<std::uint64_t>(
	    std::llround(content.tuning.economy.intervalSeconds / hosted.world.FixedStep()));
	for (int round = 0; round < 4; ++round)
	{
		std::int64_t limit = 0;
		std::uint64_t totes = 0;
		for (const auto &[entity, order] : registry.view<const World::Order>().each())
			if (order.venueId == venueId && order.itemId == itemId && order.side == Side::Sell)
			{
				limit = std::max(limit, order.limit);
				totes += order.remaining;
			}
		if (totes == 0)
			break;
		REQUIRE(
		    hosted.Run(
		        bo, Trade::kPlace, Trade::kVersion,
		        Trade::Place{std::string{venueId}, std::string{itemId}, Side::Buy, totes, limit}) ==
		    Result::Succeeded);
		const std::uint64_t requoted = (hosted.world.Clock().Step() / turn + 1) * turn;
		while (hosted.world.Clock().Step() < requoted)
			hosted.Step();
	}
}


// A glut: the venue's broker given the totes, a windfall, and every broker requoted.
void Glut(
    Hosted &hosted, const Manifest::Content::ContentSet &content, std::string_view venueId,
    std::string_view itemId, std::uint64_t totes)
{
	entt::registry &registry = hosted.world.Registry();
	const entt::entity broker = hosted.services.identities.Find(Kind::Broker, std::string{venueId});
	registry.patch<World::Stock>(
	    broker,
	    [&](World::Stock &stock)
	    {
		    for (World::Holding &holding : stock.holdings)
			    if (holding.itemId == itemId)
				    holding.available += totes;
	    });
	Trade::Quote(hosted.world, hosted.services.identities, content);
}


// The totes of the item the venue's broker holds, free or in its asks.
std::uint64_t BrokerTotes(const Hosted &hosted, std::string_view venueId, std::string_view itemId)
{
	const entt::entity broker = hosted.services.identities.Find(Kind::Broker, std::string{venueId});
	for (const World::Holding &holding :
	     hosted.world.Registry().get<const World::Stock>(broker).holdings)
		if (holding.itemId == itemId)
			return holding.available + holding.reserved;
	return 0;
}


// Ada provisioned at Ore, her starting reports the markets as they stand, and her ship's standing
// policy on: the ship's ID, its cruise the drive's power law.
std::string Trader(Hosted &hosted)
{
	REQUIRE(
	    hosted.Run(
	        kHost, Company::kProvision, Company::kVersion,
	        Company::Provision{"Ada", "Ada", "Ada Co"}) == Result::Succeeded);
	entt::registry &registry = hosted.world.Registry();
	std::string shipId;
	for (const auto &[entity, identity, ship] :
	     registry.view<const World::Identity, const World::Ship>().each())
		if (registry
		        .get<const World::Company>(
		            hosted.services.identities.Find(Kind::Company, ship.companyId))
		        .principalId == "Ada")
		{
			shipId = identity.id;
			registry.get<Jettison::Space::CruiseDrive>(entity).profile.reset();
		}
	REQUIRE_FALSE(shipId.empty());
	REQUIRE(
	    hosted.Run(
	        kAda, Manifest::Policy::kSet, Manifest::Policy::kVersion,
	        Manifest::Policy::Set{shipId, true, 0, 0}) == Result::Succeeded);
	return shipId;
}


// Prices moved by stock (W19, D-102), as a trader's standing policy meets them in the trading fixture:
// unshocked, Ada's first job carries copper from Ore to Brine.
TEST_CASE(
    "A shortage pulls a cargo in and a glut sends one away, fast-forwarded to the bytes stepping "
    "leaves")
{
	const std::vector<std::filesystem::path> overlays{MANIFEST_FIXTURE_OVERLAY};
	const Manifest::Content::ContentSet content =
	    Manifest::Host::Validate({MANIFEST_MOD_DIRECTORY, overlays, "trading"});
	Hosted unshocked{MANIFEST_MOD_DIRECTORY, overlays};
	const std::string unshockedShip = Trader(unshocked);
	const auto job = [](const Hosted &hosted, const std::string &shipId) -> const World::Job &
	{
		return hosted.world.Registry().get<const World::Job>(
		    hosted.services.identities.Find(Kind::Ship, shipId));
	};
	while (job(unshocked, unshockedShip).itemId.empty())
		unshocked.Step();
	const World::Job &usual = job(unshocked, unshockedShip);
	REQUIRE(usual.itemId == kGood);
	REQUIRE(usual.sourceId == kOre);
	REQUIRE(usual.destinationId == kBrine);

	// Each shock made alike in a world fast-forwarded and one stepped; the first lot fast-forwarded
	// until it sells, the stepped world stepped as far, and the two the same to the byte.
	const auto firstLot = [&](const auto &shock, std::string_view venueId, std::string_view itemId)
	{
		Hosted forwarded{MANIFEST_MOD_DIRECTORY, overlays};
		Hosted stepped{MANIFEST_MOD_DIRECTORY, overlays};
		std::string shipId;
		for (Hosted *hosted : {&forwarded, &stepped})
		{
			shock(*hosted);
			shipId = Trader(*hosted);
		}
		REQUIRE(forwarded.world.Save() == stepped.world.Save());
		const std::uint64_t before = BrokerTotes(forwarded, venueId, itemId);
		const std::uint64_t end = forwarded.world.Clock().Step() + std::uint64_t{60} * 60 * 24;
		World::Job lot;
		while (forwarded.world.Clock().Step() < end &&
		       (lot.totes == 0 || job(forwarded, shipId).totes > 0))
		{
			forwarded.world.FastForward(end);
			(void)forwarded.world.Events().Take();
			if (job(forwarded, shipId).totes > 0)
				lot = job(forwarded, shipId);
		}
		REQUIRE(lot.totes > 0);
		REQUIRE(job(forwarded, shipId).totes == 0);
		while (stepped.world.Clock().Step() < forwarded.world.Clock().Step())
		{
			stepped.world.Step();
			(void)stepped.world.Events().Take();
		}
		CHECK(forwarded.world.Save() == stepped.world.Save());
		return std::tuple{lot, before, BrokerTotes(forwarded, venueId, itemId)};
	};

	SUBCASE("Brine's furs bought out, Ada carries Ore's furs to Brine instead, for more than copper")
	{
		const auto [lot, before, after] = firstLot(
		    [&](Hosted &hosted) { Hoard(hosted, content, kBrine, "ItemFurs"); }, kBrine, "ItemFurs");
		CHECK(before == 0);
		CHECK(lot.itemId == "ItemFurs");
		CHECK(lot.sourceId == kOre);
		CHECK(lot.destinationId == kBrine);
		CHECK(lot.estimate > usual.estimate);
		CHECK(after == lot.totes);
	}
	SUBCASE("Brine's copper at five times its target, Ada buys it there and carries it away")
	{
		const std::uint64_t target = 13;
		const auto [lot, before, after] = firstLot(
		    [&](Hosted &hosted) { Glut(hosted, content, kBrine, kGood, 5 * target); }, kBrine, kGood);
		CHECK(before > 5 * target);
		CHECK(lot.itemId == kGood);
		CHECK(lot.sourceId == kBrine);
		CHECK(lot.destinationId != kBrine);
		CHECK(after == before - lot.totes);
	}
}


// Prices settling over a long run (D-102): with Ada and Nia trading on their standing policies,
// eight days leave every broker's stock of every good, so its prices, about its target, the
// seeded stock: never below half of it, a tote spared a small target's rounding, nor past twice it.
TEST_CASE("Eight days of two traders keep every broker's stock within half and twice its target")
{
	const std::vector<std::filesystem::path> overlays{
	    MANIFEST_FIXTURE_OVERLAY, MANIFEST_TRADER_OVERLAY};
	Hosted hosted{MANIFEST_MOD_DIRECTORY, overlays};
	(void)Trader(hosted);
	entt::registry &registry = hosted.world.Registry();
	for (const std::string &id : kTraderShips)
		registry.get<Jettison::Space::CruiseDrive>(hosted.services.identities.Find(Kind::Ship, id))
		    .profile.reset();
	std::map<std::string, std::uint64_t> targets;
	for (const auto &[entity, identity, broker, stock] :
	     registry.view<const World::Identity, const World::Broker, const World::Stock>().each())
		for (const World::Holding &holding : stock.holdings)
			targets[identity.id + "/" + holding.itemId] = holding.available + holding.reserved;
	REQUIRE_FALSE(targets.empty());
	const std::uint64_t first = hosted.world.Clock().Step();
	const std::uint64_t day = std::uint64_t{60} * 60 * 24;
	for (std::uint64_t days = 1; days <= 8; ++days)
	{
		const std::uint64_t end = first + days * day;
		while (hosted.world.Clock().Step() < end)
		{
			hosted.world.FastForward(end);
			(void)hosted.world.Events().Take();
		}
		for (const auto &[entity, identity, broker, stock] :
		     registry.view<const World::Identity, const World::Broker, const World::Stock>().each())
			for (const World::Holding &holding : stock.holdings)
			{
				const std::string key = identity.id + "/" + holding.itemId;
				const std::uint64_t held = holding.available + holding.reserved;
				// A good first stocked after the start has no target to hold to.
				if (!targets.contains(key))
					continue;
				CHECK_MESSAGE(2 * (held + 1) >= targets[key], key << " day " << days << ": " << held);
				CHECK_MESSAGE(held <= 2 * targets[key], key << " day " << days << ": " << held);
			}
	}
}
