#include "TestHelpers.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <vector>

#include <jettison/ecs/Scheduler.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/command/Requests.h>
#include <manifest/company/Company.h>
#include <manifest/procurement/Procurement.h>
#include <manifest/query/Query.h>
#include <manifest/trade/Economy.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

using namespace Manifest::Procurement;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Jettison::Sim::SimWorld;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Receipt;
using Manifest::Command::Result;
using Manifest::World::Kind;
using Manifest::World::ProjectPhase;
using Manifest::World::TenderState;
namespace World = Manifest::World;

namespace
{
const Manifest::Content::ContentSet &Governance()
{
	static const Manifest::Content::ContentSet loaded = []
	{
		Jettison::Vfs::FileSystem files;
		files.AddMount({"manifest", MANIFEST_MOD_DIRECTORY});
		Jettison::Sim::DefinitionRegistry definitions{files};
		return Manifest::Content::LoadScenario(files, definitions, "governance");
	}();
	return loaded;
}


constexpr std::string_view kEast = "RegionForgeEast";
constexpr std::string_view kForge = "VenueForgeHighport";
constexpr std::string_view kBronze = "ProjectBronzeMetallurgy";
constexpr std::uint64_t kDay = 86'400;
const Actor kAda{ActorKind::Principal, "Ada"};
const Actor kBea{ActorKind::Principal, "Bea"};
const Actor kDi{ActorKind::Principal, "Di"};
const Actor kHost{ActorKind::Administrator, "Host"};
// Seven days of one-second steps.
constexpr std::uint64_t kWorkSteps = 7ULL * 86'400;


// The governance scenario as the host starts it, its market and projects among the systems.
struct Hosted
{
	Hosted()
	    : identities{(
	          Manifest::Company::RegisterTypes(), Manifest::Trade::RegisterTypes(), RegisterTypes(),
	          World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, Governance());
		Manifest::Company::RegisterCommands(world, requests, identities, fleet, Governance());
		Manifest::Trade::Register(world, requests, identities, Governance());
		Register(world, requests, identities, Governance());
		World::CreateInitialState(world, identities, Governance());
		Manifest::Travel::OpenStations(world, fleet);
		for (const std::string name : {"Ada", "Bea", "Cy"})
			world.Registry().emplace<World::Principal>(
			    identities.Create(world.Registry(), Kind::Principal, name), name);
	}

	// Fixture bootstrap: Ada directs Forge East's council, as W13's founding will let a player.
	void Direct()
	{
		world.Registry()
		    .get<World::Organisation>(
		        identities.Find(Kind::Organisation, "OrganisationForgeEastCouncil"))
		    .principalId = "Ada";
	}

	// Fixture bootstrap: a company of the principal's with its credits, as provisioning gives one.
	void AddCompany(const std::string &principal, const std::string &company, std::int64_t credits)
	{
		entt::registry &registry = world.Registry();
		if (identities.Find(Kind::Principal, principal) == entt::null)
			registry.emplace<World::Principal>(
			    identities.Create(registry, Kind::Principal, principal), principal);
		const entt::entity entity = identities.Create(registry, Kind::Company, company);
		registry.emplace<World::Company>(entity, principal, company);
		registry.emplace<World::Stock>(entity);
		registry.emplace<World::Account>(
		    identities.Create(registry, Kind::CompanyAccount, company), credits);
	}

	// The request admitted as the actor's next, to run in the next step; its sequence.
	template <typename T>
	std::uint64_t Queue(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{kVersion});
		REQUIRE(
		    requests.Admit(world, actor, sequence, std::string{command}, kVersion, bytes).status ==
		    Receipt::Status::Queued);
		return sequence;
	}

	Result Outcome(const Actor &actor, std::uint64_t sequence) const
	{
		const auto completed = requests.Outcome(actor, sequence);
		REQUIRE(completed);
		return completed->result;
	}

	World::Tender TenderOf(const std::string &id) const
	{
		const entt::entity entity = identities.Find(Kind::Tender, id);
		REQUIRE(entity != entt::null);
		return world.Registry().get<World::Tender>(entity);
	}

	// The request admitted as the actor's next and run in the next step.
	template <typename T> Result Run(const Actor &actor, std::string_view command, const T &payload)
	{
		const std::uint64_t sequence = requests.NextSequence(actor);
		const std::vector<std::byte> bytes = BinaryArchive::Write(payload, FormatVersion{kVersion});
		REQUIRE(
		    requests.Admit(world, actor, sequence, std::string{command}, kVersion, bytes).status ==
		    Receipt::Status::Queued);
		world.Step();
		const auto completed = requests.Outcome(actor, sequence);
		REQUIRE(completed);
		return completed->result;
	}

	Result Publishes(
	    std::string_view item, std::uint64_t totes, std::int64_t budget = 1'000,
	    std::uint32_t bidDays = 2, std::uint32_t deliveryDays = 30, const Actor &actor = kAda)
	{
		return Run(
		    actor, kPublish,
		    Publish{
		        std::string{kEast}, std::string{kBronze}, std::string{item}, std::string{kForge}, totes,
		        budget, bidDays, deliveryDays});
	}

	std::int64_t Balance(Kind kind, const std::string &id) const
	{
		const entt::entity account = identities.Find(kind, id);
		REQUIRE(account != entt::null);
		return world.Registry().get<World::Account>(account).balance;
	}

	std::int64_t Public() const
	{
		return Balance(Kind::PublicAccount, std::string{kEast});
	}

	// Every tender, by ID.
	std::map<std::string, World::Tender> Tenders() const
	{
		std::map<std::string, World::Tender> tenders;
		for (const auto [entity, identity, tender] :
		     world.Registry().view<const World::Identity, const World::Tender>().each())
			tenders.emplace(identity.id, tender);
		return tenders;
	}

	void FastForwardTo(std::uint64_t step)
	{
		while (world.Clock().Step() < step)
			world.FastForward(step);
	}

	entt::entity ProjectEntity() const
	{
		const entt::entity project =
		    identities.Find(Kind::Project, World::ProjectId(kEast, "ProjectBronzeMetallurgy"));
		REQUIRE(project != entt::null);
		return project;
	}

	World::Project &Project()
	{
		return world.Registry().get<World::Project>(ProjectEntity());
	}

	World::Stock &Held()
	{
		return world.Registry().get<World::Stock>(ProjectEntity());
	}

	World::Knowledge &Knows(std::string_view region = kEast)
	{
		return world.Registry().get<World::Knowledge>(
		    identities.Find(Kind::Region, std::string{region}));
	}

	// Fixture bootstrap: the workshop put out of action or back, which nothing in the game does yet.
	void SetWorkshop(bool operational)
	{
		world.Registry()
		    .get<World::Facilities>(identities.Find(Kind::Region, std::string{kEast}))
		    .facilities.at(0)
		    .operational = operational;
	}

	// Fixture bootstrap: what deliveries would put in the project's stock.
	void Deliver(std::vector<World::Holding> holdings)
	{
		Held().holdings = std::move(holdings);
	}

	// A company of the principal's, provisioned as a player's is, its starter ship docked at Forge
	// Highport carrying the lots (fixture bootstrap, as buying and loading would).
	void AddHauler(const std::string &principal, std::vector<World::Lot> lots)
	{
		REQUIRE(
		    Run(kHost, Manifest::Company::kProvision,
		        Manifest::Company::Provision{principal, principal, principal}) == Result::Succeeded);
		const std::string shipId = std::format("Ship{}", identities.LastIssued(Kind::Ship));
		REQUIRE(Ship(shipId).dockedAt == kForge);
		Cargo(shipId).lots = std::move(lots);
	}

	World::Ship &Ship(const std::string &shipId)
	{
		return world.Registry().get<World::Ship>(identities.Find(Kind::Ship, shipId));
	}

	World::Cargo &Cargo(const std::string &shipId)
	{
		return world.Registry().get<World::Cargo>(identities.Find(Kind::Ship, shipId));
	}

	// The tender awarded to the company at the total: its bid placed and its bid close run.
	void Award(
	    const std::string &tenderId, const Actor &actor, const std::string &companyId,
	    std::int64_t total)
	{
		REQUIRE(Run(actor, kBid, Bid{tenderId, companyId, total, 0}) == Result::Succeeded);
		FastForwardTo(TenderOf(tenderId).bidClose + 1);
		REQUIRE(TenderOf(tenderId).state == TenderState::Awarded);
		REQUIRE(TenderOf(tenderId).companyId == companyId);
	}

	void Start()
	{
		Deliver({{std::string{kForge}, "ItemCopper", 9, 0}, {std::string{kForge}, "ItemTin", 1, 0}});
		world.Step();
		REQUIRE(Project().phase == ProjectPhase::Working);
	}

	SimWorld world{1.0, 0};
	World::Identities &identities;
	Manifest::Command::Requests &requests;
};
} // namespace


TEST_CASE("Whole days are their steps at the fixed step")
{
	CHECK(Steps(7, 1.0) == kWorkSteps);
	CHECK(Steps(7, 1.0 / 60.0) == 36'288'000);
	CHECK(Steps(1, 1.0e6) == 1);
}


TEST_CASE("A region's project starts tendering with nothing held, its region's knowledge seeded")
{
	Hosted hosted;
	CHECK(hosted.Project() == World::Project{std::string{kEast}, "ProjectBronzeMetallurgy"});
	CHECK(hosted.Held().holdings.empty());
	CHECK(hosted.Knows() == World::Knowledge{{"SkillSmelting"}, {}});
	CHECK(
	    hosted.world.Registry()
	        .get<World::Facilities>(hosted.identities.Find(Kind::Region, std::string{kEast}))
	        .facilities ==
	    std::vector<World::Facility>{
	        {"FacilityForgeEastWorkshop", Manifest::Content::FacilityKind::Workshop, true}});
	CHECK(hosted.world.Registry().view<const World::Project>().size() == 1);

	const Jettison::Ecs::Scheduler &scheduler = hosted.world.Scheduler();
	const std::vector<std::string> order = scheduler.Order(Jettison::Ecs::TickPhase::Simulation);
	REQUIRE(order.size() >= 4);
	CHECK(
	    std::vector<std::string>{order.begin(), order.begin() + 4} ==
	    std::vector<std::string>{
	        std::string{Manifest::Trade::kExpiry}, std::string{kTenders}, std::string{kProjects},
	        std::string{Manifest::Trade::kEconomy}});
	CHECK(scheduler.Ambiguities(Jettison::Ecs::TickPhase::Simulation).empty());
}


TEST_CASE("A project waits for every input, and an idle workshop leaves them unconsumed")
{
	Hosted hosted;
	hosted.Deliver({{std::string{kForge}, "ItemCopper", 9, 0}});
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Tendering);
	CHECK(hosted.Held().holdings.size() == 1);

	hosted.Deliver({{std::string{kForge}, "ItemCopper", 8, 0}, {std::string{kForge}, "ItemTin", 1, 0}});
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Tendering);

	hosted.Deliver({{std::string{kForge}, "ItemCopper", 9, 0}, {std::string{kForge}, "ItemTin", 1, 0}});
	hosted.SetWorkshop(false);
	hosted.world.Step();
	CHECK(hosted.Project() == World::Project{std::string{kEast}, "ProjectBronzeMetallurgy"});
	CHECK(hosted.Held().holdings.size() == 2);

	hosted.SetWorkshop(true);
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Working);
	CHECK(hosted.Project().worked == 1);
	CHECK(hosted.Held().holdings.empty());
}


TEST_CASE("Work pauses with its workshop and resumes, and its last step unlocks the recipe once")
{
	Hosted hosted;
	hosted.Start();
	for (int step = 0; step < 10; ++step)
		hosted.world.Step();
	CHECK(hosted.Project().worked == 11);

	hosted.SetWorkshop(false);
	hosted.world.Step();
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Paused);
	CHECK(hosted.Project().worked == 11);
	hosted.SetWorkshop(true);
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Working);
	CHECK(hosted.Project().worked == 12);
	CHECK(hosted.Held().holdings.empty());

	// The steps until the last, passed over where the systems allow, then the last.
	const std::uint64_t last = hosted.world.Clock().Step() + (kWorkSteps - 12) - 1;
	while (hosted.world.Clock().Step() < last)
		hosted.world.FastForward(last);
	CHECK(hosted.Project().phase == ProjectPhase::Working);
	CHECK(hosted.Project().worked == kWorkSteps - 1);
	CHECK(hosted.Knows().recipes.empty());
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Done);
	CHECK(hosted.Project().worked == kWorkSteps);
	CHECK(hosted.Knows().recipes == std::vector<std::string>{"RecipeBronze"});
	CHECK(hosted.Knows("RegionForgeWest").recipes.empty());

	// Done, nothing more is worked or unlocked, and the knowledge outlasts the workshop.
	hosted.SetWorkshop(false);
	hosted.world.Step();
	CHECK(hosted.Project().phase == ProjectPhase::Done);
	CHECK(hosted.Project().worked == kWorkSteps);
	CHECK(hosted.Knows().recipes == std::vector<std::string>{"RecipeBronze"});
}


TEST_CASE("Work passed over leaves the bytes stepping does, across its start, a pause and its end")
{
	Hosted stepped;
	Hosted passed;
	for (Hosted *pHosted : {&stepped, &passed})
		pHosted->Deliver(
		    {{std::string{kForge}, "ItemCopper", 9, 0}, {std::string{kForge}, "ItemTin", 1, 0}});
	for (int step = 0; step < 5'000; ++step)
		stepped.world.Step();
	while (passed.world.Clock().Step() < 5'000)
		passed.world.FastForward(5'000);
	CHECK(passed.world.StateHash() == stepped.world.StateHash());

	for (Hosted *pHosted : {&stepped, &passed})
		pHosted->SetWorkshop(false);
	stepped.world.Step();
	passed.world.Step();
	CHECK(passed.Project().phase == ProjectPhase::Paused);
	for (Hosted *pHosted : {&stepped, &passed})
		pHosted->SetWorkshop(true);

	// Each to 50 steps past the end, the one stepped through its last 100.
	const std::uint64_t end = stepped.world.Clock().Step() + (kWorkSteps - stepped.Project().worked);
	while (stepped.world.Clock().Step() < end - 50)
		stepped.world.FastForward(end - 50);
	for (int step = 0; step < 100; ++step)
		stepped.world.Step();
	while (passed.world.Clock().Step() < end + 50)
		passed.world.FastForward(end + 50);
	CHECK(stepped.Project().phase == ProjectPhase::Done);
	CHECK(passed.world.StateHash() == stepped.world.StateHash());
}


TEST_CASE("A save part-way through a project's work loads and carries it on alike")
{
	Hosted hosted;
	hosted.Start();
	hosted.world.Step();
	const std::vector<std::byte> save = hosted.world.Save();
	Hosted loaded;
	loaded.world.Load(save);
	CHECK(loaded.Project() == hosted.Project());
	hosted.world.Step();
	loaded.world.Step();
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
}


TEST_CASE("A save whose project holds or has worked what it may not does not load")
{
	Hosted hosted;
	const std::uint64_t before = hosted.world.StateHash();
	const std::string forge{kForge};
	const auto refused = [&](auto change)
	{
		Hosted broken;
		change(broken);
		CHECK_THROWS_AS(hosted.world.Load(broken.world.Save()), Jettison::Serialise::FormatError);
	};
	refused([&](Hosted &broken) { broken.Deliver({{forge, "ItemCopper", 10, 0}}); });
	refused([&](Hosted &broken) { broken.Deliver({{forge, "ItemOats", 1, 0}}); });
	refused(
	    [&](Hosted &broken)
	    {
		    broken.Start();
		    broken.Deliver({{forge, "ItemTin", 1, 0}});
	    });
	refused([](Hosted &broken) { broken.Project().worked = 1; });
	refused(
	    [](Hosted &broken)
	    {
		    broken.Start();
		    broken.Project().worked = kWorkSteps;
	    });
	refused(
	    [](Hosted &broken)
	    {
		    broken.Start();
		    broken.Project().phase = ProjectPhase::Done;
	    });
	refused([](Hosted &broken) { broken.Project().projectId = "ProjectNowhere"; });
	refused([](Hosted &broken) { broken.Project().regionId = "RegionNowhere"; });
	refused([](Hosted &broken)
	        { broken.world.Registry().remove<World::Stock>(broken.ProjectEntity()); });
	refused([](Hosted &broken) { broken.Knows().skills.emplace_back("SkillAnything"); });
	refused([](Hosted &broken) { broken.Knows().recipes = {"RecipeB", "RecipeA"}; });
	refused(
	    [](Hosted &broken)
	    {
		    auto &facilities =
		        broken.world.Registry()
		            .get<World::Facilities>(broken.identities.Find(Kind::Region, std::string{kEast}))
		            .facilities;
		    facilities.push_back(facilities.front());
	    });
	CHECK(hosted.world.StateHash() == before);

	// What a project may hold loads.
	Hosted partial;
	partial.Deliver({{forge, "ItemCopper", 9, 0}});
	hosted.world.Load(partial.world.Save());
	CHECK(hosted.world.StateHash() == partial.world.StateHash());
}


TEST_CASE("As the world starts the AI issuer tenders each input its project needs, priced by need")
{
	Hosted hosted;
	const std::int64_t funds = hosted.Public();
	const auto unitsPerTote = [](std::string_view itemId)
	{
		const auto &items = Governance().gameMode.items;
		const auto item = std::ranges::find(items, itemId, &Manifest::Core::ItemNode::itemId);
		REQUIRE(item != items.end());
		return item->unitsPerCubicMetre / static_cast<double>(Manifest::Core::kTotesPerIbc);
	};
	const auto budget = [&](std::string_view itemId, std::uint64_t totes)
	{
		const std::optional<std::int64_t> price =
		    Manifest::Trade::TenderPrice(hosted.world, hosted.identities, Governance(), kForge, itemId);
		REQUIRE(price);
		const std::optional<std::int64_t> priced =
		    Manifest::Trade::Budget(*price, static_cast<double>(totes) * unitsPerTote(itemId), 0.5);
		REQUIRE(priced);
		return *priced;
	};
	const std::int64_t copper = budget("ItemCopper", 9);
	const std::int64_t tin = budget("ItemTin", 1);
	hosted.world.Step();

	const std::string east{kEast};
	const std::string bronze{kBronze};
	const std::string forge{kForge};
	const std::map<std::string, World::Tender> expected{
	    {TenderId(kEast, kBronze, "ItemCopper", 1),
	     {east, bronze, "", "ItemCopper", forge, 9, copper, 0, kDay, 14, TenderState::Open, 0}},
	    {TenderId(kEast, kBronze, "ItemTin", 2),
	     {east, bronze, "", "ItemTin", forge, 1, tin, 0, kDay, 14, TenderState::Open, 0}}};
	CHECK(hosted.Tenders() == expected);
	CHECK(hosted.Public() == funds - copper - tin);
	CHECK(hosted.Balance(Kind::TenderAccount, TenderId(kEast, kBronze, "ItemCopper", 1)) == copper);
	CHECK(hosted.Balance(Kind::TenderAccount, TenderId(kEast, kBronze, "ItemTin", 2)) == tin);

	// Posted: a player with no company reads every tender's terms.
	const Manifest::Query::Snapshot seen =
	    Manifest::Query::Observe(hosted.world, hosted.identities, kBea);
	REQUIRE(seen.tenders.size() == 2);
	CHECK(seen.tenders[0].tenderId == TenderId(kEast, kBronze, "ItemCopper", 1));
	CHECK(seen.tenders[0].tender == expected.begin()->second);
}


TEST_CASE("A tender with no award closes at its bid close, its budget back, and the AI tenders again")
{
	Hosted hosted;
	const std::int64_t funds = hosted.Public();
	hosted.FastForwardTo(kDay);
	CHECK(hosted.Tenders().size() == 2);
	hosted.world.Step();

	const std::map<std::string, World::Tender> tenders = hosted.Tenders();
	REQUIRE(tenders.size() == 4);
	for (const char *pItem : {"ItemCopper", "ItemTin"})
	{
		const World::Tender &closed =
		    tenders.at(TenderId(kEast, kBronze, pItem, pItem == std::string{"ItemCopper"} ? 1 : 2));
		CHECK(closed.state == TenderState::Unawarded);
		CHECK(closed.closed == kDay);
	}
	CHECK(
	    hosted.identities.Find(Kind::TenderAccount, TenderId(kEast, kBronze, "ItemCopper", 1)) ==
	    entt::null);
	std::int64_t reserved = 0;
	for (const auto &[id, tender] : tenders)
		if (tender.state == TenderState::Open)
		{
			CHECK(tender.published == kDay);
			CHECK(tender.bidClose == 2 * kDay);
			reserved += tender.budget;
		}
	CHECK(tenders.contains(TenderId(kEast, kBronze, "ItemCopper", 3)));
	CHECK(tenders.contains(TenderId(kEast, kBronze, "ItemTin", 4)));
	// The public account has had its tax since, and holds back only the open budgets.
	CHECK(hosted.Public() >= funds - reserved);
}


TEST_CASE("The governor's director publishes and cancels, and every refusal writes nothing")
{
	Hosted hosted;
	hosted.Direct();
	const std::int64_t funds = hosted.Public();
	hosted.world.Step();
	CHECK(hosted.Tenders().empty());

	const auto unchanged = [&](Result result, auto send)
	{
		const std::map<std::string, World::Tender> before = hosted.Tenders();
		const std::int64_t balance = hosted.Public();
		CHECK(send() == result);
		CHECK(hosted.Tenders() == before);
		CHECK(hosted.Public() == balance);
	};
	const std::string east{kEast};
	const std::string bronze{kBronze};
	const std::string forge{kForge};
	const auto publish = [&](Publish payload)
	{ return [&hosted, payload] { return hosted.Run(kAda, kPublish, payload); }; };
	unchanged(
	    Result::NotAuthorised, [&] { return hosted.Publishes("ItemCopper", 1, 1'000, 2, 30, kBea); });
	unchanged(
	    Result::UnknownTarget,
	    publish({"RegionNowhere", bronze, "ItemCopper", forge, 1, 1'000, 2, 30}));
	unchanged(
	    Result::UnknownTarget, publish({east, "ProjectNowhere", "ItemCopper", forge, 1, 1'000, 2, 30}));
	unchanged(Result::UnknownTarget, [&] { return hosted.Publishes("ItemOats", 1); });
	// Forge West has no project, and its governor is the AI's.
	unchanged(
	    Result::UnknownTarget,
	    publish({"RegionForgeWest", bronze, "ItemCopper", forge, 1, 1'000, 2, 30}));
	unchanged(
	    Result::WrongVenue,
	    publish({east, bronze, "ItemCopper", "VenueCassiterDock", 1, 1'000, 2, 30}));
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 0); });
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 10); });
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 1, 1'000, 0, 30); });
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 1, 1'000, 8, 30); });
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 1, 1'000, 2, 61); });
	unchanged(Result::InvalidPrice, [&] { return hosted.Publishes("ItemCopper", 1, 0); });
	unchanged(Result::InsufficientFunds, [&] { return hosted.Publishes("ItemCopper", 1, funds + 1); });

	const std::uint64_t published = hosted.world.Clock().Step();
	CHECK(hosted.Publishes("ItemCopper", 4, 2'000, 7, 60) == Result::Succeeded);
	const std::string first = TenderId(kEast, kBronze, "ItemCopper", 1);
	CHECK(
	    hosted.Tenders().at(first) == World::Tender{
	                                      east, bronze, "", "ItemCopper", forge, 4, 2'000, published,
	                                      published + 7 * kDay, 60, TenderState::Open, 0});
	CHECK(hosted.Public() == funds - 2'000);
	// Five copper are left to call for.
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 6); });
	CHECK(hosted.Publishes("ItemCopper", 5) == Result::Succeeded);
	unchanged(Result::InvalidQuantity, [&] { return hosted.Publishes("ItemCopper", 1); });

	unchanged(Result::NotAuthorised, [&] { return hosted.Run(kBea, kCancel, Cancel{first}); });
	unchanged(
	    Result::UnknownTarget, [&] { return hosted.Run(kAda, kCancel, Cancel{"TenderNowhere"}); });
	CHECK(hosted.Run(kAda, kCancel, Cancel{first}) == Result::Succeeded);
	CHECK(hosted.Tenders().at(first).state == TenderState::Cancelled);
	CHECK(hosted.Tenders().at(first).closed == hosted.world.Clock().Step() - 1);
	CHECK(hosted.identities.Find(Kind::TenderAccount, first) == entt::null);
	CHECK(hosted.Public() == funds - 1'000);
	unchanged(Result::Closed, [&] { return hosted.Run(kAda, kCancel, Cancel{first}); });
	// The copper it called for can be called for again.
	CHECK(hosted.Publishes("ItemCopper", 4) == Result::Succeeded);
}


TEST_CASE("A cancel at its tender's bid close is refused, and the step before it is not")
{
	Hosted hosted;
	hosted.Direct();
	CHECK(hosted.Publishes("ItemCopper", 1, 1'000, 1) == Result::Succeeded);
	CHECK(hosted.Publishes("ItemCopper", 1, 1'000, 1) == Result::Succeeded);
	const std::string early = TenderId(kEast, kBronze, "ItemCopper", 1);
	const std::string late = TenderId(kEast, kBronze, "ItemCopper", 2);
	hosted.FastForwardTo(hosted.Tenders().at(early).bidClose - 1);
	CHECK(hosted.Run(kAda, kCancel, Cancel{early}) == Result::Succeeded);
	REQUIRE(hosted.world.Clock().Step() <= hosted.Tenders().at(late).bidClose);
	hosted.FastForwardTo(hosted.Tenders().at(late).bidClose);
	CHECK(hosted.Run(kAda, kCancel, Cancel{late}) == Result::Closed);
	CHECK(hosted.Tenders().at(late).state == TenderState::Unawarded);
}


TEST_CASE("A region's open tenders are bounded, and it keeps its last closed ones")
{
	Hosted hosted;
	hosted.Direct();
	for (int count = 0; count < 8; ++count)
		CHECK(hosted.Publishes("ItemCopper", 1) == Result::Succeeded);
	const std::int64_t balance = hosted.Public();
	CHECK(hosted.Publishes("ItemTin", 1) == Result::LimitReached);
	CHECK(hosted.Public() == balance);

	for (std::uint64_t count = 1; count <= 8; ++count)
		CHECK(
		    hosted.Run(kAda, kCancel, Cancel{TenderId(kEast, kBronze, "ItemCopper", count)}) ==
		    Result::Succeeded);
	for (std::uint64_t count = 9; count <= 17; ++count)
	{
		CHECK(hosted.Publishes("ItemCopper", 1) == Result::Succeeded);
		CHECK(
		    hosted.Run(kAda, kCancel, Cancel{TenderId(kEast, kBronze, "ItemCopper", count)}) ==
		    Result::Succeeded);
	}
	const std::map<std::string, World::Tender> tenders = hosted.Tenders();
	CHECK(tenders.size() == 16);
	CHECK(!tenders.contains(TenderId(kEast, kBronze, "ItemCopper", 1)));
	CHECK(tenders.contains(TenderId(kEast, kBronze, "ItemCopper", 2)));
	CHECK(tenders.contains(TenderId(kEast, kBronze, "ItemCopper", 17)));
}


TEST_CASE("On a turn's last step the AI calls a relief tender for a need its households are short of")
{
	Hosted hosted;
	hosted.world.Step();
	const std::string needId = Governance().tuning.needs.front().needId;
	World::Pantry &pantry = hosted.world.Registry().get<World::Pantry>(
	    hosted.identities.Find(Kind::Household, std::string{kEast}));
	pantry.unmet.push_back({needId, 1.0, 0.5});
	hosted.FastForwardTo(59);
	std::vector<Manifest::Trade::Relief> reliefs =
	    Manifest::Trade::Reliefs(hosted.world, hosted.identities, Governance());
	std::erase_if(
	    reliefs, [](const Manifest::Trade::Relief &relief) { return relief.regionId != kEast; });
	REQUIRE(reliefs.size() == 1);
	const Manifest::Trade::Relief relief = reliefs.front();
	hosted.world.Step();

	std::vector<World::Tender> called;
	for (const auto &[id, tender] : hosted.Tenders())
		if (!tender.needId.empty())
			called.push_back(tender);
	REQUIRE(called.size() == 1);
	CHECK(
	    called.front() == World::Tender{
	                          relief.regionId, "", needId, relief.itemId, relief.venueId, relief.totes,
	                          relief.budget, 59, 59 + kDay, 14, TenderState::Open, 0});

	// Open, it is not called again on the next turn.
	hosted.FastForwardTo(120);
	std::size_t reliefTenders = 0;
	for (const auto &[id, tender] : hosted.Tenders())
		reliefTenders += tender.needId.empty() ? 0 : 1;
	CHECK(reliefTenders == 1);
}


TEST_CASE("A save whose tender reserves other than its budget does not load")
{
	Hosted hosted;
	hosted.world.Step();
	const std::uint64_t before = hosted.world.StateHash();
	const std::string copper = TenderId(kEast, kBronze, "ItemCopper", 1);
	const auto refused = [&](auto change)
	{
		Hosted broken;
		broken.world.Step();
		change(broken);
		CHECK_THROWS_AS(hosted.world.Load(broken.world.Save()), Jettison::Serialise::FormatError);
	};
	refused(
	    [&](Hosted &broken)
	    {
		    broken.world.Registry()
		        .get<World::Account>(broken.identities.Find(Kind::TenderAccount, copper))
		        .balance -= 1;
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    broken.identities.Destroy(
		        broken.world.Registry(), broken.identities.Find(Kind::TenderAccount, copper));
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    World::Tender &tender = broken.world.Registry().get<World::Tender>(
		        broken.identities.Find(Kind::Tender, copper));
		    tender.state = TenderState::Unawarded;
		    tender.closed = 1;
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    broken.identities.Destroy(
		        broken.world.Registry(), broken.identities.Find(Kind::Tender, copper));
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    broken.world.Registry()
		        .get<World::Tender>(broken.identities.Find(Kind::Tender, copper))
		        .totes = 0;
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    broken.world.Registry()
		        .get<World::Tender>(broken.identities.Find(Kind::Tender, copper))
		        .needId = "NeedAnything";
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    broken.world.Registry()
		        .get<World::Tender>(broken.identities.Find(Kind::Tender, copper))
		        .projectId = "ProjectNowhere";
	    });
	CHECK(hosted.world.StateHash() == before);
}


TEST_CASE("Sealed bids are placed, replaced at their revision and withdrawn, each its company's alone")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddCompany("Ada", "CompanyA", 10'000);
	hosted.AddCompany("Bea", "CompanyB", 10'000);
	REQUIRE(hosted.Publishes("ItemTin", 1, 10'000, 1) == Result::Succeeded);
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 1);
	const auto bid = [&](const Actor &actor, const std::string &company, std::int64_t total,
	                     std::uint64_t revision, const std::string &tender = "")
	{ return hosted.Run(actor, kBid, Bid{tender.empty() ? tin : tender, company, total, revision}); };

	CHECK(bid(kBea, "CompanyB", 8'000, 0) == Result::Succeeded);
	CHECK(bid(kBea, "CompanyB", 7'000, 0) == Result::StaleRevision);
	CHECK(bid(kBea, "CompanyB", 7'000, 1) == Result::Succeeded);
	CHECK(bid(kBea, "CompanyB", 6'000, 1) == Result::StaleRevision);
	CHECK(bid(kBea, "CompanyA", 7'000, 0) == Result::NotAuthorised);
	CHECK(bid(kBea, "CompanyNowhere", 7'000, 0) == Result::UnknownTarget);
	CHECK(bid(kBea, "CompanyB", 7'000, 2, "TenderNowhere") == Result::UnknownTarget);
	CHECK(bid(kBea, "CompanyB", 0, 2) == Result::InvalidPrice);
	CHECK(bid(kBea, "CompanyB", 10'001, 2) == Result::InvalidPrice);
	// The governor's own company bids as any other.
	CHECK(bid(kAda, "CompanyA", 9'000, 0) == Result::Succeeded);

	const auto bidsOf = [&](const Actor &actor)
	{
		std::vector<Manifest::Query::BidView> bids;
		for (const Manifest::Query::CompanyView &company :
		     Manifest::Query::Observe(hosted.world, hosted.identities, actor).companies)
			bids.insert(bids.end(), company.bids.begin(), company.bids.end());
		return bids;
	};
	const std::vector<Manifest::Query::BidView> beas = bidsOf(kBea);
	REQUIRE(beas.size() == 1);
	CHECK(beas[0].bidId == World::BidId(tin, "CompanyB"));
	CHECK(beas[0].bid.total == 7'000);
	CHECK(beas[0].bid.revision == 2);
	const std::vector<Manifest::Query::BidView> adas = bidsOf(kAda);
	REQUIRE(adas.size() == 1);
	CHECK(adas[0].bid.companyId == "CompanyA");

	CHECK(hosted.Run(kAda, kWithdraw, Withdraw{tin, "CompanyB"}) == Result::NotAuthorised);
	CHECK(hosted.Run(kBea, kWithdraw, Withdraw{tin, "CompanyB"}) == Result::Succeeded);
	CHECK(hosted.Run(kBea, kWithdraw, Withdraw{tin, "CompanyB"}) == Result::UnknownTarget);
	CHECK(bidsOf(kBea).empty());
	CHECK(bid(kBea, "CompanyB", 7'000, 0) == Result::Succeeded);
}


TEST_CASE("At its bid close a tender goes to its lowest bid, then the earliest, its bond held")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddCompany("Bea", "CompanyB", 10'000);
	hosted.AddCompany("Cy", "CompanyC", 10'000);
	hosted.AddCompany("Ada", "CompanyA", 10'000);
	REQUIRE(hosted.Publishes("ItemTin", 1, 10'000, 1, 30) == Result::Succeeded);
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 1);
	// Cy's is admitted after Bea's, in the same step, at the same total.
	const std::uint64_t beas = hosted.Queue(kBea, kBid, Bid{tin, "CompanyB", 6'000, 0});
	const std::uint64_t cys =
	    hosted.Queue(Actor{ActorKind::Principal, "Cy"}, kBid, Bid{tin, "CompanyC", 6'000, 0});
	const std::uint64_t adas = hosted.Queue(kAda, kBid, Bid{tin, "CompanyA", 7'000, 0});
	hosted.world.Step();
	CHECK(hosted.Outcome(kBea, beas) == Result::Succeeded);
	CHECK(hosted.Outcome(Actor{ActorKind::Principal, "Cy"}, cys) == Result::Succeeded);
	CHECK(hosted.Outcome(kAda, adas) == Result::Succeeded);

	const std::uint64_t close = hosted.TenderOf(tin).bidClose;
	hosted.FastForwardTo(close);
	const std::int64_t before = hosted.Public();
	hosted.world.Step();
	const World::Tender awarded = hosted.TenderOf(tin);
	CHECK(awarded.state == TenderState::Awarded);
	CHECK(awarded.award == World::Award::Lowest);
	CHECK(awarded.companyId == "CompanyB");
	CHECK(awarded.price == 6'000);
	CHECK(awarded.bond == 600);
	CHECK(awarded.deliveryClose == close + 30 * kDay);
	CHECK(hosted.Public() == before + 4'000);
	CHECK(hosted.Balance(Kind::TenderAccount, tin) == 6'000);
	CHECK(hosted.Balance(Kind::BondAccount, tin) == 600);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyB") == 9'400);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyC") == 10'000);
	CHECK(hosted.world.Registry().view<const World::Bid>().empty());
	// Posted: the winner and its total, seen by a player who did not bid.
	const Manifest::Query::Snapshot seen =
	    Manifest::Query::Observe(hosted.world, hosted.identities, kAda);
	REQUIRE(seen.tenders.size() == 1);
	CHECK(seen.tenders[0].tender == awarded);
}


TEST_CASE("Equal totals accepted in one step and order go to the company of the lower ID")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddCompany("Bea", "CompanyB", 10'000);
	hosted.AddCompany("Ada", "CompanyA", 10'000);
	REQUIRE(hosted.Publishes("ItemTin", 1, 10'000, 1) == Result::Succeeded);
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 1);
	// As agents act, after their step's requests: Bea's first.
	CHECK(
	    hosted.requests.Act(hosted.world, kBea, kBid, kVersion, Bid{tin, "CompanyB", 5'000, 0}) ==
	    Result::Succeeded);
	CHECK(
	    hosted.requests.Act(hosted.world, kAda, kBid, kVersion, Bid{tin, "CompanyA", 5'000, 0}) ==
	    Result::Succeeded);
	hosted.FastForwardTo(hosted.TenderOf(tin).bidClose + 1);
	CHECK(hosted.TenderOf(tin).companyId == "CompanyA");
}


TEST_CASE("A winner short of its bond is passed over, and an earlier award's bond spends a later one's")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddCompany("Bea", "CompanyB", 650);
	hosted.AddCompany("Cy", "CompanyC", 10'000);
	const Actor cy{ActorKind::Principal, "Cy"};
	const std::string east{kEast};
	const std::string bronze{kBronze};
	const std::string forge{kForge};
	for (const char *pItem : {"ItemCopper", "ItemTin", "ItemCopper"})
		hosted.Queue(kAda, kPublish, Publish{east, bronze, pItem, forge, 1, 10'000, 1, 30});
	hosted.world.Step();
	const std::string first = TenderId(kEast, kBronze, "ItemCopper", 1);
	const std::string second = TenderId(kEast, kBronze, "ItemTin", 2);
	const std::string third = TenderId(kEast, kBronze, "ItemCopper", 3);
	for (const std::string &tender : {first, second, third})
		CHECK(hosted.Run(kBea, kBid, Bid{tender, "CompanyB", 6'000, 0}) == Result::Succeeded);
	CHECK(hosted.Run(cy, kBid, Bid{second, "CompanyC", 6'500, 0}) == Result::Succeeded);
	const std::uint64_t close = hosted.TenderOf(first).bidClose;
	REQUIRE(hosted.TenderOf(third).bidClose == close);
	hosted.FastForwardTo(close + 1);

	CHECK(hosted.TenderOf(first).companyId == "CompanyB");
	CHECK(hosted.TenderOf(first).award == World::Award::Lowest);
	CHECK(hosted.Balance(Kind::CompanyAccount, "CompanyB") == 50);
	const World::Tender passed = hosted.TenderOf(second);
	CHECK(passed.companyId == "CompanyC");
	CHECK(passed.award == World::Award::PastUnbonded);
	CHECK(passed.price == 6'500);
	CHECK(passed.bond == 650);
	const World::Tender unbonded = hosted.TenderOf(third);
	CHECK(unbonded.state == TenderState::Unawarded);
	CHECK(unbonded.award == World::Award::Unbonded);
	CHECK(unbonded.closed == close);
	CHECK(hosted.identities.Find(Kind::TenderAccount, third) == entt::null);
	CHECK(hosted.identities.Find(Kind::BondAccount, third) == entt::null);
}


TEST_CASE("A bid or withdrawal at its tender's bid close is refused, and a tender's bids are bounded")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddCompany("Bea", "CompanyB", 10'000);
	REQUIRE(hosted.Publishes("ItemTin", 1, 10'000, 1) == Result::Succeeded);
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 1);
	for (int count = 1; count < 32; ++count)
	{
		const std::string principal = std::format("P{}", count);
		hosted.AddCompany(principal, std::format("CompanyP{:02}", count), 10'000);
		CHECK(
		    hosted.Run(
		        Actor{ActorKind::Principal, principal}, kBid,
		        Bid{tin, std::format("CompanyP{:02}", count), 9'000, 0}) == Result::Succeeded);
	}
	CHECK(hosted.Run(kBea, kBid, Bid{tin, "CompanyB", 9'000, 0}) == Result::Succeeded);
	hosted.AddCompany("P32", "CompanyP32", 10'000);
	CHECK(
	    hosted.Run(Actor{ActorKind::Principal, "P32"}, kBid, Bid{tin, "CompanyP32", 9'000, 0}) ==
	    Result::LimitReached);
	// A replacement is no new bid.
	CHECK(hosted.Run(kBea, kBid, Bid{tin, "CompanyB", 8'000, 1}) == Result::Succeeded);

	hosted.FastForwardTo(hosted.TenderOf(tin).bidClose);
	const std::uint64_t bid = hosted.Queue(kBea, kBid, Bid{tin, "CompanyB", 7'000, 2});
	const std::uint64_t withdrawal = hosted.Queue(kBea, kWithdraw, Withdraw{tin, "CompanyB"});
	hosted.world.Step();
	CHECK(hosted.Outcome(kBea, bid) == Result::Closed);
	CHECK(hosted.Outcome(kBea, withdrawal) == Result::Closed);
	CHECK(hosted.TenderOf(tin).companyId == "CompanyB");
	CHECK(hosted.TenderOf(tin).price == 8'000);
}


TEST_CASE("The AI tenders again for what closed unawarded, and not for what its contract calls for")
{
	Hosted hosted;
	hosted.AddCompany("Bea", "CompanyB", 1'000'000);
	hosted.world.Step();
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 2);
	CHECK(hosted.Run(kBea, kBid, Bid{tin, "CompanyB", 100, 0}) == Result::Succeeded);
	hosted.FastForwardTo(kDay + 1);
	CHECK(hosted.TenderOf(tin).state == TenderState::Awarded);
	CHECK(hosted.TenderOf(TenderId(kEast, kBronze, "ItemCopper", 1)).state == TenderState::Unawarded);
	hosted.FastForwardTo(kDay + 120);
	std::map<std::string, std::size_t> open;
	for (const auto &[id, tender] : hosted.Tenders())
		if (tender.state == TenderState::Open || tender.state == TenderState::Awarded)
			++open[tender.itemId];
	CHECK(open == std::map<std::string, std::size_t>{{"ItemCopper", 1}, {"ItemTin", 1}});
}


TEST_CASE("A save whose award or bids break their rules does not load")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddCompany("Bea", "CompanyB", 10'000);
	const std::uint64_t before = hosted.world.StateHash();
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 1);
	const auto awarded = [&](Hosted &broken)
	{
		broken.Direct();
		broken.AddCompany("Bea", "CompanyB", 10'000);
		REQUIRE(broken.Publishes("ItemTin", 1, 10'000, 1) == Result::Succeeded);
		REQUIRE(broken.Run(kBea, kBid, Bid{tin, "CompanyB", 6'000, 0}) == Result::Succeeded);
		broken.FastForwardTo(broken.TenderOf(tin).bidClose + 1);
		REQUIRE(broken.TenderOf(tin).state == TenderState::Awarded);
	};
	const auto refused = [&](auto change)
	{
		Hosted broken;
		awarded(broken);
		change(broken);
		CHECK_THROWS_AS(hosted.world.Load(broken.world.Save()), Jettison::Serialise::FormatError);
	};
	const auto account = [](Hosted &broken, Kind kind, const std::string &id) -> World::Account &
	{ return broken.world.Registry().get<World::Account>(broken.identities.Find(kind, id)); };
	const auto tender = [&](Hosted &broken) -> World::Tender &
	{ return broken.world.Registry().get<World::Tender>(broken.identities.Find(Kind::Tender, tin)); };
	refused([&](Hosted &broken) { account(broken, Kind::BondAccount, tin).balance += 1; });
	refused([&](Hosted &broken) { account(broken, Kind::TenderAccount, tin).balance += 1; });
	refused([&](Hosted &broken) { tender(broken).companyId = "CompanyNowhere"; });
	refused([&](Hosted &broken) { tender(broken).price = 10'001; });
	refused([&](Hosted &broken) { tender(broken).award = World::Award::NoBids; });
	refused(
	    [&](Hosted &broken)
	    {
		    entt::registry &registry = broken.world.Registry();
		    registry.emplace<World::Bid>(
		        broken.identities.Create(registry, Kind::Bid, World::BidId(tin, "CompanyB")), tin,
		        "CompanyB", 5'000, std::uint64_t{1}, std::uint64_t{0}, std::uint64_t{0});
	    });
	refused(
	    [&](Hosted &broken)
	    {
		    entt::registry &registry = broken.world.Registry();
		    registry.emplace<World::Account>(
		        broken.identities.Create(registry, Kind::BondAccount, "TenderNowhere"), 1);
	    });
	CHECK(hosted.world.StateHash() == before);

	Hosted good;
	awarded(good);
	hosted.world.Load(good.world.Save());
	CHECK(hosted.world.StateHash() == good.world.StateHash());
}


TEST_CASE("Deliveries are paid their share of the total, an excess stays aboard, and the last fulfils")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddHauler("Di", {{"ItemCopper", 4}});
	REQUIRE(hosted.Publishes("ItemCopper", 9, 10'000, 1, 30) == Result::Succeeded);
	const std::string copper = TenderId(kEast, kBronze, "ItemCopper", 1);
	hosted.Award(copper, kDi, "Company1", 1'000);
	CHECK(hosted.Balance(Kind::BondAccount, copper) == 100);
	const std::int64_t credits = hosted.Balance(Kind::CompanyAccount, "Company1");
	const std::int64_t publicFunds = hosted.Public();
	const Deliver deliver{copper, "Ship1"};

	// 4 of 9: 444.4 credits, to the nearest 444.
	CHECK(hosted.Run(kDi, kDeliver, deliver) == Result::Succeeded);
	CHECK(hosted.TenderOf(copper).delivered == 4);
	CHECK(hosted.Balance(Kind::TenderAccount, copper) == 556);
	CHECK(hosted.Balance(Kind::CompanyAccount, "Company1") == credits + 444);
	CHECK(hosted.Cargo("Ship1").lots.empty());
	CHECK(hosted.Run(kDi, kDeliver, deliver) == Result::InsufficientStock);
	// 7 of 9: 777.8, to the nearest 778, so this one is paid 334.
	hosted.Cargo("Ship1").lots = {{"ItemCopper", 3}};
	CHECK(hosted.Run(kDi, kDeliver, deliver) == Result::Succeeded);
	CHECK(hosted.Balance(Kind::CompanyAccount, "Company1") == credits + 778);
	CHECK(hosted.TenderOf(copper).state == TenderState::Awarded);

	// Five aboard for the two still owed: three stay aboard, and the total and the bond are paid.
	hosted.Cargo("Ship1").lots = {{"ItemCopper", 5}, {"ItemTin", 1}};
	CHECK(hosted.Run(kDi, kDeliver, deliver) == Result::Succeeded);
	const World::Tender fulfilled = hosted.TenderOf(copper);
	CHECK(fulfilled.state == TenderState::Fulfilled);
	CHECK(fulfilled.delivered == 9);
	CHECK(fulfilled.closed == hosted.world.Clock().Step() - 1);
	CHECK(hosted.Balance(Kind::CompanyAccount, "Company1") == credits + 1'000 + 100);
	CHECK(hosted.Public() == publicFunds);
	CHECK(hosted.identities.Find(Kind::TenderAccount, copper) == entt::null);
	CHECK(hosted.identities.Find(Kind::BondAccount, copper) == entt::null);
	CHECK(hosted.Cargo("Ship1").lots == std::vector<World::Lot>{{"ItemCopper", 3}, {"ItemTin", 1}});
	CHECK(
	    hosted.Held().holdings ==
	    std::vector<World::Holding>{{std::string{kForge}, "ItemCopper", 9, 0}});
	CHECK(hosted.Run(kDi, kDeliver, deliver) == Result::Closed);
	CHECK(hosted.Run(kDi, kAbandon, Abandon{copper}) == Result::Closed);
	// Posted, its deliveries with it.
	const Manifest::Query::Snapshot seen =
	    Manifest::Query::Observe(hosted.world, hosted.identities, kAda);
	REQUIRE(seen.tenders.size() == 1);
	CHECK(seen.tenders[0].tender == fulfilled);
}


TEST_CASE(
    "A delivery is its contract's company's, from its ship docked at the venue, and refusals write "
    "nothing")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddHauler("Di", {{"ItemTin", 1}});
	hosted.AddHauler("Eve", {{"ItemTin", 1}});
	const Actor eve{ActorKind::Principal, "Eve"};
	REQUIRE(hosted.Publishes("ItemTin", 1, 10'000, 1, 1) == Result::Succeeded);
	const std::string tin = TenderId(kEast, kBronze, "ItemTin", 1);
	CHECK(hosted.Run(kDi, kDeliver, Deliver{tin, "Ship1"}) == Result::Closed);
	CHECK(hosted.Run(kDi, kAbandon, Abandon{tin}) == Result::Closed);
	hosted.Award(tin, kDi, "Company1", 500);
	const World::Tender awarded = hosted.TenderOf(tin);

	CHECK(hosted.Run(kDi, kDeliver, Deliver{"TenderNowhere", "Ship1"}) == Result::UnknownTarget);
	CHECK(hosted.Run(kDi, kDeliver, Deliver{tin, "ShipNowhere"}) == Result::UnknownTarget);
	CHECK(hosted.Run(kDi, kDeliver, Deliver{tin, "Ship2"}) == Result::NotAuthorised);
	CHECK(hosted.Run(eve, kDeliver, Deliver{tin, "Ship2"}) == Result::NotAuthorised);
	CHECK(hosted.Run(eve, kAbandon, Abandon{tin}) == Result::NotAuthorised);
	CHECK(hosted.Run(kDi, kAbandon, Abandon{"TenderNowhere"}) == Result::UnknownTarget);
	hosted.Ship("Ship1").dockedAt = "VenueCassiterDock";
	CHECK(hosted.Run(kDi, kDeliver, Deliver{tin, "Ship1"}) == Result::WrongVenue);
	hosted.Ship("Ship1").dockedAt.clear();
	CHECK(hosted.Run(kDi, kDeliver, Deliver{tin, "Ship1"}) == Result::NotDocked);
	hosted.Ship("Ship1").dockedAt = kForge;
	hosted.Cargo("Ship1").lots = {{"ItemCopper", 1}};
	CHECK(hosted.Run(kDi, kDeliver, Deliver{tin, "Ship1"}) == Result::InsufficientStock);
	CHECK(hosted.TenderOf(tin) == awarded);
	CHECK(hosted.Balance(Kind::TenderAccount, tin) == 500);
	CHECK(hosted.Cargo("Ship1").lots == std::vector<World::Lot>{{"ItemCopper", 1}});
	CHECK(hosted.Held().holdings.empty());
}


TEST_CASE("At its delivery close a contract short of its totes is in default, its deliveries kept")
{
	Hosted hosted;
	hosted.Direct();
	hosted.AddHauler("Di", {{"ItemCopper", 4}});
	REQUIRE(hosted.Publishes("ItemCopper", 9, 10'000, 1, 1) == Result::Succeeded);
	const std::string copper = TenderId(kEast, kBronze, "ItemCopper", 1);
	hosted.Award(copper, kDi, "Company1", 1'000);
	REQUIRE(hosted.Run(kDi, kDeliver, Deliver{copper, "Ship1"}) == Result::Succeeded);
	hosted.Cargo("Ship1").lots = {{"ItemCopper", 5}};

	const std::uint64_t close = hosted.TenderOf(copper).deliveryClose;
	hosted.FastForwardTo(close);
	const std::int64_t credits = hosted.Balance(Kind::CompanyAccount, "Company1");
	const std::int64_t publicFunds = hosted.Public();
	const std::uint64_t delivery = hosted.Queue(kDi, kDeliver, Deliver{copper, "Ship1"});
	const std::uint64_t abandon = hosted.Queue(kDi, kAbandon, Abandon{copper});
	hosted.world.Step();
	CHECK(hosted.Outcome(kDi, delivery) == Result::Closed);
	CHECK(hosted.Outcome(kDi, abandon) == Result::Closed);

	const World::Tender defaulted = hosted.TenderOf(copper);
	CHECK(defaulted.state == TenderState::Defaulted);
	CHECK(defaulted.closed == close);
	CHECK(defaulted.delivered == 4);
	// The total's 556 unpaid and the bond's share of the five missing, 55.6 rounded down, are the
	// public account's; the bond's other 45 is the supplier's again.
	CHECK(hosted.Public() == publicFunds + 556 + 55);
	CHECK(hosted.Balance(Kind::CompanyAccount, "Company1") == credits + 45);
	CHECK(hosted.identities.Find(Kind::TenderAccount, copper) == entt::null);
	CHECK(hosted.identities.Find(Kind::BondAccount, copper) == entt::null);
	CHECK(
	    hosted.Held().holdings ==
	    std::vector<World::Holding>{{std::string{kForge}, "ItemCopper", 4, 0}});
	CHECK(hosted.Cargo("Ship1").lots == std::vector<World::Lot>{{"ItemCopper", 5}});
	// What its default leaves the project needing may be tendered again.
	CHECK(hosted.Publishes("ItemCopper", 6) == Result::InvalidQuantity);
	CHECK(hosted.Publishes("ItemCopper", 5) == Result::Succeeded);
}


TEST_CASE("A contract passed over to its delivery close leaves the bytes stepping does")
{
	Hosted stepped;
	Hosted passed;
	const std::string copper = TenderId(kEast, kBronze, "ItemCopper", 1);
	// A player's tender, published in the world's second step, so its closes fall on no turn's end.
	for (Hosted *pHosted : {&stepped, &passed})
	{
		pHosted->Direct();
		pHosted->AddHauler("Di", {{"ItemCopper", 4}});
		REQUIRE(pHosted->Publishes("ItemCopper", 9, 10'000, 1, 1) == Result::Succeeded);
		pHosted->Award(copper, kDi, "Company1", 1'000);
		REQUIRE(pHosted->Run(kDi, kDeliver, Deliver{copper, "Ship1"}) == Result::Succeeded);
	}
	const std::uint64_t close = stepped.TenderOf(copper).deliveryClose;
	stepped.FastForwardTo(close - 50);
	for (int step = 0; step < 100; ++step)
		stepped.world.Step();
	passed.FastForwardTo(close + 50);
	CHECK(stepped.TenderOf(copper).state == TenderState::Defaulted);
	CHECK(passed.TenderOf(copper).closed == close);
	CHECK(passed.world.StateHash() == stepped.world.StateHash());
}


TEST_CASE(
    "An abandoned contract forfeits its bond's undelivered share, and the AI tenders again at once")
{
	Hosted hosted;
	hosted.AddHauler("Di", {});
	const std::string copper = TenderId(kEast, kBronze, "ItemCopper", 1);
	REQUIRE(hosted.TenderOf(copper).totes == 9);
	hosted.Award(copper, kDi, "Company1", 1'000);
	const std::int64_t credits = hosted.Balance(Kind::CompanyAccount, "Company1");
	const std::int64_t publicFunds = hosted.Public();
	CHECK(hosted.Run(kBea, kAbandon, Abandon{copper}) == Result::NotAuthorised);
	CHECK(hosted.Run(kDi, kAbandon, Abandon{copper}) == Result::Succeeded);
	const std::uint64_t now = hosted.world.Clock().Step() - 1;

	const World::Tender abandoned = hosted.TenderOf(copper);
	CHECK(abandoned.state == TenderState::Defaulted);
	CHECK(abandoned.closed == now);
	CHECK(hosted.Balance(Kind::CompanyAccount, "Company1") == credits);
	std::vector<World::Tender> again;
	for (const auto &[id, tender] : hosted.Tenders())
		if (tender.itemId == "ItemCopper" && tender.state == TenderState::Open)
			again.push_back(tender);
	REQUIRE(again.size() == 1);
	CHECK(again[0].published == now);
	CHECK(again[0].totes == 9);
	// The whole total and the whole bond back, the new tender's budget reserved.
	CHECK(hosted.Public() == publicFunds + 1'000 + 100 - again[0].budget);
	CHECK(hosted.Run(kDi, kAbandon, Abandon{copper}) == Result::Closed);
}


TEST_CASE("A relief tender's goods are delivered into its region's households' stock")
{
	Hosted hosted;
	hosted.AddHauler("Di", {});
	const std::string needId = Governance().tuning.needs.front().needId;
	hosted.world.Registry()
	    .get<World::Pantry>(hosted.identities.Find(Kind::Household, std::string{kEast}))
	    .unmet.push_back({needId, 1.0, 0.5});
	hosted.FastForwardTo(60);
	std::string reliefId;
	for (const auto &[id, tender] : hosted.Tenders())
		if (tender.needId == needId && tender.regionId == kEast)
			reliefId = id;
	REQUIRE(!reliefId.empty());
	const World::Tender relief = hosted.TenderOf(reliefId);
	hosted.Award(reliefId, kDi, "Company1", 1);
	hosted.Ship("Ship1").dockedAt = relief.venueId;
	hosted.Cargo("Ship1").lots = {{relief.itemId, relief.totes}};

	CHECK(hosted.Run(kDi, kDeliver, Deliver{reliefId, "Ship1"}) == Result::Succeeded);
	CHECK(hosted.TenderOf(reliefId).state == TenderState::Fulfilled);
	const World::Stock &households = hosted.world.Registry().get<World::Stock>(
	    hosted.identities.Find(Kind::Household, std::string{kEast}));
	const auto held = std::ranges::find_if(
	    households.holdings, [&](const World::Holding &holding)
	    { return holding.venueId == relief.venueId && holding.itemId == relief.itemId; });
	REQUIRE(held != households.holdings.end());
	CHECK(held->available >= relief.totes);
	CHECK(hosted.Held().holdings.empty());
}


TEST_CASE(
    "A save part-way through a contract loads, and one whose deliveries break their rules does not")
{
	const std::string copper = TenderId(kEast, kBronze, "ItemCopper", 1);
	const auto delivering = [&](Hosted &hosted)
	{
		hosted.Direct();
		hosted.AddHauler("Di", {{"ItemCopper", 4}});
		REQUIRE(hosted.Publishes("ItemCopper", 9, 10'000, 1, 30) == Result::Succeeded);
		hosted.Award(copper, kDi, "Company1", 1'000);
		REQUIRE(hosted.Run(kDi, kDeliver, Deliver{copper, "Ship1"}) == Result::Succeeded);
	};
	Hosted hosted;
	delivering(hosted);
	Hosted loaded;
	loaded.world.Load(hosted.world.Save());
	CHECK(loaded.TenderOf(copper) == hosted.TenderOf(copper));
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	const std::uint64_t before = hosted.world.StateHash();
	const auto refused = [&](auto change)
	{
		Hosted broken;
		delivering(broken);
		change(broken);
		CHECK_THROWS_AS(hosted.world.Load(broken.world.Save()), Jettison::Serialise::FormatError);
	};
	const auto tender = [&](Hosted &broken) -> World::Tender &
	{
		return broken.world.Registry().get<World::Tender>(broken.identities.Find(Kind::Tender, copper));
	};
	// Its account holds the total less the 444 paid.
	refused(
	    [&](Hosted &broken)
	    {
		    broken.world.Registry()
		        .get<World::Account>(broken.identities.Find(Kind::TenderAccount, copper))
		        .balance += 444;
	    });
	refused([&](Hosted &broken) { tender(broken).delivered = 10; });
	refused([&](Hosted &broken) { tender(broken).state = TenderState::Fulfilled; });
	refused(
	    [&](Hosted &broken)
	    {
		    tender(broken).state = TenderState::Defaulted;
		    tender(broken).closed = broken.world.Clock().Step();
	    });
	CHECK(hosted.world.StateHash() == before);
}


TEST_CASE("A governor's queue runs its projects in order, and a cancel keeps what was delivered")
{
	constexpr std::string_view kWest = "RegionForgeWest";
	constexpr std::string_view kGuild = "ProjectSmeltersGuild";
	Hosted hosted;
	entt::registry &registry = hosted.world.Registry();
	// Fixture bootstrap: Ada directs Forge West's council, as founding lets a player, and its
	// workshop stands.
	registry
	    .get<World::Organisation>(
	        hosted.identities.Find(Kind::Organisation, "OrganisationForgeWestCouncil"))
	    .principalId = "Ada";
	registry.get<World::Facilities>(hosted.identities.Find(Kind::Region, std::string{kWest}))
	    .facilities = {{"FacilityForgeWestWorkshop", Manifest::Content::FacilityKind::Workshop, true}};
	const auto queue = [&](const Actor &actor, std::string_view region, std::string_view project)
	{ return hosted.Run(actor, kQueue, Queue{std::string{region}, std::string{project}}); };
	const auto cancel = [&](std::string_view project)
	{
		return hosted.Run(
		    kAda, kCancelProject, CancelProject{std::string{kWest}, std::string{project}});
	};
	const auto project = [&](std::string_view id) -> World::Project &
	{
		const entt::entity entity = hosted.identities.Find(Kind::Project, World::ProjectId(kWest, id));
		REQUIRE(entity != entt::null);
		return registry.get<World::Project>(entity);
	};
	const auto treasury = [&] { return hosted.Balance(Kind::PublicAccount, std::string{kWest}); };

	CHECK(queue(kBea, kWest, kGuild) == Result::NotAuthorised);
	CHECK(queue(kAda, kEast, kGuild) == Result::NotAuthorised);
	CHECK(queue(kAda, kWest, "ProjectNone") == Result::UnknownTarget);
	CHECK(queue(kAda, "RegionNowhere", kGuild) == Result::UnknownTarget);
	CHECK(hosted.identities.Find(Kind::Project, World::ProjectId(kWest, kGuild)) == entt::null);

	// The first queued is the head at once; the next waits behind it.
	CHECK(queue(kAda, kWest, kGuild) == Result::Succeeded);
	CHECK(queue(kAda, kWest, kBronze) == Result::Succeeded);
	CHECK(queue(kAda, kWest, kGuild) == Result::AlreadyQueued);
	CHECK(project(kGuild).phase == ProjectPhase::Tendering);
	CHECK(project(kBronze).phase == ProjectPhase::Queued);
	CHECK(project(kBronze).queued > project(kGuild).queued);
	const auto publish = [&](std::string_view projectId, std::string_view item, std::uint64_t totes)
	{
		return hosted.Run(
		    kAda, kPublish,
		    Publish{
		        std::string{kWest}, std::string{projectId}, std::string{item}, std::string{kForge},
		        totes, 1'000, 2, 30});
	};
	CHECK(publish(kBronze, "ItemCopper", 9) == Result::InvalidQuantity);
	const std::int64_t funds = treasury();
	CHECK(publish(kGuild, "ItemCopper", 3) == Result::Succeeded);
	CHECK(treasury() == funds - 1'000);

	// Cancelled: its open tender closed with its budget back, and Bronze the head.
	CHECK(cancel(kGuild) == Result::Succeeded);
	CHECK(treasury() == funds);
	for (const auto &[id, tender] : hosted.Tenders())
		if (tender.regionId == kWest)
			CHECK(tender.state == TenderState::Cancelled);
	CHECK(project(kGuild).phase == ProjectPhase::Cancelled);
	CHECK(project(kBronze).phase == ProjectPhase::Tendering);
	CHECK(cancel(kGuild) == Result::Closed);
	CHECK(cancel("ProjectNone") == Result::UnknownTarget);

	// What an awarded contract delivered to it is kept, saved, and taken up again behind Bronze.
	registry.get<World::Stock>(hosted.identities.Find(Kind::Project, World::ProjectId(kWest, kGuild)))
	    .holdings = {{std::string{kForge}, "ItemCopper", 3, 0}};
	CHECK(queue(kAda, kWest, kGuild) == Result::Succeeded);
	CHECK(project(kGuild).phase == ProjectPhase::Queued);
	Hosted loaded;
	loaded.world.Load(hosted.world.Save());
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());

	// Bronze cannot start where smelting is not known; taken out, the guild starts with its copper,
	// teaches smelting, and Bronze, queued again, is the head.
	CHECK(cancel(kBronze) == Result::Succeeded);
	CHECK(queue(kAda, kWest, kBronze) == Result::Succeeded);
	CHECK(project(kGuild).phase == ProjectPhase::Working);
	CHECK(project(kBronze).phase == ProjectPhase::Queued);
	CHECK(cancel(kGuild) == Result::Closed);
	hosted.FastForwardTo(hosted.world.Clock().Step() + 3 * kDay);
	CHECK(project(kGuild).phase == ProjectPhase::Done);
	CHECK(hosted.Knows(kWest).skills == std::vector<std::string>{"SkillSmelting"});
	CHECK(project(kBronze).phase == ProjectPhase::Tendering);
	CHECK(queue(kAda, kWest, kGuild) == Result::AlreadyQueued);
}
