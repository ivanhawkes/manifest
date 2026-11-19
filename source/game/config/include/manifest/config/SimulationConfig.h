#pragma once

#include <optional>
#include <string>
#include <vector>


namespace Manifest::Core
{
// The configuration records, as Jettison's DefinitionRegistry reads them through Manifest::Content
// (docs/stage-2-content-plan.md, "Schema contract"). A field a file leaves out keeps its
// initialiser here; which fields are required, and every rule between records, is the content
// set's validation, not the reader's.

enum class NodeType
{
	Tech,
	Skill
};

// Defines a prerequisite with a specific tier requirement.
struct Prerequisite
{
	std::string nodeId;
	int requiredTier{1};
};

struct AgeNode
{
	// Identifier of the age: the ageId an item names as its age, and the id the
	// skillId of the age's gate in the tree repeats.
	std::string ageId;

	// Display name of the age.
	std::string displayName;

	// Description of the age.
	std::string description;
};

using Ages = std::vector<AgeNode>;

struct CategoryNode
{
	// Identifier of the category: the categoryId an item names as its category.
	std::string categoryId;

	// Display name of the category.
	std::string displayName;

	// Type of the category; empty means the categoryId.
	std::string type;

	// Description of the category; empty means the displayName.
	std::string description;
};

using Categories = std::vector<CategoryNode>;

// Registered by the names the authored files use.
enum class TradeUnit
{
	Carat,
	TroyOunce,
	Kilogram,
	Tonne,
	Litre,
	Kilolitre,
	Unit,
	Sheet,
	SquareMetre
};

struct ItemNode
{
	// Identifier of the item: the itemId a game-mode file names to select the good.
	std::string itemId;

	// Display name of the item.
	std::string displayName;

	// Description of the item.
	std::string description;

	// Category and age links: each names a loaded definition.
	std::string categoryId;
	std::string ageId;

	// The number of units that can fit into one cubic metre of storage.
	double unitsPerCubicMetre{0.0};

	// Universal base price of the item: Decimal text as authored, parsed by Decimal::TryParse
	// where it is used, since a float in between would round the microcredits.
	std::string basePrice;

	// The type of units of measurement for this type of item.
	std::optional<TradeUnit> unitType;

	// Specific gravity of the item's material against water at sea level, so 1.0
	// is 1000 kilograms per cubic metre.
	double density{0.0};

	// Packing factor of the item. The fraction of a storage cube the product
	// fills, counting packaging, dunnage and the voids between the pieces, so the
	// mass in a cubic metre is density x 1000 x packFactor kilograms.
	double packFactor{0.0};
};

using Items = std::vector<ItemNode>;

struct SkillNode
{
	// Identifier of the node: the skillId a game-mode file names to select the node,
	// the skillId of the age whose gate a Tech node is, and the nodeId a
	// prerequisite names.
	std::string skillId;

	// Display name of the skill; empty means the skillId.
	std::string displayName;

	// Description of the skill.
	std::string description;

	// Type of the node (Tech or Skill).
	NodeType nodeType{NodeType::Skill};

	// Maximum number of tiers this skill can have. Must be at least 1.
	int maxTiers{1};

	// List of skills required to unlock this skill: the YAML key "requires".
	std::vector<Prerequisite> requiredSkills;

	// Amount of resources required to unlock this skill.
	int resourceCostPerTier{1};

	// ID of the effect that this skill targets.
	std::string targetEffectID;

	// Mutual exclusion group ID.
	// If empty, this node has no rival choices.
	// If populated, only ONE node sharing this group ID can be unlocked.
	std::string exclusiveGroupId;

	// Layout metadata for UI Rendering.
	float gridX{0.0f};
	float gridY{0.0f};
};

using Skills = std::vector<SkillNode>;

struct GameModeNode
{
	Items items;
	Skills skills;
};
} // namespace Manifest::Core
