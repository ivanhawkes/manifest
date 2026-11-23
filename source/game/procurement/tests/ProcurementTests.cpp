#include "TestHelpers.h"

#include <cstdint>
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
#include <manifest/trade/Economy.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/World.h>

using namespace Manifest::Procurement;
using Jettison::Sim::SimWorld;
using Manifest::World::Kind;
using Manifest::World::ProjectPhase;
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
// Seven days of one-second steps.
constexpr std::uint64_t kWorkSteps = 7ULL * 86'400;


// The governance scenario as the host starts it, its market and projects among the systems.
struct Hosted
{
	Hosted()
	    : identities{(
	          Manifest::Company::RegisterTypes(), Manifest::Trade::RegisterTypes(),
	          World::RegisterWorld(world))},
	      requests{Manifest::Command::RegisterRequests(world, identities)}
	{
		Manifest::Travel::Fleet &fleet =
		    Manifest::Travel::Register(world, requests, identities, Governance());
		Manifest::Company::RegisterCommands(world, requests, identities, fleet, Governance());
		Manifest::Trade::Register(world, requests, identities, Governance());
		Register(world, identities, Governance());
		World::CreateInitialState(world, identities, Governance());
		Manifest::Travel::OpenStations(world, fleet);
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

	// Fixture bootstrap: what delivery (W12's fourth slice) will put in the project's stock.
	void Deliver(std::vector<World::Holding> holdings)
	{
		Held().holdings = std::move(holdings);
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


TEST_CASE("A project's work is its days at the fixed step")
{
	CHECK(WorkSteps(7, 1.0) == kWorkSteps);
	CHECK(WorkSteps(7, 1.0 / 60.0) == 36'288'000);
	CHECK(WorkSteps(1, 1.0e6) == 1);
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
	REQUIRE(order.size() >= 3);
	CHECK(
	    std::vector<std::string>{order.begin(), order.begin() + 3} ==
	    std::vector<std::string>{
	        std::string{Manifest::Trade::kExpiry}, std::string{kProjects},
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
