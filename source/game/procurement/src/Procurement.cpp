#include <manifest/procurement/Procurement.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <jettison/profile/Profile.h>

#include <manifest/trade/Economy.h>
#include <manifest/trade/Trade.h>

namespace Manifest::Procurement
{
namespace
{
using Jettison::Sim::SimWorld;
using World::Kind;
using World::ProjectPhase;

constexpr double kSecondsPerDay = 86'400.0;

// A project as its definition gives it, by project ID.
struct Plan
{
	World::ProjectNeeds needs;
	std::vector<std::string> skills;
	std::string recipeId;
};

struct Rules
{
	const World::Identities *pIdentities{nullptr};
	std::map<std::string, Plan, std::less<>> plans;
};


bool Operational(const World::Facilities &facilities)
{
	// shortcut: every project is metallurgy's, worked at a workshop (Content.cpp's CheckProjects);
	// a project names its facility when a second needs another.
	return std::ranges::any_of(
	    facilities.facilities, [](const World::Facility &facility)
	    { return facility.kind == Content::FacilityKind::Workshop && facility.operational; });
}


bool Startable(const Plan &plan, const World::Stock &stock, const World::Knowledge &knowledge)
{
	for (const std::string &skill : plan.skills)
		if (!std::ranges::binary_search(knowledge.skills, skill))
			return false;
	for (const auto &[itemId, totes] : plan.needs.inputs)
	{
		std::uint64_t held = 0;
		for (const World::Holding &holding : stock.holdings)
			if (holding.itemId == itemId)
				held += holding.available;
		if (held < totes)
			return false;
	}
	return true;
}


// What a project's region is, for its rules.
struct Region
{
	entt::entity entity{entt::null};
	bool operational{false};
};


Region RegionOf(const entt::registry &registry, const Rules &rules, const World::Project &project)
{
	const entt::entity region = rules.pIdentities->Find(Kind::Region, project.regionId);
	return {region, Operational(registry.get<const World::Facilities>(region))};
}


void Work(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Procurement.Work");
	entt::registry &registry = world.Registry();
	for (const auto [entity, project, stock] : registry.view<World::Project, World::Stock>().each())
	{
		if (project.phase == ProjectPhase::Done)
			continue;
		const Plan &plan = rules.plans.at(project.projectId);
		const Region region = RegionOf(registry, rules, project);
		if (project.phase == ProjectPhase::Tendering)
		{
			if (!region.operational ||
			    !Startable(plan, stock, registry.get<const World::Knowledge>(region.entity)))
				continue;
			// A project holds no more than its inputs (Identities::Load, and delivery owes no more).
			stock.holdings.clear();
		}
		if (!region.operational)
		{
			project.phase = ProjectPhase::Paused;
			continue;
		}
		project.phase = ProjectPhase::Working;
		if (++project.worked < plan.needs.workSteps)
			continue;
		project.phase = ProjectPhase::Done;
		std::vector<std::string> &recipes = registry.get<World::Knowledge>(region.entity).recipes;
		if (const auto at = std::ranges::lower_bound(recipes, plan.recipeId);
		    at == recipes.end() || *at != plan.recipeId)
			recipes.insert(at, plan.recipeId);
	}
}


// The first step the projects have to run in (Jettison::Sim::Quiet): now for one that starts, pauses
// or resumes, the last of a Working one's work, and none while the rest wait.
std::uint64_t Next(const SimWorld &world, const Rules &rules)
{
	const entt::registry &registry = world.Registry();
	const std::uint64_t now = world.Clock().Step();
	std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
	for (const auto [entity, project, stock] :
	     registry.view<const World::Project, const World::Stock>().each())
	{
		const Region region = RegionOf(registry, rules, project);
		switch (project.phase)
		{
			case ProjectPhase::Tendering:
				if (region.operational && Startable(
				                              rules.plans.at(project.projectId), stock,
				                              registry.get<const World::Knowledge>(region.entity)))
					return now;
				break;
			case ProjectPhase::Paused:
				if (region.operational)
					return now;
				break;
			case ProjectPhase::Working:
				if (!region.operational)
					return now;
				// Run at now, the step's work is worked + 1; the last is workSteps.
				next = std::min(
				    next,
				    now + (rules.plans.at(project.projectId).needs.workSteps - project.worked - 1));
				break;
			case ProjectPhase::Done: break;
		}
	}
	return next;
}


// The work of the steps passed over, one a step, each Working project's, none of them its last.
void Skip(SimWorld &world, std::uint64_t passed)
{
	for (const auto [entity, project] : world.Registry().view<World::Project>().each())
		if (project.phase == ProjectPhase::Working)
			project.worked += passed;
}
} // namespace


std::uint64_t WorkSteps(std::uint32_t workDays, double fixedStep)
{
	return std::max<std::uint64_t>(
	    1, static_cast<std::uint64_t>(std::llround(workDays * kSecondsPerDay / fixedStep)));
}


void Register(SimWorld &world, World::Identities &identities, const Content::ContentSet &content)
{
	auto rules = std::make_shared<Rules>();
	rules->pIdentities = &identities;
	std::map<std::string, World::ProjectNeeds, std::less<>> needs;
	for (const Content::ProjectDefinition &project : content.tuning.projects)
	{
		Plan plan;
		for (const Content::ItemTotes &input : project.inputs)
			plan.needs.inputs.emplace(input.itemId, input.totes);
		plan.needs.workSteps = WorkSteps(project.workDays, world.FixedStep());
		for (const Content::SkillSelection &skill : project.requiredSkills)
			plan.skills.push_back(skill.skillId);
		plan.recipeId = project.unlocksRecipe;
		needs.emplace(project.projectId, plan.needs);
		rules->plans.emplace(project.projectId, std::move(plan));
	}
	identities.SetProjects(std::move(needs));

	using Jettison::Ecs::Access;
	world.RegisterSystem(
	    {.name = std::string{kProjects},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = Access::Of<World::Facilities, World::Identity>()
	                   .Writing<World::Project, World::Stock, World::Knowledge>(),
	     .after = {std::string{Trade::kExpiry}},
	     .run = [rules, &world](entt::registry &, double) { Work(world, *rules); },
	     .before = {std::string{Trade::kEconomy}}},
	    {.next = [rules](const SimWorld &target) { return Next(target, *rules); },
	     .skip = [](SimWorld &target, std::uint64_t passed, double) { Skip(target, passed); }});
}
} // namespace Manifest::Procurement
