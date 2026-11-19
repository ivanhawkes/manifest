#include <manifest/content/Content.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/content/Definitions.h>
#include <manifest/effects/EffectCatalogue.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/sim/Simulation.h>

namespace Manifest::Content
{
namespace
{
template <typename T> std::vector<const T *> All(const Jettison::Sim::DefinitionRegistry &registry)
{
	std::vector<const T *> definitions;
	for (const std::string &name : registry.Names<T>())
		definitions.push_back(registry.Find<T>(name));
	return definitions;
}


bool Positive(double value)
{
	return std::isfinite(value) && value > 0.0;
}


// Holds the loaded set to its rules, failing on the first one broken with the record's path and
// the mounts behind it.
class Checker
{
  public:
	explicit Checker(const Jettison::Vfs::FileSystem &files) : m_files{files}
	{
	}

	// Names the record a failure is reported against.
	void At(
	    std::string_view directory, std::string_view name, std::string_view type, std::string_view id)
	{
		m_directory = directory;
		m_name = name;
		m_type = type;
		m_id = id;
	}

	void Require(bool holds, std::string_view field, std::string_view problem) const
	{
		if (holds)
			return;
		const std::string path = std::format("{}/{}", m_directory, m_name);
		// What the registry read: the topmost whole file, then the patches at or above its mount,
		// in mount order. A layer under the whole file was replaced, and gave nothing.
		const std::vector<Jettison::Vfs::Layer> wholes = m_files.ReadLayers(path + ".yaml");
		const std::size_t top = wholes.empty() ? 0 : wholes.back().mount;
		std::string layers = wholes.empty() ? "" : m_files.Mounts()[top].name;
		for (const Jettison::Vfs::Layer &patch : m_files.ReadLayers(path + ".patch.yaml"))
			if (patch.mount >= top)
				layers += std::format(", {} (patch)", m_files.Mounts()[patch.mount].name);
		throw ContentError{std::format(
		    "{}.yaml: {} '{}': {} {} (from {})", path, m_type, m_id, field, problem, layers)};
	}

  private:
	const Jettison::Vfs::FileSystem &m_files;
	std::string m_directory;
	std::string m_name;
	std::string m_type;
	std::string m_id;
};


const DefinitionType &TypeOf(const entt::meta_type &type)
{
	for (const DefinitionType &definition : DefinitionTypes())
		if (definition.type == type)
			return definition;
	throw std::logic_error{"Content: a type is not a definition type"};
}


template <typename T> void At(Checker &checker, std::string_view name, std::string_view id)
{
	const DefinitionType &type = TypeOf(entt::resolve<T>());
	checker.At(type.directory, name, type.migrations.Format(), id);
}


template <typename T, typename Id>
std::set<std::string, std::less<>> Ids(const Jettison::Sim::DefinitionRegistry &registry, Id id)
{
	std::set<std::string, std::less<>> ids;
	for (const T *pDefinition : All<T>(registry))
		ids.insert(pDefinition->*id);
	return ids;
}


void CheckAges(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	for (const std::string &name : registry.Names<Core::AgeNode>())
	{
		const Core::AgeNode &age = *registry.Find<Core::AgeNode>(name);
		At<Core::AgeNode>(checker, name, age.ageId);
		checker.Require(!age.ageId.empty(), "ageId", "is required");
		checker.Require(!age.displayName.empty(), "displayName", "is required");
	}
}


void CheckCategories(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	for (const std::string &name : registry.Names<Core::CategoryNode>())
	{
		const Core::CategoryNode &category = *registry.Find<Core::CategoryNode>(name);
		At<Core::CategoryNode>(checker, name, category.categoryId);
		checker.Require(!category.categoryId.empty(), "categoryId", "is required");
		checker.Require(!category.displayName.empty(), "displayName", "is required");
	}
}


void CheckItems(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	const auto ages = Ids<Core::AgeNode>(registry, &Core::AgeNode::ageId);
	const auto categories = Ids<Core::CategoryNode>(registry, &Core::CategoryNode::categoryId);
	for (const std::string &name : registry.Names<Core::ItemNode>())
	{
		const Core::ItemNode &item = *registry.Find<Core::ItemNode>(name);
		At<Core::ItemNode>(checker, name, item.itemId);
		checker.Require(!item.itemId.empty(), "itemId", "is required");
		checker.Require(!item.displayName.empty(), "displayName", "is required");
		checker.Require(
		    categories.contains(item.categoryId), "categoryId",
		    std::format("'{}' names no category", item.categoryId));
		checker.Require(
		    ages.contains(item.ageId), "ageId", std::format("'{}' names no age", item.ageId));
		checker.Require(item.unitType.has_value(), "unitType", "is required");
		checker.Require(Positive(item.unitsPerCubicMetre), "unitsPerCubicMetre", "must be above 0");
		checker.Require(Positive(item.density), "density", "must be above 0");
		checker.Require(
		    Positive(item.packFactor) && item.packFactor <= 1.0, "packFactor", "must be in (0, 1]");
		checker.Require(
		    Core::Simulation::IsValidBasePrice(item.basePrice), "basePrice",
		    std::format(
		        "'{}' is not a nonnegative price in microcredits with room for the market's "
		        "multipliers",
		        item.basePrice));
	}
}


void CheckSkills(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	const auto ages = Ids<Core::AgeNode>(registry, &Core::AgeNode::ageId);
	const Core::EffectCatalogue effects = Core::EffectCatalogue::Default();
	for (const std::string &name : registry.Names<Core::SkillNode>())
	{
		const Core::SkillNode &skill = *registry.Find<Core::SkillNode>(name);
		At<Core::SkillNode>(checker, name, skill.skillId);
		checker.Require(!skill.skillId.empty(), "skillId", "is required");
		checker.Require(
		    skill.nodeType != Core::NodeType::Tech || ages.contains(skill.skillId), "nodeType",
		    "is Tech, and the skillId names no age");
		checker.Require(skill.maxTiers >= 1, "maxTiers", "must be at least 1");
		checker.Require(skill.resourceCostPerTier >= 1, "resourceCostPerTier", "must be at least 1");
		checker.Require(std::isfinite(skill.gridX), "gridX", "must be finite");
		checker.Require(std::isfinite(skill.gridY), "gridY", "must be finite");
		checker.Require(
		    skill.targetEffectID.empty() || effects.Has(skill.targetEffectID), "targetEffectID",
		    std::format("'{}' names no effect", skill.targetEffectID));
		for (const Core::Prerequisite &prerequisite : skill.requiredSkills)
		{
			const auto *pRequired = registry.Find<Core::SkillNode>(RegistryName(prerequisite.nodeId));
			checker.Require(
			    pRequired != nullptr && pRequired->skillId == prerequisite.nodeId, "requires",
			    std::format("'{}' names no skill", prerequisite.nodeId));
			checker.Require(
			    prerequisite.requiredTier >= 1 && prerequisite.requiredTier <= pRequired->maxTiers,
			    "requires",
			    std::format(
			        "'{}' asks for tier {}, of {}", prerequisite.nodeId, prerequisite.requiredTier,
			        pRequired->maxTiers));
		}
	}

	// The whole graph, as well as each mode's: a cycle among skills no mode selects is still one.
	std::vector<Core::SkillNode> all;
	for (const Core::SkillNode *pSkill : All<Core::SkillNode>(registry))
		all.push_back(*pSkill);
	Core::ProgressionManager progression;
	std::string problem;
	if (progression.Build(all, problem))
		return;

	// Reported against the skill whose own prerequisites fail with the fewest skills behind it, the
	// first in name order of those: a skill in the cycle, not one that only depends on it.
	std::string blamed;
	std::size_t smallest = 0;
	std::string reason;
	for (const std::string &name : registry.Names<Core::SkillNode>())
	{
		std::vector<Core::SkillNode> closure;
		std::set<std::string, std::less<>> reached;
		std::vector<const Core::SkillNode *> pending{registry.Find<Core::SkillNode>(name)};
		while (!pending.empty())
		{
			const Core::SkillNode *pNode = pending.back();
			pending.pop_back();
			if (!reached.insert(pNode->skillId).second)
				continue;
			closure.push_back(*pNode);
			for (const Core::Prerequisite &prerequisite : pNode->requiredSkills)
				pending.push_back(registry.Find<Core::SkillNode>(RegistryName(prerequisite.nodeId)));
		}
		std::string own;
		if ((blamed.empty() || closure.size() < smallest) &&
		    !Core::ProgressionManager{}.Build(closure, own))
		{
			blamed = name;
			smallest = closure.size();
			reason = std::move(own);
		}
	}
	if (!blamed.empty())
	{
		if (reason.ends_with('.'))
			reason.pop_back();
		At<Core::SkillNode>(checker, blamed, registry.Find<Core::SkillNode>(blamed)->skillId);
		checker.Require(false, "requires", reason);
	}
	throw ContentError{
	    std::format("{}: {}", TypeOf(entt::resolve<Core::SkillNode>()).directory, problem)};
}


void CheckNamePools(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	const std::vector<std::string> pools = registry.Names<NamePoolDefinition>();
	if (pools.empty())
		throw ContentError{std::format(
		    "{}: there is no name pool", TypeOf(entt::resolve<NamePoolDefinition>()).directory)};
	for (const std::string &name : pools)
	{
		const NamePoolDefinition &pool = *registry.Find<NamePoolDefinition>(name);
		At<NamePoolDefinition>(checker, name, pool.pool);
		checker.Require(!pool.pool.empty(), "pool", "is required");
		checker.Require(pool.pool == name, "pool", "must be the record's name");
		checker.Require(!pool.names.empty(), "names", "holds none");
		for (const std::string &entry : pool.names)
			checker.Require(!entry.empty(), "names", "holds an empty name");
	}
}


void CheckHulls(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	const Core::EffectCatalogue effects = Core::EffectCatalogue::Default();
	for (const std::string &name : registry.Names<Core::ShipClassSpec>())
	{
		const Core::ShipClassSpec &hull = *registry.Find<Core::ShipClassSpec>(name);
		At<Core::ShipClassSpec>(checker, name, hull.hullId);
		checker.Require(!hull.hullId.empty(), "hullId", "is required");
		checker.Require(!hull.className.empty(), "className", "is required");
		checker.Require(!hull.baseStats.empty(), "baseStats", "holds none");
		for (const auto &[stat, value] : hull.baseStats)
			checker.Require(
			    std::isfinite(value) && value >= 0.0, "baseStats",
			    std::format("{} must be finite and not below 0", Core::StatName(stat)));
		for (const std::string &effect : hull.fitableEffects)
			checker.Require(
			    effects.Has(effect), "fitableEffects", std::format("'{}' names no effect", effect));
		checker.Require(hull.occupants.value_or(1) >= 1, "occupants", "must be at least 1");
		checker.Require(
		    hull.scrubberCartridges.value_or(1) >= 1, "scrubberCartridges", "must be at least 1");
		if (hull.oxygenTanks)
		{
			checker.Require(!hull.oxygenTanks->empty(), "oxygenTanks", "holds none");
			for (const double capacity : *hull.oxygenTanks)
				checker.Require(Positive(capacity), "oxygenTanks", "a capacity must be above 0");
		}
	}
}


bool Fraction(double value)
{
	return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}


bool NotNegative(double value)
{
	return std::isfinite(value) && value >= 0.0;
}


// Each quantity names an item, and is above 0.
void CheckQuantities(
    Checker &checker, const Jettison::Sim::DefinitionRegistry &registry,
    const std::vector<ItemQuantity> &quantities, std::string_view field)
{
	checker.Require(!quantities.empty(), field, "holds none");
	for (const ItemQuantity &quantity : quantities)
	{
		const auto *pItem = registry.Find<Core::ItemNode>(RegistryName(quantity.itemId));
		checker.Require(
		    pItem != nullptr && pItem->itemId == quantity.itemId, field,
		    std::format("'{}' names no item", quantity.itemId));
		checker.Require(
		    Positive(quantity.quantity), field,
		    std::format("'{}' must be a quantity above 0", quantity.itemId));
	}
}


void CheckTuning(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	for (const std::string &name : registry.Names<TuningDefinition>())
	{
		const TuningDefinition &tuning = *registry.Find<TuningDefinition>(name);
		At<TuningDefinition>(checker, name, name);
		checker.Require(
		    Positive(tuning.economy.intervalSeconds), "economy.intervalSeconds", "must be above 0");
		checker.Require(
		    Fraction(tuning.economy.wageTaxRate) && tuning.economy.wageTaxRate < 1.0,
		    "economy.wageTaxRate", "must be in [0, 1)");

		checker.Require(!tuning.needs.empty(), "needs", "holds none");
		std::set<std::string, std::less<>> needs;
		for (const NeedDefinition &need : tuning.needs)
		{
			checker.Require(!need.needId.empty(), "needs", "a needId is required");
			checker.Require(
			    needs.insert(need.needId).second, "needs",
			    std::format("'{}' is given twice", need.needId));
			checker.Require(
			    Positive(need.weight), "needs",
			    std::format("'{}' weight must be above 0", need.needId));
			checker.Require(
			    Positive(need.perThousandPerDay), "needs",
			    std::format("'{}' perThousandPerDay must be above 0", need.needId));
			checker.Require(
			    need.unit.has_value(), "needs", std::format("'{}' unit is required", need.needId));
			std::set<std::string, std::less<>> items;
			for (const ItemSelection &selection : need.items)
			{
				const auto *pItem = registry.Find<Core::ItemNode>(RegistryName(selection.itemId));
				checker.Require(
				    pItem != nullptr && pItem->itemId == selection.itemId, "needs",
				    std::format("'{}' item '{}' names nothing", need.needId, selection.itemId));
				checker.Require(
				    items.insert(selection.itemId).second, "needs",
				    std::format("'{}' selects '{}' twice", need.needId, selection.itemId));
				// Met 1:1 in the need's unit: an item in another needs a declared conversion first.
				checker.Require(
				    pItem->unitType == need.unit, "needs",
				    std::format(
				        "'{}' item '{}' is not traded in the need's unit", need.needId,
				        selection.itemId));
			}
		}

		const ConditionTuning &condition = tuning.condition;
		checker.Require(
		    NotNegative(condition.initial) && condition.initial <= 100.0, "condition.initial",
		    "must be in [0, 100]");
		for (const auto &[rate, field] :
		     {std::pair{condition.healthLossPerHour, "condition.healthLossPerHour"},
		      {condition.happinessLossPerHour, "condition.happinessLossPerHour"},
		      {condition.healthRecoveryPerHour, "condition.healthRecoveryPerHour"},
		      {condition.happinessRecoveryPerHour, "condition.happinessRecoveryPerHour"}})
			checker.Require(Positive(rate), field, "must be above 0");
		checker.Require(
		    Fraction(condition.workRateFloor), "condition.workRateFloor", "must be in [0, 1]");

		checker.Require(
		    Positive(tuning.lifeSupport.oxygenKilogramsPerOccupantDay),
		    "lifeSupport.oxygenKilogramsPerOccupantDay", "must be above 0");
		checker.Require(
		    Positive(tuning.lifeSupport.cartridgeOccupantDays), "lifeSupport.cartridgeOccupantDays",
		    "must be above 0");
		checker.Require(
		    Positive(tuning.concessions.termHours), "concessions.termHours", "must be above 0");
		checker.Require(
		    Fraction(tuning.procurement.performanceSecurityRate), "procurement.performanceSecurityRate",
		    "must be in [0, 1]");

		std::set<std::string, std::less<>> recipes;
		for (const RecipeDefinition &recipe : tuning.recipes)
		{
			checker.Require(!recipe.recipeId.empty(), "recipes", "a recipeId is required");
			checker.Require(
			    recipes.insert(recipe.recipeId).second, "recipes",
			    std::format("'{}' is given twice", recipe.recipeId));
			CheckQuantities(checker, registry, recipe.inputs, "recipes");
			CheckQuantities(checker, registry, recipe.outputs, "recipes");
		}
		std::set<std::string, std::less<>> projects;
		for (const ProjectDefinition &project : tuning.projects)
		{
			checker.Require(!project.projectId.empty(), "projects", "a projectId is required");
			checker.Require(
			    projects.insert(project.projectId).second, "projects",
			    std::format("'{}' is given twice", project.projectId));
			for (const SkillSelection &skill : project.requiredSkills)
			{
				const auto *pSkill = registry.Find<Core::SkillNode>(RegistryName(skill.skillId));
				checker.Require(
				    pSkill != nullptr && pSkill->skillId == skill.skillId, "projects",
				    std::format(
				        "'{}' requires '{}', which names no skill", project.projectId, skill.skillId));
			}
			CheckQuantities(checker, registry, project.inputs, "projects");
			checker.Require(
			    recipes.contains(project.unlocksRecipe), "projects",
			    std::format(
			        "'{}' unlocks '{}', which names no recipe", project.projectId,
			        project.unlocksRecipe));
		}
	}
}


// Each selection names a definition of its type, once.
template <typename T, typename Selection>
void CheckSelections(
    Checker &checker, const Jettison::Sim::DefinitionRegistry &registry,
    const std::vector<Selection> &selections, std::string Selection::*selected, std::string T::*id,
    std::string_view field)
{
	std::set<std::string, std::less<>> seen;
	for (const Selection &selection : selections)
	{
		const std::string &wanted = selection.*selected;
		const T *pDefinition = registry.Find<T>(RegistryName(wanted));
		checker.Require(
		    pDefinition != nullptr && pDefinition->*id == wanted, field,
		    std::format("'{}' names nothing", wanted));
		checker.Require(seen.insert(wanted).second, field, std::format("selects '{}' twice", wanted));
	}
}


Core::GameModeNode Resolve(
    const Jettison::Sim::DefinitionRegistry &registry, const GameModeDefinition &mode)
{
	Core::GameModeNode resolved;
	for (const ItemSelection &item : mode.items)
		resolved.items.push_back(*registry.Find<Core::ItemNode>(RegistryName(item.itemId)));
	for (const SkillSelection &skill : mode.skills)
	{
		Core::SkillNode &node =
		    resolved.skills.emplace_back(*registry.Find<Core::SkillNode>(RegistryName(skill.skillId)));
		if (node.displayName.empty())
			node.displayName = node.skillId;
	}
	return resolved;
}


void CheckGameModes(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	for (const std::string &name : registry.Names<GameModeDefinition>())
	{
		const GameModeDefinition &mode = *registry.Find<GameModeDefinition>(name);
		At<GameModeDefinition>(checker, name, name);
		checker.Require(!name.empty(), "name", "is required: the record's file has none");
		CheckSelections(
		    checker, registry, mode.ages, &AgeSelection::ageId, &Core::AgeNode::ageId, "ages");
		CheckSelections(
		    checker, registry, mode.items, &ItemSelection::itemId, &Core::ItemNode::itemId, "items");
		CheckSelections(
		    checker, registry, mode.skills, &SkillSelection::skillId, &Core::SkillNode::skillId,
		    "skills");

		// The tree a mode selects must be one the game can play: prerequisites inside it, no cycle,
		// no branch out of reach.
		Core::ProgressionManager progression;
		std::string problem;
		checker.Require(progression.Build(Resolve(registry, mode).skills, problem), "skills", problem);
	}
}
} // namespace


ContentSet LoadContent(
    const Jettison::Vfs::FileSystem &files, Jettison::Sim::DefinitionRegistry &registry,
    std::string_view gameMode)
{
	LoadDefinitions(registry);

	Checker checker{files};
	CheckAges(checker, registry);
	CheckCategories(checker, registry);
	CheckItems(checker, registry);
	CheckSkills(checker, registry);
	CheckNamePools(checker, registry);
	CheckHulls(checker, registry);
	CheckTuning(checker, registry);
	CheckGameModes(checker, registry);

	const auto *pMode = registry.Find<GameModeDefinition>(gameMode);
	if (pMode == nullptr)
		throw ContentError{std::format(
		    "{}: there is no game mode '{}'", TypeOf(entt::resolve<GameModeDefinition>()).directory,
		    gameMode)};

	ContentSet content;
	for (const AgeSelection &age : pMode->ages)
		content.ages.push_back(*registry.Find<Core::AgeNode>(RegistryName(age.ageId)));
	for (const Core::CategoryNode *pCategory : All<Core::CategoryNode>(registry))
	{
		Core::CategoryNode &category = content.categories.emplace_back(*pCategory);
		if (category.type.empty())
			category.type = category.categoryId;
		if (category.description.empty())
			category.description = category.displayName;
	}
	content.gameMode = Resolve(registry, *pMode);
	for (const NamePoolDefinition *pPool : All<NamePoolDefinition>(registry))
		content.names.AddPool(pPool->pool, pPool->names);
	const auto *pTuning = registry.Find<TuningDefinition>("balance");
	if (pTuning == nullptr)
		throw ContentError{std::format(
		    "{}: there is no balance tuning", TypeOf(entt::resolve<TuningDefinition>()).directory)};
	content.tuning = *pTuning;
	for (const Core::ShipClassSpec *pHull : All<Core::ShipClassSpec>(registry))
		content.hulls.push_back(*pHull);
	content.identity = BuildContentIdentity(registry);
	return content;
}


ContentSet LoadContent(std::span<const Jettison::Vfs::Mount> sources, std::string_view gameMode)
{
	Jettison::Vfs::FileSystem files;
	for (const Jettison::Vfs::Mount &source : sources)
		files.AddMount({source.name, source.source});
	Jettison::Sim::DefinitionRegistry registry{files};
	return LoadContent(files, registry, gameMode);
}
} // namespace Manifest::Content
