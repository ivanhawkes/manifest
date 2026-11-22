#include <manifest/content/Content.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <jettison/celestial/StarSystem.h>
#include <jettison/dock/Definitions.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/sim/Snapshot.h>
#include <jettison/space/Cruise.h>
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


template <typename Range, typename Value, typename Projection>
bool Contains(const Range &range, const Value &value, Projection projection)
{
	return std::ranges::find(range, value, projection) != std::ranges::end(range);
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
		if (const auto volume = hull.baseStats.find(Core::EffectStat::CargoVolume);
		    volume != hull.baseStats.end())
			checker.Require(
			    volume->second <= static_cast<double>(Core::kMaxHoldTotes) &&
			        std::trunc(volume->second) == volume->second,
			    "baseStats",
			    std::format(
			        "CargoVolume must be a whole number of totes up to {}", Core::kMaxHoldTotes));
		for (const std::string &effect : hull.fitableEffects)
			checker.Require(
			    effects.Has(effect), "fitableEffects", std::format("'{}' names no effect", effect));
		checker.Require(hull.occupants.value_or(0) >= 0, "occupants", "must not be negative");
		checker.Require(
		    hull.scrubberCartridges.value_or(1) >= 1, "scrubberCartridges", "must be at least 1");
		checker.Require(
		    Positive(hull.pelletMagazine.value_or(1.0)), "pelletMagazine", "must be above 0");
		if (hull.oxygenTanks)
		{
			checker.Require(!hull.oxygenTanks->empty(), "oxygenTanks", "holds none");
			for (const double capacity : *hull.oxygenTanks)
				checker.Require(Positive(capacity), "oxygenTanks", "a capacity must be above 0");
		}
		// Jolt keeps a body's mass as a float, which must hold it above 0.
		const double mass = hull.mass.value_or(1.0);
		checker.Require(
		    Positive(mass) && mass <= std::numeric_limits<float>::max() &&
		        static_cast<float>(mass) >= std::numeric_limits<float>::min(),
		    "mass", "must be above 0 and within a float's normal range");
	}
}


void CheckOffers(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	const Core::EffectCatalogue effects = Core::EffectCatalogue::Default();
	for (const std::string &name : registry.Names<OfferDefinition>())
	{
		const OfferDefinition &offer = *registry.Find<OfferDefinition>(name);
		At<OfferDefinition>(checker, name, offer.offerId);
		checker.Require(!offer.offerId.empty(), "offerId", "is required");
		checker.Require(offer.target.has_value(), "target", "is required");
		checker.Require(offer.cost >= 1, "cost", "must be at least 1 credit");
		const auto *pSkill = registry.Find<Core::SkillNode>(RegistryName(offer.skillId));
		checker.Require(
		    pSkill != nullptr && pSkill->skillId == offer.skillId, "skillId",
		    std::format("'{}' names no skill", offer.skillId));
		const Core::EffectDefinition *pEffect = effects.Find(pSkill->targetEffectID);
		checker.Require(
		    pSkill->nodeType == Core::NodeType::Skill && pEffect != nullptr &&
		        pEffect->domain == Core::EffectDomain::Ship,
		    "skillId", std::format("'{}' is no ship system", offer.skillId));
		checker.Require(
		    offer.maxTier >= 1 && offer.maxTier <= pSkill->maxTiers, "maxTier",
		    std::format("must be from 1 to its skill's {}", pSkill->maxTiers));
	}
}


Jettison::Dock::PadClasses PadClassesOf(const Jettison::Sim::DefinitionRegistry &registry)
{
	Jettison::Dock::PadClasses classes;
	for (const std::string &name : registry.Names<Jettison::Dock::PadClassDefinition>())
		classes.emplace(name, *registry.Find<Jettison::Dock::PadClassDefinition>(name));
	return classes;
}


// A new player starts only where they can dock the starter and connect to working life support
// (D-047, D-089): a venue that is public and operational, has life support, and has a pad of its
// layout, not for display, that the starter's dock hull fits.
bool StartsPlayers(
    const Jettison::Sim::DefinitionRegistry &registry, const Jettison::Dock::PadClasses &classes,
    const ScenarioDefinition &scenario, const VenueDefinition &venue)
{
	const auto *pStarterDock =
	    registry.Find<Jettison::Dock::HullDefinition>(RegistryName(scenario.starterHull));
	const auto *pLayout = registry.Find<Jettison::Dock::StationLayout>(venue.layout);
	// The scenario's traders take a pad and a connection each, and a player needs one more.
	const auto traders = std::ssize(scenario.traders);
	return venue.isPublic && venue.operational && venue.lifeSupportCapacity > traders &&
	       pStarterDock != nullptr && pLayout != nullptr &&
	       std::ranges::count_if(
	           pLayout->pads, [&](const Jettison::Dock::PadLayout &pad)
	           { return !pad.display && Jettison::Dock::Fits(classes, *pStarterDock, pad.padClass); }) >
	           traders;
}


// The engine's own check of a record (Dock::CheckHull and the like), whose FormatError names what
// does not fit, reported against the record as any other broken rule is.
template <typename Check> void RequireDock(const Checker &checker, Check check)
{
	try
	{
		check();
	}
	catch (const Jettison::Serialise::FormatError &error)
	{
		checker.Require(false, "dock", error.what());
	}
}


// Jettison's ship, station and star-system definitions as Manifest authors them (D-062): each checked
// as the engine checks it (a system as Resolve does), every Manifest.Hull with the Dock.HullDefinition
// and the Ai.CruiseDrive of its name, so a hull has a size, a pad class and a drive, and the reverse,
// and each system named by its record.
void CheckDocks(Checker &checker, const Jettison::Sim::DefinitionRegistry &registry)
{
	namespace Dock = Jettison::Dock;
	const Dock::PadClasses classes = PadClassesOf(registry);
	for (const auto &[name, padClass] : classes)
	{
		At<Dock::PadClassDefinition>(checker, name, name);
		RequireDock(checker, [&] { Dock::CheckPadClass(name, padClass); });
	}
	for (const std::string &name : registry.Names<Dock::HullDefinition>())
	{
		At<Dock::HullDefinition>(checker, name, name);
		RequireDock(
		    checker,
		    [&] { Dock::CheckHull(name, *registry.Find<Dock::HullDefinition>(name), classes); });
		checker.Require(
		    registry.Find<Core::ShipClassSpec>(name) != nullptr, "dock",
		    "is the dock hull of no Manifest.Hull of its name");
	}
	for (const std::string &name : registry.Names<Dock::StationLayout>())
	{
		At<Dock::StationLayout>(checker, name, name);
		RequireDock(
		    checker,
		    [&] { Dock::CheckLayout(name, *registry.Find<Dock::StationLayout>(name), classes); });
	}
	for (const std::string &name : registry.Names<Jettison::Space::CruiseDrive>())
	{
		At<Jettison::Space::CruiseDrive>(checker, name, name);
		RequireDock(
		    checker,
		    [&]
		    {
			    Jettison::Space::CheckCruiseDrive(
			        name, *registry.Find<Jettison::Space::CruiseDrive>(name));
		    });
		checker.Require(
		    registry.Find<Core::ShipClassSpec>(name) != nullptr, "dock",
		    "is the drive of no Manifest.Hull of its name");
	}
	for (const std::string &name : registry.Names<Core::ShipClassSpec>())
	{
		At<Core::ShipClassSpec>(checker, name, registry.Find<Core::ShipClassSpec>(name)->hullId);
		checker.Require(
		    registry.Find<Dock::HullDefinition>(name) != nullptr, "dock",
		    std::format("has no Dock.HullDefinition '{}'", name));
		// A ship is a box of its dock hull's size, whose largest moment of inertia, its mass times its
		// two larger sides squared over 12, Jolt keeps and decomposes in floats: held far below a
		// float's largest, so that arithmetic cannot overflow.
		constexpr double kMaxInertia = 1e18;
		if (const std::optional<double> mass = registry.Find<Core::ShipClassSpec>(name)->mass)
		{
			const Dock::HullDefinition &dock = *registry.Find<Dock::HullDefinition>(name);
			std::array<double, 3> sides{dock.length, dock.beam, dock.height};
			std::ranges::sort(sides);
			checker.Require(
			    *mass * (sides[1] * sides[1] + sides[2] * sides[2]) / 12.0 <= kMaxInertia, "mass",
			    std::format(
			        "gives its {} x {} x {} m box an inertia past {} kg m^2", dock.length, dock.beam,
			        dock.height, kMaxInertia));
		}
		checker.Require(
		    registry.Find<Jettison::Space::CruiseDrive>(name) != nullptr, "dock",
		    std::format("has no Ai.CruiseDrive '{}'", name));
	}
	for (const std::string &name : registry.Names<Jettison::Celestial::StarSystem>())
	{
		Jettison::Celestial::StarSystem system = *registry.Find<Jettison::Celestial::StarSystem>(name);
		At<Jettison::Celestial::StarSystem>(checker, name, system.name);
		checker.Require(
		    RegistryName(system.name) == name, "name",
		    std::format("'{}' is not the system of the record's name", system.name));
		RequireDock(checker, [&] { Jettison::Celestial::Resolve(system); });
	}
}


// A scenario's system as its record has it, resolved; null for one with no record, or one that does
// not resolve.
std::optional<Jettison::Celestial::StarSystem> ResolvedSystem(
    const Jettison::Sim::DefinitionRegistry &registry, std::string_view systemId)
{
	const auto *pSystem = registry.Find<Jettison::Celestial::StarSystem>(RegistryName(systemId));
	if (pSystem == nullptr || pSystem->name != systemId)
		return std::nullopt;
	Jettison::Celestial::StarSystem system = *pSystem;
	try
	{
		Jettison::Celestial::Resolve(system);
	}
	catch (const Jettison::Serialise::FormatError &)
	{
		return std::nullopt;
	}
	return system;
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
		// A credit is withheld for each whole 1 / rate of gross paid, carried between payments.
		checker.Require(
		    tuning.economy.wageTaxRate <= 0.0 ||
		        std::abs(
		            1.0 / tuning.economy.wageTaxRate - std::round(1.0 / tuning.economy.wageTaxRate)) <
		            1e-9,
		    "economy.wageTaxRate", "must be 0 or one credit in a whole number of credits");

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

		const auto categories = Ids<Core::CategoryNode>(registry, &Core::CategoryNode::categoryId);
		std::set<std::string, std::less<>> buckets;
		std::set<std::string, std::less<>> bucketed;
		for (const DemandBucket &bucket : tuning.optionalGoods)
		{
			checker.Require(!bucket.bucketId.empty(), "optionalGoods", "a bucketId is required");
			checker.Require(
			    buckets.insert(bucket.bucketId).second, "optionalGoods",
			    std::format("'{}' is given twice", bucket.bucketId));
			checker.Require(
			    NotNegative(bucket.premium), "optionalGoods",
			    std::format("'{}' premium must be at least 0", bucket.bucketId));
			checker.Require(
			    Positive(bucket.demandShare) && bucket.demandShare <= 1.0, "optionalGoods",
			    std::format("'{}' demandShare must be in (0, 1]", bucket.bucketId));
			for (const CategorySelection &selection : bucket.categories)
			{
				checker.Require(
				    categories.contains(selection.categoryId), "optionalGoods",
				    std::format(
				        "'{}' category '{}' names nothing", bucket.bucketId, selection.categoryId));
				checker.Require(
				    bucketed.insert(selection.categoryId).second, "optionalGoods",
				    std::format("'{}' is in more than one bucket", selection.categoryId));
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
		    Positive(tuning.production.batchSeconds), "production.batchSeconds", "must be above 0");
		checker.Require(
		    Fraction(tuning.production.wageShare) && tuning.production.wageShare < 1.0,
		    "production.wageShare", "must be in [0, 1)");

		checker.Require(
		    Positive(tuning.lifeSupport.oxygenKilogramsPerOccupantDay),
		    "lifeSupport.oxygenKilogramsPerOccupantDay", "must be above 0");
		checker.Require(
		    Positive(tuning.lifeSupport.cartridgeOccupantDays), "lifeSupport.cartridgeOccupantDays",
		    "must be above 0");
		for (const auto &[itemId, unit, field] :
		     {std::tuple{
		          tuning.lifeSupport.oxygenItemId, Core::TradeUnit::Kilogram,
		          "lifeSupport.oxygenItemId"},
		      {tuning.lifeSupport.cartridgeItemId, Core::TradeUnit::Unit,
		       "lifeSupport.cartridgeItemId"}})
		{
			const auto *pItem = registry.Find<Core::ItemNode>(RegistryName(itemId));
			checker.Require(
			    pItem != nullptr && pItem->itemId == itemId && pItem->unitType == unit, field,
			    std::format("'{}' names no item traded in its unit", itemId));
		}
		const FuelTuning &fuel = tuning.fuel;
		checker.Require(Positive(fuel.exhaustKms), "fuel.exhaustKms", "must be above 0");
		checker.Require(
		    Positive(fuel.pelletMilligramsPerTonneKms), "fuel.pelletMilligramsPerTonneKms",
		    "must be above 0");
		checker.Require(fuel.curveExponent >= 0, "fuel.curveExponent", "must not be below 0");
		checker.Require(
		    NotNegative(fuel.tankTonnesPerTonne), "fuel.tankTonnesPerTonne", "must not be below 0");
		checker.Require(
		    NotNegative(fuel.occupantKilograms), "fuel.occupantKilograms", "must not be below 0");
		checker.Require(
		    tuning.company.startingCredits > 0, "company.startingCredits", "must be above 0");
		checker.Require(
		    tuning.market.listingFeeBps >= 0 && tuning.market.listingFeeBps <= 10000,
		    "market.listingFeeBps", "must be in [0, 10000]");
		checker.Require(
		    NotNegative(tuning.market.orderLifeSeconds), "market.orderLifeSeconds",
		    "must be finite and not below 0");
		checker.Require(
		    Positive(tuning.market.salesWindowSeconds), "market.salesWindowSeconds", "must be above 0");
		std::set<std::string, std::less<>> charged;
		for (const ClassChargesTuning &fees : tuning.charges.padClasses)
		{
			checker.Require(
			    registry.Find<Jettison::Dock::PadClassDefinition>(fees.padClass) != nullptr,
			    "charges.padClasses", std::format("'{}' names no pad class", fees.padClass));
			checker.Require(
			    charged.insert(fees.padClass).second, "charges.padClasses",
			    std::format("'{}' is charged twice", fees.padClass));
			checker.Require(
			    fees.dockingFee >= 0 && fees.berthFeePerHour >= 0, "charges.padClasses",
			    std::format("'{}' has a fee below 0", fees.padClass));
		}
		for (const std::string &padClass : registry.Names<Jettison::Dock::PadClassDefinition>())
			checker.Require(
			    charged.contains(padClass), "charges.padClasses",
			    std::format("pad class '{}' has no fees", padClass));
		checker.Require(
		    tuning.charges.berthFeePerOccupantHour >= 0, "charges.berthFeePerOccupantHour",
		    "must not be below 0");
		checker.Require(
		    tuning.charges.maxArrearsRateBps >= 0 && tuning.charges.maxArrearsRateBps <= 1'000'000,
		    "charges.maxArrearsRateBps", "must be in [0, 1000000]");
		checker.Require(
		    tuning.charges.arrearsRateBps >= 0 &&
		        tuning.charges.arrearsRateBps <= tuning.charges.maxArrearsRateBps,
		    "charges.arrearsRateBps", "must be in [0, charges.maxArrearsRateBps]");
		checker.Require(
		    tuning.charges.seizureGraceDays >= 0 && tuning.charges.seizureGraceDays <= 36'500,
		    "charges.seizureGraceDays", "must be in [0, 36500]");
		const PricingTuning &pricing = tuning.charges.pricing;
		checker.Require(
		    pricing.lowerBelowBps >= 0 && pricing.lowerBelowBps <= pricing.raiseAboveBps &&
		        pricing.raiseAboveBps <= 10'000,
		    "charges.pricing", "needs 0 <= lowerBelowBps <= raiseAboveBps <= 10000");
		checker.Require(
		    pricing.stepBps >= 0 && pricing.stepBps <= 1'000'000 && pricing.minLevelBps >= 0 &&
		        pricing.minLevelBps <= 10'000 && pricing.maxLevelBps >= 10'000 &&
		        pricing.maxLevelBps <= 1'000'000,
		    "charges.pricing",
		    "needs stepBps in [0, 1000000] and 0 <= minLevelBps <= 10000 <= maxLevelBps <= 1000000");
		checker.Require(tuning.brokers.treasury >= 0, "brokers.treasury", "must not be below 0");
		checker.Require(
		    tuning.brokers.spreadBps >= 0 && tuning.brokers.spreadBps < 20000, "brokers.spreadBps",
		    "must be in [0, 20000)");
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

		if (mode.cruise)
			for (const auto &[crew, profile] :
			     {std::pair{"manned", mode.cruise->manned},
			      std::pair{"unmanned", mode.cruise->unmanned}})
				RequireDock(
				    checker,
				    [&]
				    {
					    Jettison::Space::CheckCruiseDrive(
					        std::format("{} {}", name, crew),
					        Jettison::Space::CruiseDrive{.profile = profile});
				    });

		// The tree a mode selects must be one the game can play: prerequisites inside it, no cycle,
		// no branch out of reach.
		Core::ProgressionManager progression;
		std::string problem;
		checker.Require(progression.Build(Resolve(registry, mode).skills, problem), "skills", problem);
	}
}


// The IDs of one of a scenario's lists, each required and given once.
template <typename T>
std::set<std::string, std::less<>> ListIds(
    const Checker &checker, const std::vector<T> &list, std::string T::*id, std::string_view field)
{
	std::set<std::string, std::less<>> ids;
	for (const T &entry : list)
	{
		checker.Require(!(entry.*id).empty(), field, "an ID is required");
		checker.Require(
		    ids.insert(entry.*id).second, field, std::format("'{}' is given twice", entry.*id));
	}
	return ids;
}


// A region's projects: known prerequisites, an operational workshop, and inputs some venue makes.
void CheckProjects(
    const Checker &checker, const ScenarioDefinition &scenario, const TuningDefinition &tuning,
    const RegionDefinition &region)
{
	for (const ProjectSelection &selection : region.projects)
	{
		const auto project =
		    std::ranges::find(tuning.projects, selection.projectId, &ProjectDefinition::projectId);
		checker.Require(
		    project != tuning.projects.end(), "regions",
		    std::format("'{}' project '{}' names no project", region.regionId, selection.projectId));
		for (const SkillSelection &skill : project->requiredSkills)
			checker.Require(
			    Contains(region.skills, skill.skillId, &SkillSelection::skillId), "regions",
			    std::format(
			        "'{}' project '{}' requires '{}', which the region does not know", region.regionId,
			        project->projectId, skill.skillId));
		// shortcut: every project is metallurgy's, worked at a workshop; a project names its
		// facility when a second needs another.
		checker.Require(
		    std::ranges::any_of(
		        region.facilities, [](const FacilityDefinition &facility)
		        { return facility.kind == FacilityKind::Workshop && facility.operational; }),
		    "regions",
		    std::format(
		        "'{}' project '{}' needs an operational workshop", region.regionId,
		        project->projectId));

		std::vector<ItemQuantity> needed = project->inputs;
		const auto recipe =
		    std::ranges::find(tuning.recipes, project->unlocksRecipe, &RecipeDefinition::recipeId);
		needed.insert(needed.end(), recipe->inputs.begin(), recipe->inputs.end());
		for (const ItemQuantity &input : needed)
		{
			const bool made = std::ranges::any_of(
			    scenario.venues,
			    [&](const VenueDefinition &venue)
			    {
				    return std::ranges::any_of(
				        venue.market, [&](const MarketEntry &entry)
				        { return entry.itemId == input.itemId && entry.production > 0.0; });
			    });
			checker.Require(
			    made, "regions",
			    std::format(
			        "'{}' project '{}' needs '{}', which no venue produces", region.regionId,
			        project->projectId, input.itemId));
		}
	}
}


void CheckScenarios(
    Checker &checker, const Jettison::Sim::DefinitionRegistry &registry, const TuningDefinition &tuning)
{
	const Jettison::Dock::PadClasses classes = PadClassesOf(registry);
	for (const std::string &name : registry.Names<ScenarioDefinition>())
	{
		const ScenarioDefinition &scenario = *registry.Find<ScenarioDefinition>(name);
		At<ScenarioDefinition>(checker, name, name);

		const auto *pMode = registry.Find<GameModeDefinition>(scenario.gameMode);
		checker.Require(
		    pMode != nullptr, "gameMode", std::format("'{}' names no game mode", scenario.gameMode));
		std::set<std::string, std::less<>> items;
		for (const ItemSelection &item : scenario.items)
		{
			checker.Require(
			    Contains(pMode->items, item.itemId, &ItemSelection::itemId), "items",
			    std::format("'{}' is not one of mode '{}''s items", item.itemId, scenario.gameMode));
			checker.Require(
			    items.insert(item.itemId).second, "items",
			    std::format("selects '{}' twice", item.itemId));
		}

		const auto systems = ListIds(checker, scenario.systems, &SystemDefinition::systemId, "systems");
		std::set<std::uint64_t> frames;
		for (const std::string &system : systems)
			checker.Require(
			    frames.insert(Jettison::Sim::FrameId(system)).second, "systems",
			    std::format("'{}' has another system's frame ID", system));
		for (const SystemDefinition &system : scenario.systems)
		{
			std::set<std::string, std::less<>> connected;
			for (const SystemSelection &connection : system.connections)
			{
				checker.Require(
				    systems.contains(connection.systemId) && connection.systemId != system.systemId,
				    "systems",
				    std::format(
				        "'{}' connects to '{}', which is no other system of the scenario",
				        system.systemId, connection.systemId));
				checker.Require(
				    connected.insert(connection.systemId).second, "systems",
				    std::format("'{}' connects to '{}' twice", system.systemId, connection.systemId));
			}
		}

		std::map<std::string, Jettison::Celestial::StarSystem, std::less<>> bodies;
		for (const SystemDefinition &system : scenario.systems)
		{
			std::optional<Jettison::Celestial::StarSystem> resolved =
			    ResolvedSystem(registry, system.systemId);
			checker.Require(
			    resolved.has_value(), "systems",
			    std::format(
			        "'{}' has no Celestial.StarSystem '{}' that resolves", system.systemId,
			        RegistryName(system.systemId)));
			if (resolved)
				bodies.emplace(system.systemId, *std::move(resolved));
		}
		// The body of that name and kind in the system, or null.
		const auto bodyOf = [&](std::string_view systemId, std::string_view name,
		                        std::initializer_list<Jettison::Celestial::BodyKind> kinds)
		    -> const Jettison::Celestial::Body *
		{
			const auto system = bodies.find(systemId);
			if (system == bodies.end())
				return nullptr;
			const int index = system->second.Find(name);
			const Jettison::Celestial::Body *pBody =
			    index < 0 ? nullptr : &system->second.bodies[static_cast<std::size_t>(index)];
			return pBody && std::ranges::find(kinds, pBody->kind) != kinds.end() ? pBody : nullptr;
		};

		const auto planets = ListIds(checker, scenario.planets, &PlanetDefinition::planetId, "planets");
		for (const PlanetDefinition &planet : scenario.planets)
		{
			checker.Require(
			    systems.contains(planet.systemId), "planets",
			    std::format("'{}' system '{}' names nothing", planet.planetId, planet.systemId));
			checker.Require(
			    !bodies.contains(planet.systemId) ||
			        bodyOf(
			            planet.systemId, planet.planetId,
			            {Jettison::Celestial::BodyKind::Planet, Jettison::Celestial::BodyKind::Moon}),
			    "planets",
			    std::format(
			        "'{}' is no planet or moon of system '{}'", planet.planetId, planet.systemId));
		}
		const auto systemOf = [&](std::string_view planetId)
		{
			return std::ranges::find(scenario.planets, planetId, &PlanetDefinition::planetId)->systemId;
		};

		const auto civilisations = ListIds(
		    checker, scenario.civilisations, &CivilisationDefinition::civilisationId, "civilisations");
		for (const CivilisationDefinition &civilisation : scenario.civilisations)
		{
			const auto *pAge = registry.Find<Core::AgeNode>(RegistryName(civilisation.ageId));
			checker.Require(
			    pAge != nullptr && pAge->ageId == civilisation.ageId, "civilisations",
			    std::format(
			        "'{}' age '{}' names nothing", civilisation.civilisationId, civilisation.ageId));
		}

		const auto organisations = ListIds(
		    checker, scenario.organisations, &OrganisationDefinition::organisationId, "organisations");
		for (const OrganisationDefinition &organisation : scenario.organisations)
			checker.Require(
			    organisation.kind.has_value(), "organisations",
			    std::format("'{}' kind is required", organisation.organisationId));

		const auto regions = ListIds(checker, scenario.regions, &RegionDefinition::regionId, "regions");
		for (const RegionDefinition &region : scenario.regions)
		{
			const auto problem = [&](std::string_view text)
			{ return std::format("'{}' {}", region.regionId, text); };
			checker.Require(
			    planets.contains(region.planetId), "regions",
			    problem(std::format("planet '{}' names nothing", region.planetId)));
			checker.Require(
			    civilisations.contains(region.civilisationId), "regions",
			    problem(std::format("civilisation '{}' names nothing", region.civilisationId)));
			checker.Require(
			    region.governorId.empty() || organisations.contains(region.governorId), "regions",
			    problem(std::format("governor '{}' names no organisation", region.governorId)));
			checker.Require(
			    NotNegative(region.population), "regions",
			    problem("population must be finite and not below 0"));
			checker.Require(
			    region.householdFunds >= 0 && region.publicFunds >= 0 && region.employerFunds >= 0,
			    "regions", problem("funds must not be below 0"));
			for (const double condition : {region.health, region.happiness})
				checker.Require(
				    NotNegative(condition) && condition <= 100.0, "regions",
				    problem("health and happiness must be in [0, 100]"));
			for (const SkillSelection &skill : region.skills)
			{
				const auto *pSkill = registry.Find<Core::SkillNode>(RegistryName(skill.skillId));
				checker.Require(
				    pSkill != nullptr && pSkill->skillId == skill.skillId, "regions",
				    problem(std::format("skill '{}' names nothing", skill.skillId)));
			}
			(void)ListIds(checker, region.facilities, &FacilityDefinition::facilityId, "regions");
			for (const FacilityDefinition &facility : region.facilities)
				checker.Require(
				    facility.kind.has_value(), "regions",
				    problem(std::format("facility '{}' kind is required", facility.facilityId)));
			CheckProjects(checker, scenario, tuning, region);
		}

		const auto *pStarter = registry.Find<Core::ShipClassSpec>(RegistryName(scenario.starterHull));
		const bool starterResolves = pStarter != nullptr && pStarter->hullId == scenario.starterHull;
		checker.Require(!scenario.starterHull.empty(), "starterHull", "is required");
		checker.Require(
		    starterResolves, "starterHull", std::format("'{}' names no hull", scenario.starterHull));
		checker.Require(
		    pStarter->mass.has_value(), "starterHull",
		    std::format("'{}' has no mass to fly with", scenario.starterHull));

		(void)ListIds(checker, scenario.venues, &VenueDefinition::venueId, "venues");
		(void)ListIds(checker, scenario.traders, &TraderDefinition::principalId, "traders");
		for (const VenueDefinition &venue : scenario.venues)
		{
			const auto problem = [&](std::string_view text)
			{ return std::format("'{}' {}", venue.venueId, text); };
			checker.Require(
			    systems.contains(venue.systemId), "venues",
			    problem(std::format("system '{}' names nothing", venue.systemId)));
			checker.Require(
			    venue.planetId.empty() ||
			        (planets.contains(venue.planetId) && systemOf(venue.planetId) == venue.systemId),
			    "venues", problem(std::format("planet '{}' is not in its system", venue.planetId)));
			const Jettison::Celestial::Body *pStation =
			    bodyOf(venue.systemId, venue.venueId, {Jettison::Celestial::BodyKind::Station});
			checker.Require(
			    !bodies.contains(venue.systemId) || pStation != nullptr, "venues",
			    problem(std::format("is no station of system '{}'", venue.systemId)));
			checker.Require(
			    !pStation || venue.planetId.empty() || pStation->parent == venue.planetId, "venues",
			    problem(std::format("station does not orbit its planet '{}'", venue.planetId)));
			checker.Require(
			    !pStation || pStation->layout.empty(), "venues",
			    problem("station names a layout of its own; the venue names it"));
			checker.Require(
			    organisations.contains(venue.operatorId), "venues",
			    problem(std::format("operator '{}' names no organisation", venue.operatorId)));
			checker.Require(
			    registry.Find<Jettison::Dock::StationLayout>(venue.layout) != nullptr, "venues",
			    problem(std::format("layout '{}' names nothing", venue.layout)));
			checker.Require(
			    venue.lifeSupportCapacity >= 0, "venues",
			    problem("lifeSupportCapacity must not be below 0"));
			std::set<std::string, std::less<>> served;
			for (const RegionSelection &selection : venue.regions)
			{
				checker.Require(
				    regions.contains(selection.regionId), "venues",
				    problem(std::format("region '{}' names nothing", selection.regionId)));
				checker.Require(
				    served.insert(selection.regionId).second, "venues",
				    problem(std::format("serves '{}' twice", selection.regionId)));
				const auto region = std::ranges::find(
				    scenario.regions, selection.regionId, &RegionDefinition::regionId);
				checker.Require(
				    region->planetId == venue.planetId, "venues",
				    problem(std::format("region '{}' is not on its planet", selection.regionId)));
			}
			std::set<std::string, std::less<>> goods;
			for (const MarketEntry &entry : venue.market)
			{
				checker.Require(
				    items.contains(entry.itemId), "venues",
				    problem(std::format("market item '{}' is not a scenario item", entry.itemId)));
				checker.Require(
				    goods.insert(entry.itemId).second, "venues",
				    problem(std::format("market lists '{}' twice", entry.itemId)));
				checker.Require(
				    NotNegative(entry.production) && NotNegative(entry.demand), "venues",
				    problem(
				        std::format("market '{}' rates must be finite and not below 0", entry.itemId)));
				checker.Require(
				    entry.regionId.empty() || served.contains(entry.regionId), "venues",
				    problem(
				        std::format(
				            "market '{}' region '{}' is not one it serves", entry.itemId,
				            entry.regionId)));
				checker.Require(
				    entry.production <= 0.0 || !entry.regionId.empty(), "venues",
				    problem(std::format("market '{}' production names no region", entry.itemId)));
				checker.Require(
				    entry.stock <= Core::kMaxWarehouseTotes, "venues",
				    problem(
				        std::format(
				            "market '{}' stock is more than a warehouse holds, {} totes", entry.itemId,
				            Core::kMaxWarehouseTotes)));
			}
		}

		// Every region trades every essential at each venue serving it, at the least.
		for (const VenueDefinition &venue : scenario.venues)
			if (!venue.regions.empty())
				for (const NeedDefinition &need : tuning.needs)
					for (const ItemSelection &selection : need.items)
						checker.Require(
						    Contains(venue.market, selection.itemId, &MarketEntry::itemId), "venues",
						    std::format(
						        "'{}' serves a region but trades no '{}', which '{}' needs",
						        venue.venueId, selection.itemId, need.needId));

		const auto starter =
		    std::ranges::find(scenario.venues, scenario.starterVenue, &VenueDefinition::venueId);
		checker.Require(
		    starter != scenario.venues.end(), "starterVenue",
		    std::format("'{}' names none of the scenario's venues", scenario.starterVenue));
		checker.Require(
		    StartsPlayers(registry, classes, scenario, *starter), "starterVenue",
		    std::format(
		        "'{}' must be an operational public station that docks '{}' with life support to spare",
		        scenario.starterVenue, scenario.starterHull));
	}
}


// Loads every definition type and holds the whole set to its rules.
void LoadChecked(const Jettison::Vfs::FileSystem &files, Jettison::Sim::DefinitionRegistry &registry)
{
	LoadDefinitions(registry);

	Checker checker{files};
	CheckAges(checker, registry);
	CheckCategories(checker, registry);
	CheckItems(checker, registry);
	CheckSkills(checker, registry);
	CheckNamePools(checker, registry);
	CheckHulls(checker, registry);
	CheckOffers(checker, registry);
	CheckDocks(checker, registry);
	CheckTuning(checker, registry);
	CheckGameModes(checker, registry);
	const auto *pTuning = registry.Find<TuningDefinition>("balance");
	if (pTuning == nullptr)
		throw ContentError{std::format(
		    "{}: there is no balance tuning", TypeOf(entt::resolve<TuningDefinition>()).directory)};
	CheckScenarios(checker, registry, *pTuning);
}


// The set for a game mode of a checked registry, and the scenario selected, if one is.
ContentSet Build(
    const Jettison::Sim::DefinitionRegistry &registry, std::string_view gameMode,
    std::string_view scenario)
{
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
	content.tuning = *registry.Find<TuningDefinition>("balance");
	for (const std::string &name : registry.Names<ScenarioDefinition>())
		content.scenarios.emplace(name, *registry.Find<ScenarioDefinition>(name));
	for (const Core::ShipClassSpec *pHull : All<Core::ShipClassSpec>(registry))
		content.hulls.push_back(*pHull);
	for (const OfferDefinition *pOffer : All<OfferDefinition>(registry))
		content.offers.push_back(*pOffer);
	content.padClasses = PadClassesOf(registry);
	for (const std::string &name : registry.Names<Jettison::Dock::HullDefinition>())
		content.dockHulls.emplace(name, *registry.Find<Jettison::Dock::HullDefinition>(name));
	for (const std::string &name : registry.Names<Jettison::Space::CruiseDrive>())
	{
		Jettison::Space::CruiseDrive &drive =
		    content.drives.emplace(name, *registry.Find<Jettison::Space::CruiseDrive>(name))
		        .first->second;
		// The checks hold each drive to a hull of its name.
		if (pMode->cruise)
			drive.profile = registry.Find<Core::ShipClassSpec>(name)->occupants == 0
			                    ? pMode->cruise->unmanned
			                    : pMode->cruise->manned;
	}
	for (const std::string &name : registry.Names<Jettison::Dock::StationLayout>())
		content.layouts.emplace(name, *registry.Find<Jettison::Dock::StationLayout>(name));
	content.scenario = scenario;
	if (const auto selected = content.scenarios.find(scenario); selected != content.scenarios.end())
	{
		// The scenario checks hold each to resolving.
		for (const SystemDefinition &system : selected->second.systems)
			content.systems.push_back(*ResolvedSystem(registry, system.systemId));
	}
	content.identity = BuildContentIdentity(registry, scenario);
	return content;
}
} // namespace


ContentSet LoadContent(
    const Jettison::Vfs::FileSystem &files, Jettison::Sim::DefinitionRegistry &registry,
    std::string_view gameMode)
{
	LoadChecked(files, registry);
	return Build(registry, gameMode, {});
}


ContentSet LoadScenario(
    const Jettison::Vfs::FileSystem &files, Jettison::Sim::DefinitionRegistry &registry,
    std::string_view scenario)
{
	LoadChecked(files, registry);
	const auto *pScenario = registry.Find<ScenarioDefinition>(scenario);
	if (pScenario == nullptr)
		throw ContentError{std::format(
		    "{}: there is no scenario '{}'", TypeOf(entt::resolve<ScenarioDefinition>()).directory,
		    scenario)};
	return Build(registry, pScenario->gameMode, scenario);
}


ContentSet LoadContent(std::span<const Jettison::Vfs::Mount> sources, std::string_view gameMode)
{
	Jettison::Vfs::FileSystem files;
	for (const Jettison::Vfs::Mount &source : sources)
		files.AddMount({source.name, source.source});
	Jettison::Sim::DefinitionRegistry registry{files};
	return LoadContent(files, registry, gameMode);
}


std::string FeeClass(const ContentSet &content, std::string_view hullId)
{
	const auto found = content.dockHulls.find(RegistryName(hullId));
	return found == content.dockHulls.end() ? std::string{} : found->second.pad;
}
} // namespace Manifest::Content
