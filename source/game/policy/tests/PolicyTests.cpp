#include "TestHelpers.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/space/Cruise.h>
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
	World::Job drawn;
	drawn.oxygenKilograms = 1.0;
	hosted.world.Registry().replace<World::Job>(ship, drawn);
	Hosted lotless;
	CHECK_THROWS(lotless.world.Load(hosted.world.Save()));
	hosted.world.Registry().replace<World::Job>(ship, World::Job{});

	// A company's last prices, each of one item in name order and above 0.
	const entt::entity company = hosted.Find(Kind::Company, "Company1");
	for (const std::vector<World::PaidPrice> &prices :
	     {std::vector<World::PaidPrice>{{"ItemOxygen", 0}},
	      std::vector<World::PaidPrice>{{"ItemOxygen", 1}, {"ItemCopper", 1}}})
	{
		hosted.world.Registry().emplace_or_replace<World::Paid>(company, prices);
		Hosted unpriced;
		CHECK_THROWS(unpriced.world.Load(hosted.world.Save()));
	}
	hosted.world.Registry().replace<World::Paid>(
	    company, std::vector<World::PaidPrice>{{"ItemCopper", 1}, {"ItemOxygen", 1}});
	Hosted priced;
	priced.world.Load(hosted.world.Save());
	CHECK(priced.world.StateHash() == hosted.world.StateHash());
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

	// Brine's report is the starting one, stale; the job's trade is to it, shown with its estimate:
	// copper, which Ore makes and Brine's broker, holding two totes, bids well over Ore's ask for.
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
	// Docked at Ore, its source, it buys there and pays Brine's docking fee to go.
	CHECK(ship.job->phase == World::Phase::ToDestination);
	CHECK(ship.job->itemId == "ItemCopper");
	CHECK(ship.job->sourceId == "VenueOreHighport");
	CHECK(ship.job->destinationId == "VenueBrineHighport");
	CHECK(ship.job->estimate > 0);
	REQUIRE(ship.journey);
	CHECK(ship.journey->venueId == "VenueBrineHighport");
	CHECK(ship.job->totes > 0);
	CHECK(ship.job->charges == 100);
	CHECK(hosted.Balance("Company1") == before - ship.job->cost - 100);
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
	REQUIRE(furs.Bid() > 0);
	// 4% a tote above what Ore's bid nets: no bid there clears it at no allowance.
	constexpr std::uint64_t kTotes = 10;
	constexpr auto kCount = static_cast<std::int64_t>(kTotes);
	const std::int64_t cost = hosted.NetPerTote(furs.Bid(), "ItemFurs") * 104 / 100 * kCount;
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
	            "VenueOreHighport", "ItemFurs", World::Side::Buy, 2, furs.Bid() / 20 * 21}) ==
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
	            "VenueBrineHighport", "ItemFurs", World::Side::Buy, 8, furs.Bid() * 3}) ==
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

	// Saved and loaded, then an allowance enough to walk down Ore's bids, read at the next decision:
	// the rest sells, losing no more than the allowance.
	Hosted loaded;
	loaded.world.Load(hosted.world.Save());
	REQUIRE(loaded.world.StateHash() == hosted.world.StateHash());
	const std::int64_t remaining = loaded.Job().cost;
	const std::int64_t allowance = 120 * kCount;
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


TEST_CASE("Docked at its source, a job walks the live asks, cancels the rest, loads it and leaves")
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
	const World::Line asks = hosted.Line("Ada", "VenueOreHighport", "ItemFurs");
	REQUIRE(hosted.SetPolicy(kAda, {"Ship1", true, 0, 0}) == Result::Succeeded);
	const World::Job &bought = hosted.Job();
	CHECK(bought.phase == World::Phase::ToDestination);
	REQUIRE(bought.totes > 0);

	// Up the asks to the level its last tote takes, past the first; the company keeps that price.
	std::uint64_t taken = 0;
	std::int64_t deepest = 0;
	for (const World::Level &level : asks.asks)
		if (taken < bought.totes)
		{
			taken += level.totes;
			deepest = level.price;
		}
	CHECK(taken >= bought.totes);
	CHECK(deepest > asks.Ask());
	const World::PaidPrice *pPaid = World::Find(
	    hosted.world.Registry().get<World::Paid>(hosted.Find(Kind::Company, "Company1")), "ItemFurs");
	REQUIRE(pPaid);
	CHECK(pPaid->price == deepest);
	// The lot's cost is what the account paid for it, its docking fee at Brine a charge of the lot.
	CHECK(bought.charges == 100);
	CHECK(bought.cost == before - hosted.Balance("Company1") - 100);
	CHECK(
	    hosted.world.Registry().get<World::Cargo>(hosted.Ship("Ship1")).lots ==
	    std::vector<World::Lot>{{"ItemFurs", bought.totes}});
	CHECK(hosted.Resting("Company1") == 0);
	CHECK(
	    hosted.world.Registry().get<World::Journey>(hosted.Ship("Ship1")).venueId ==
	    "VenueBrineHighport");
}


TEST_CASE(
    "Life support drawn in transit is charged to the lot at the company's last prices, as estimated")
{
	Hosted hosted;
	REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
	hosted.Lot(4, 400);
	entt::registry &registry = hosted.world.Registry();
	const entt::entity ship = hosted.Ship("Ship1");
	const World::Ship &record = registry.get<const World::Ship>(ship);
	const std::string ore = "VenueOreHighport";
	const std::string brine = "VenueBrineHighport";
	const Manifest::Content::LifeSupportTuning &tuning = hosted.content.tuning.lifeSupport;
	const auto charges = [&]
	{ return Charges(registry, hosted.identities, hosted.content, ship, {}, brine); };
	const auto worth = [&](double kilograms, double occupantDays)
	{
		return static_cast<std::int64_t>(std::ceil(
		    (kilograms * 260'000.0 + occupantDays / tuning.cartridgeOccupantDays * 800'000'000.0) /
		    1e6));
	};

	// Estimated as Brine's docking fee alone while the company has bought no life support, and with its
	// last prices, 0.26 a kilogram of oxygen and 800 a cartridge, the cruise's occupant-days of both
	// too.
	const std::int64_t docking = charges();
	CHECK(docking == 100);
	registry.emplace<World::Paid>(
	    hosted.Find(Kind::Company, "Company1"),
	    std::vector<World::PaidPrice>{
	        {"ItemCarbonScrubberCartridge", 800'000'000}, {"ItemOxygen", 260'000}});
	const auto hull =
	    std::ranges::find(hosted.content.hulls, record.hullId, &Manifest::Core::ShipClassSpec::hullId);
	REQUIRE(hull != hosted.content.hulls.end());
	const auto at = [&](const std::string &venueId)
	{
		return registry
		    .get<const Jettison::Sim::Transform>(
		        Manifest::Travel::StationOf(registry, hosted.content, venueId))
		    .position;
	};
	const double days = hull->occupants.value_or(0) *
	                    Jettison::Space::CruiseSeconds(
	                        hosted.content.drives.at(Manifest::Content::RegistryName(record.hullId)),
	                        glm::distance(at(ore), at(brine))) /
	                    86'400.0;
	REQUIRE(days > 0.0);
	const std::int64_t estimated = worth(days * tuning.oxygenKilogramsPerOccupantDay, days);
	CHECK(estimated > 0);
	CHECK(charges() == docking + estimated);

	// Flown to Brine with its lot aboard, its policy off on the way: what the ship draws of its own
	// tanks and cartridges is the job's.
	World::Job &job = hosted.Job();
	job.phase = World::Phase::ToDestination;
	job.destinationId = brine;
	const auto total = [](const std::vector<double> &reserve)
	{ return std::accumulate(reserve.begin(), reserve.end(), 0.0); };
	const World::LifeSupport full = registry.get<const World::LifeSupport>(ship);
	REQUIRE(
	    hosted.Run(
	        kAda, Manifest::Travel::kTravel, Manifest::Travel::kVersion,
	        Manifest::Travel::Travel{"Ship1", brine}) == Result::Succeeded);
	// Passed over where it may be, as the server's loop steps it, its draws with it.
	const auto ended = [&] { return registry.get<const World::Journey>(ship).Ended(); };
	const std::uint64_t limit = hosted.world.Clock().Step() + std::uint64_t{60} * 86'400 * 8;
	while (!ended() && hosted.world.Clock().Step() < limit)
		hosted.world.FastForward(limit);
	REQUIRE(ended());
	REQUIRE(record.dockedAt == brine);
	const World::LifeSupport &left = registry.get<const World::LifeSupport>(ship);
	const double kilograms = job.oxygenKilograms;
	const double occupantDays = job.cartridgeDays;
	CHECK(kilograms == doctest::Approx(total(full.tanks) - total(left.tanks)));
	CHECK(occupantDays == doctest::Approx(total(full.cartridges) - total(left.cartridges)));
	CHECK(occupantDays > 0.0);

	// Charged as its job next decides docked, at those prices rounded up, so the floor recovers it;
	// with a target no bid meets the job keeps the lot. The flight out of Ore's reach and down to
	// Brine's pad draws more than the cruise alone the estimate counts.
	const std::int64_t before = job.charges;
	REQUIRE(
	    hosted.SetPolicy(kAda, {"Ship1", true, std::numeric_limits<std::int64_t>::max() / 2, 0}) ==
	    Result::Succeeded);
	CHECK(job.charges == before + worth(kilograms, occupantDays));
	CHECK(job.oxygenKilograms == 0.0);
	CHECK(job.cartridgeDays == 0.0);
	CHECK(job.totes == 4);
	CHECK(worth(kilograms, occupantDays) >= estimated);
}


TEST_CASE("Past the grace, a ship's cargo is sold tote by tote at the live bids for its arrears")
{
	// Ore's broker with little to bid with, for a few totes of furs and soap, and arrears that stand a
	// day.
	Manifest::Content::ContentSet content = Scenario("trading");
	content.tuning.brokers.treasury = 70'000;
	content.tuning.charges.seizureGraceDays = 1;
	const std::string ore = "VenueOreHighport";
	const std::uint64_t day = std::uint64_t{60} * 86'400;
	// Ada's ship docked at Ore with the totes of furs, ten for her job, and of soap, her account empty
	// and owing what is given since now; Bea directs Ore's operator, whose fees are 0.
	const auto owing = [&](Hosted &hosted, std::int64_t owed, std::vector<World::Lot> lots)
	{
		REQUIRE(hosted.Provision("Ada") == Result::Succeeded);
		hosted.Lot(10, 1000);
		entt::registry &registry = hosted.world.Registry();
		registry.get<World::Cargo>(hosted.Ship("Ship1")).lots = std::move(lots);
		registry.get<World::Account>(hosted.Find(Kind::CompanyAccount, "Company1")).balance = 0;
		registry
		    .get<World::Organisation>(hosted.Find(Kind::Organisation, "OrganisationAnvilPortAuthority"))
		    .principalId = "Bea";
		World::Fees &fees = registry.get<World::Fees>(hosted.Find(Kind::Market, ore));
		for (World::ClassFees &fee : fees.classes)
			fee.berthPerHour = 0;
		fees.berthPerOccupantHour = 0;
		fees.arrearsRateBps = 0;
		registry.emplace<World::Arrears>(hosted.Ship("Ship1"), owed, 0, hosted.world.Clock().Step());
	};
	const auto forward = [](Hosted &hosted, std::uint64_t until)
	{
		while (hosted.world.Clock().Step() < until)
			hosted.world.FastForward(until);
	};
	const auto lots = [](Hosted &hosted)
	{ return hosted.world.Registry().get<const World::Cargo>(hosted.Ship("Ship1")).lots; };
	// The live bids at Ore as the seizure's step begins, a tote's net at each and its totes, once Ore's
	// employer has sold its broker two days of furs.
	const auto bids = [&](Hosted &hosted, std::string_view item)
	{
		std::vector<std::pair<std::int64_t, std::uint64_t>> nets;
		for (const World::Level &level : hosted.Line("Ada", ore, item).bids)
			nets.emplace_back(hosted.NetPerTote(level.price, item), level.totes);
		return nets;
	};
	const auto owe = [](Hosted &hosted, std::int64_t owed)
	{ hosted.world.Registry().get<World::Arrears>(hosted.Ship("Ship1")).owed = owed; };

	// Not as the first day ends, less than a day after they began; as the second does, furs a tote at
	// a time until their proceeds cover what is owed, which they pay, the job's lot sharing it: three
	// totes, all from the bids' first level, for a little over two.
	const std::vector<World::Lot> held{{"ItemFurs", 10}, {"ItemSoap", 2}};
	Hosted forwarded{content};
	owing(forwarded, 1, held);
	forward(forwarded, day);
	CHECK(lots(forwarded) == held);
	CHECK(forwarded.world.Registry().all_of<World::Arrears>(forwarded.Ship("Ship1")));
	forward(forwarded, 2 * day - 1);
	const auto [furs, first] = bids(forwarded, "ItemFurs").front();
	REQUIRE(first >= 3);
	owe(forwarded, 2 * furs + 1);
	forward(forwarded, 2 * day);
	CHECK(lots(forwarded) == std::vector<World::Lot>{{"ItemFurs", 7}, {"ItemSoap", 2}});
	CHECK_FALSE(forwarded.world.Registry().all_of<World::Arrears>(forwarded.Ship("Ship1")));
	CHECK(forwarded.Balance("Company1") == 3 * furs - (2 * furs + 1));
	CHECK(forwarded.Resting("Company1") == 0);
	const World::Job &job = forwarded.Job();
	CHECK(job.totes == 7);
	CHECK(job.cost == 700);
	CHECK(job.phase == World::Phase::Selling);

	// To the same bytes as stepping the hour before the seizure and its step.
	Hosted stepped{content};
	owing(stepped, 1, held);
	forward(stepped, 2 * day - 3600);
	while (stepped.world.Clock().Step() < 2 * day - 1)
		stepped.world.Step();
	owe(stepped, 2 * furs + 1);
	stepped.world.Step();
	CHECK(stepped.world.Save() == forwarded.world.Save());

	// Owing more than the bids take: every level of the furs, then of the soap, the rest loaded back
	// and the proceeds paid, the rest still owed. The job's three totes of the furs go, and with no
	// lot it chooses again.
	Hosted unpaid{content};
	owing(unpaid, 100'000, {{"ItemFurs", 150}, {"ItemSoap", 10}});
	unpaid.Job().totes = 3;
	unpaid.Job().cost = 300;
	forward(unpaid, 2 * day - 1);
	std::int64_t proceeds = 0;
	std::map<std::string, std::uint64_t> taken;
	for (const std::string item : {"ItemFurs", "ItemSoap"})
		for (const auto &[net, totes] : bids(unpaid, item))
		{
			proceeds += net * static_cast<std::int64_t>(totes);
			taken[item] += totes;
		}
	forward(unpaid, 2 * day);
	const std::vector<World::Lot> left = lots(unpaid);
	REQUIRE(left.size() == 2);
	CHECK(left[0] == World::Lot{"ItemFurs", 150 - taken["ItemFurs"]});
	CHECK(left[1] == World::Lot{"ItemSoap", 10 - taken["ItemSoap"]});
	CHECK(taken["ItemSoap"] > 0);
	for (const auto [entity, order] : unpaid.world.Registry().view<const World::Order>().each())
		CHECK_FALSE(
		    (order.venueId == ore && order.side == World::Side::Buy &&
		     (order.itemId == "ItemFurs" || order.itemId == "ItemSoap")));
	const World::Arrears *pArrears =
	    unpaid.world.Registry().try_get<const World::Arrears>(unpaid.Ship("Ship1"));
	REQUIRE(pArrears);
	CHECK(pArrears->owed == 100'000 - proceeds);
	CHECK(unpaid.Balance("Company1") == 0);
	CHECK(unpaid.Resting("Company1") == 0);
	CHECK(unpaid.Job() == World::Job{});
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

	// By the first economic turn's end, its reports show a trade from Ore, where it is docked: it buys
	// and sets off for the destination.
	hosted.StepTo(3600);
	const World::Job &job = hosted.Job();
	CHECK(job.phase == World::Phase::ToDestination);
	CHECK(job.sourceId == "VenueOreHighport");
	CHECK(job.estimate > 0);
	CHECK(registry.get<World::Journey>(hosted.Ship("Ship1")).venueId == job.destinationId);

	// Traders the starter cannot take, which the content's check refuses, refuse the world too.
	content.scenarios.at(content.scenario).traders = {
	    {"Nia", "Nia", "Co"},
	    {"Oto", "Oto", "Co"},
	    {"Pim", "Pim", "Co"},
	    {"Quy", "Quy", "Co"},
	    {"Rua", "Rua", "Co"}};
	CHECK_THROWS_AS(Hosted{content}, std::runtime_error);
}
