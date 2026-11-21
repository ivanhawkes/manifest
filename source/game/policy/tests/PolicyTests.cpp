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
#include <manifest/query/Query.h>
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
	explicit Hosted(const Manifest::Content::ContentSet &set = Scenario("trading"))
	    : content{set}, identities{(RegisterAllTypes(), World::RegisterWorld(world))},
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
		ProvisionTraders(world, requests, identities, content);
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

	entt::entity Find(Kind kind, const std::string &id) const
	{
		const entt::entity found = identities.Find(kind, id);
		REQUIRE_MESSAGE(found != entt::null, id);
		return found;
	}

	std::int64_t Balance(const std::string &companyId) const
	{
		return world.Registry().get<World::Account>(Find(Kind::CompanyAccount, companyId)).balance;
	}

	World::Job &Job(const std::string &shipId = "Ship1")
	{
		return world.Registry().get<World::Job>(Ship(shipId));
	}

	// The player's report of the venue's good.
	World::Line Line(const std::string &principalId, std::string_view venueId, std::string_view itemId)
	{
		const World::Reports &reports =
		    world.Registry().get<World::Reports>(Find(Kind::Principal, principalId));
		const auto report = std::ranges::find(reports.reports, venueId, &World::Report::venueId);
		REQUIRE(report != reports.reports.end());
		const auto line = std::ranges::find(report->lines, itemId, &World::Line::itemId);
		REQUIRE(line != report->lines.end());
		return *line;
	}

	// The player forgets the venue, as one that never saw it.
	void Forget(const std::string &principalId, std::string_view venueId)
	{
		std::erase_if(
		    world.Registry().get<World::Reports>(Find(Kind::Principal, principalId)).reports,
		    [venueId](const World::Report &report) { return report.venueId == venueId; });
	}

	// The company's resting orders.
	std::size_t Resting(const std::string &companyId)
	{
		std::size_t resting = 0;
		for (const auto [entity, order] : world.Registry().view<const World::Order>().each())
			resting += order.ownerKind == Kind::Company && order.ownerId == companyId ? 1 : 0;
		return resting;
	}

	double UnitsPerTote(std::string_view itemId) const
	{
		const auto item =
		    std::ranges::find(content.gameMode.items, itemId, &Manifest::Core::ItemNode::itemId);
		REQUIRE(item != content.gameMode.items.end());
		return item->unitsPerCubicMetre / static_cast<double>(Manifest::Core::kTotesPerIbc);
	}

	// What one tote sells for at the price, net of its fee.
	std::int64_t NetPerTote(std::int64_t price, std::string_view itemId) const
	{
		const std::int64_t gross = *Manifest::Trade::Cost(price, UnitsPerTote(itemId), false);
		return gross - *Manifest::Trade::Fee(gross, content.tuning.market.listingFeeBps);
	}

	// Ada's ship, docked at Ore, holding a lot of furs that cost the credits, its job selling them
	// there, with Brine unknown to her: the job's policy disabled until a test enables it.
	void Lot(std::uint64_t totes, std::int64_t cost)
	{
		REQUIRE(SetPolicy(kAda, {"Ship1", false, 0, 0}) == Result::Succeeded);
		world.Registry().get<World::Cargo>(Ship("Ship1")).lots = {{"ItemFurs", totes}};
		World::Job &job = Job();
		job.phase = World::Phase::Selling;
		job.itemId = "ItemFurs";
		job.destinationId = "VenueOreHighport";
		job.totes = totes;
		job.cost = cost;
		Forget("Ada", "VenueBrineHighport");
	}

	void StepTo(std::uint64_t step)
	{
		while (world.Clock().Step() < step)
			world.Step();
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

	CHECK(hosted.SetPolicy(kAda, {"Ship1", false, 250, 100}) == Result::Succeeded);
	CHECK(registry.get<World::Policy>(ship) == World::Policy{false, 250, 100});
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


TEST_CASE("A sale's limit nets the floor's share of any totes sold, however the book splits them")
{
	const double units = 0.5;
	for (const std::int64_t floor : {-50LL, 0LL, 1LL, 999LL, 12'345LL})
		for (const std::uint64_t totes : {1ULL, 7ULL, 64ULL, 100ULL})
		{
			const std::optional<std::int64_t> limit = SellLimit(floor, totes, units, 40);
			REQUIRE(limit);
			CHECK(*limit >= 1);
			// The worst split: a fill a tote, each at the limit.
			const std::int64_t gross = *Manifest::Trade::Cost(*limit, units, false);
			const std::int64_t perFill = gross - *Manifest::Trade::Fee(gross, 40);
			for (std::uint64_t sold = 1; sold <= totes; ++sold)
			{
				const std::uint64_t fills = std::min<std::uint64_t>(sold, Manifest::Trade::kMaxFills);
				// The rest of the totes in the last fill, at the limit.
				const std::int64_t last = [&]
				{
					const std::int64_t whole = *Manifest::Trade::Cost(
					    *limit, units * static_cast<double>(sold - fills + 1), false);
					return whole - *Manifest::Trade::Fee(whole, 40);
				}();
				const std::int64_t net = perFill * static_cast<std::int64_t>(fills - 1) + last;
				const __int128 product = static_cast<__int128>(floor) * sold;
				const std::int64_t share =
				    sold == totes
				        ? floor
				        : static_cast<std::int64_t>(
				              product / totes + (product > 0 && product % totes != 0 ? 1 : 0));
				CHECK(net >= share);
			}
		}
	CHECK_FALSE(SellLimit(100, 0, units, 40));
	CHECK_FALSE(SellLimit(100, 10, units, 10'000));
}


TEST_CASE(
    "A job plans its route from its owner's reports, a stale starting one included, and pays to go")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	const std::int64_t before = hosted.Balance("Company1");
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", true, 0, 0}) == Result::Succeeded);

	// Brine's report is the starting one, stale; the job's trade is from it, shown with its estimate.
	const Manifest::Query::Snapshot seen =
	    Manifest::Query::Observe(hosted.world, hosted.identities, kAda);
	const auto brine = std::ranges::find_if(
	    seen.markets, [](const Manifest::Query::MarketView &market)
	    { return market.report.venueId == "VenueBrineHighport"; });
	REQUIRE(brine != seen.markets.end());
	CHECK_FALSE(brine->live);
	const Manifest::Query::ShipView &ship = seen.companies.at(0).ships.at(0);
	REQUIRE(ship.policy);
	REQUIRE(ship.job);
	CHECK(ship.job->phase == World::Phase::ToSource);
	CHECK(ship.job->sourceId == "VenueBrineHighport");
	CHECK(ship.job->destinationId == "VenueOreHighport");
	CHECK(ship.job->estimate > 0);
	REQUIRE(ship.journey);
	CHECK(ship.journey->venueId == "VenueBrineHighport");
	CHECK(hosted.Balance("Company1") == before - 10);
	CHECK(ship.job->totes == 0);
}


TEST_CASE("A live ask past the target is skipped, and the job replans from its reports alone")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", false, 0, 0}) == Result::Succeeded);
	// Arrived at Ore, its source, for furs to Brine, with a target no trade there reaches.
	World::Job &job = hosted.Job();
	job.phase = World::Phase::ToSource;
	job.itemId = "ItemFurs";
	job.sourceId = "VenueOreHighport";
	job.destinationId = "VenueBrineHighport";
	const std::int64_t before = hosted.Balance("Company1");
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", true, 1'000'000'000, 0}) == Result::Succeeded);
	CHECK(hosted.Job().phase == World::Phase::Choosing);
	CHECK(hosted.Resting("Company1") == 0);
	CHECK(hosted.Balance("Company1") == before);
	hosted.world.Step();
	CHECK(hosted.Job().phase == World::Phase::Blocked);
	CHECK(hosted.Job().blockage == World::Blockage::NoTrade);
	CHECK(hosted.world.Registry().get<World::Cargo>(hosted.Ship("Ship1")).lots.empty());
}


TEST_CASE("A lot no bid clears is kept, and the allowance is shared across its sales, never multiplied")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.Provision("Bea") == Result::Succeeded);
	const World::Line furs = hosted.Line("Ada", "VenueOreHighport", "ItemFurs");
	REQUIRE(furs.bid > 0);
	// Two credits a tote above what Ore's bid nets: no bid there clears it at no allowance.
	constexpr std::uint64_t kTotes = 10;
	constexpr auto kCount = static_cast<std::int64_t>(kTotes);
	const std::int64_t cost = (hosted.NetPerTote(furs.bid, "ItemFurs") + 2) * kCount;
	hosted.Lot(kTotes, cost);

	const std::int64_t before = hosted.Balance("Company1");
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", true, 0, 0}) == Result::Succeeded);
	CHECK(hosted.Balance("Company1") == before);
	CHECK(hosted.Resting("Company1") == 0);
	CHECK(
	    hosted.world.Registry().get<World::Cargo>(hosted.Ship("Ship1")).lots ==
	    std::vector<World::Lot>{{"ItemFurs", kTotes}});
	hosted.world.Step();
	REQUIRE(hosted.Job().phase == World::Phase::Blocked);
	CHECK(hosted.Job().blockage == World::Blockage::NoTrade);

	// Bea bids for two totes at 5% more, inside the spread: they sell, at no less than their share of
	// the cost.
	REQUIRE(
	    hosted.Run(
	        kBea, Manifest::Trade::kPlace, Manifest::Trade::kVersion,
	        Manifest::Trade::Place{
	            "VenueOreHighport", "ItemFurs", World::Side::Buy, 2, furs.bid / 20 * 21}) ==
	    Result::Succeeded);
	const std::int64_t beforeSale = hosted.Balance("Company1");
	hosted.StepTo(hosted.Job().retry + 1);
	const std::int64_t share = (cost * 2 + kCount - 1) / kCount;
	CHECK(hosted.Balance("Company1") - beforeSale >= share);
	CHECK(hosted.Job().totes == kTotes - 2);
	CHECK(hosted.Job().cost == cost - share);
	CHECK(hosted.Job().allowanceSpent == 0);
	CHECK(hosted.Resting("Company1") == 0);

	// Blocked again for the rest, and still blocked from then at each retry: a bid at Brine, a market
	// Ada does not know, changes nothing.
	if (hosted.Job().phase != World::Phase::Blocked)
		hosted.world.Step();
	REQUIRE(hosted.Job().phase == World::Phase::Blocked);
	const std::uint64_t since = hosted.Job().blockedSince;
	REQUIRE(
	    hosted.Run(
	        kBea, Manifest::Trade::kPlace, Manifest::Trade::kVersion,
	        Manifest::Trade::Place{
	            "VenueBrineHighport", "ItemFurs", World::Side::Buy, 8, furs.bid * 3}) ==
	    Result::Succeeded);
	hosted.StepTo(hosted.Job().retry + 1);
	CHECK(hosted.Job().phase == World::Phase::Blocked);
	CHECK(hosted.Job().blockedSince == since);
	CHECK(hosted.Job().totes == kTotes - 2);

	// Its docked wait to the next retry passed over, to the bytes stepping leaves (L28).
	{
		const std::uint64_t retry = hosted.Job().retry + 1;
		const std::uint64_t waited = retry - hosted.world.Clock().Step();
		Hosted forwarded;
		Hosted stepped;
		forwarded.world.Load(hosted.world.Save());
		stepped.world.Load(hosted.world.Save());
		std::uint64_t calls = 0;
		for (; forwarded.world.Clock().Step() < retry; ++calls)
			forwarded.world.FastForward(retry);
		stepped.StepTo(retry);
		CHECK(forwarded.world.Save() == stepped.world.Save());
		MESSAGE(waited << " steps blocked and docked in " << calls << " calls");
		CHECK(calls * 100 < waited);
	}

	// Saved and loaded, then an allowance enough for Ore's bid, read at the next decision: the rest
	// sells, losing no more than the allowance.
	Hosted loaded;
	loaded.world.Load(hosted.world.Save());
	REQUIRE(loaded.world.StateHash() == hosted.world.StateHash());
	const std::int64_t remaining = loaded.Job().cost;
	const std::int64_t allowance = 6 * kCount;
	REQUIRE(loaded.SetPolicy(kAda, {"Ship1", true, 0, allowance}) == Result::Succeeded);
	CHECK(loaded.Job().phase == World::Phase::Blocked);
	const std::int64_t beforeRest = loaded.Balance("Company1");
	loaded.StepTo(loaded.Job().retry + 1);
	const std::int64_t net = loaded.Balance("Company1") - beforeRest;
	CHECK(net > 0);
	CHECK(net >= remaining - allowance);
	CHECK(net < remaining);
	CHECK(loaded.world.Registry().get<World::Cargo>(loaded.Ship("Ship1")).lots.empty());
	CHECK(loaded.Job().totes == 0);
	CHECK(loaded.Resting("Company1") == 0);
}


TEST_CASE("Docked at its source, a job buys at the live ask, cancels the rest, loads it and leaves")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", false, 0, 0}) == Result::Succeeded);
	World::Job &job = hosted.Job();
	job.phase = World::Phase::ToSource;
	job.itemId = "ItemFurs";
	job.sourceId = "VenueOreHighport";
	job.destinationId = "VenueBrineHighport";
	const std::int64_t before = hosted.Balance("Company1");
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", true, 0, 0}) == Result::Succeeded);

	const World::Job &bought = hosted.Job();
	CHECK(bought.phase == World::Phase::ToDestination);
	REQUIRE(bought.totes > 0);
	// The lot's cost is what the account paid for it, its docking fee at Brine a charge of the lot.
	CHECK(bought.charges == 10);
	CHECK(bought.cost == before - hosted.Balance("Company1") - 10);
	CHECK(
	    hosted.world.Registry().get<World::Cargo>(hosted.Ship("Ship1")).lots ==
	    std::vector<World::Lot>{{"ItemFurs", bought.totes}});
	CHECK(hosted.Resting("Company1") == 0);
	CHECK(
	    hosted.world.Registry().get<World::Journey>(hosted.Ship("Ship1")).venueId ==
	    "VenueBrineHighport");
}


TEST_CASE("A scenario's NPC traders start provisioned, the world's own, their jobs trading")
{
	Manifest::Content::ContentSet content = Scenario("trading");
	content.scenarios.at(content.scenario).traders = {{"Nia", "Nia", "Nia Freight"}};
	Hosted hosted{content};
	const entt::registry &registry = hosted.world.Registry();
	const entt::entity nia = hosted.Find(Kind::Principal, "Nia");
	CHECK(registry.get<World::Principal>(nia).npc);
	CHECK(registry.get<World::Policy>(hosted.Ship("Ship1")) == World::Policy{true, 0, 0});
	CHECK(hosted.Job().phase == World::Phase::Choosing);

	// A player provisioned beside it is not the world's, and was seeded reports as it was.
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	const entt::entity ada = hosted.Find(Kind::Principal, "Ada");
	CHECK_FALSE(registry.get<World::Principal>(ada).npc);
	CHECK(
	    registry.get<World::Reports>(ada).reports.size() ==
	    registry.get<World::Reports>(nia).reports.size());

	// By the first economic turn's end, its reports show a trade, and it sets off for its source.
	hosted.StepTo(3600);
	const World::Job &job = hosted.Job();
	CHECK(job.phase == World::Phase::ToSource);
	CHECK(job.estimate > 0);
	CHECK(registry.get<World::Journey>(hosted.Ship("Ship1")).venueId == job.sourceId);

	// Traders the starter cannot take, which the content's check refuses, refuse the world too.
	content.scenarios.at(content.scenario).traders = {
	    {"Nia", "Nia", "Co"},
	    {"Oto", "Oto", "Co"},
	    {"Pim", "Pim", "Co"},
	    {"Quy", "Quy", "Co"},
	    {"Rua", "Rua", "Co"}};
	CHECK_THROWS_AS(Hosted{content}, std::runtime_error);
}
