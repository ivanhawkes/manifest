#pragma once

#include <entt/entt.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Archive.h>

#include <manifest/config/SimulationConfig.h>

namespace Jettison::Sim
{
class DefinitionRegistry;
}

namespace Manifest::Content
{
// The definitions Manifest loads through Jettison's DefinitionRegistry, one versioned file each,
// the schema settled in docs/stage-2-content-plan.md ("Schema contract"). Each field is read under
// its authored YAML key, so a key below is the one a file and a patch name. A field a file leaves
// out keeps its initialiser here; which fields are required, and every rule between records, is
// semantic validation over the loaded set, not the reader's.

// Registered by the names the authored files use.
enum class TradeUnit
{
	Gram,
	Kilogram,
	Litre,
	Kilolitre,
	Unit,
	Sheet,
	SquareMetre
};

struct AgeDefinition
{
	std::string ageId;
	std::string displayName;
	std::string description;
};

struct CategoryDefinition
{
	std::string categoryId;
	std::string displayName;
	// Empty means the categoryId.
	std::string type;
	std::string description;
};

struct ItemDefinition
{
	std::string itemId;
	std::string categoryId;
	std::string ageId;
	std::string displayName;
	std::string description;
	// The physical fields are what the field notes in data/trade-items/ define.
	double unitsPerCubicMetre{0.0};
	// Decimal text as authored, parsed by Types::Decimal::TryParse after loading: a float in
	// between would round the microcredits.
	std::string basePrice;
	std::optional<TradeUnit> unitType;
	double density{0.0};
	double packFactor{0.0};
};

struct SkillDefinition
{
	std::string skillId;
	// Empty means the skillId.
	std::string displayName;
	std::string description;
	Core::NodeType nodeType{Core::NodeType::Skill};
	// YAML key "requires".
	std::vector<Core::Prerequisite> requiredSkills;
	int maxTiers{1};
	int resourceCostPerTier{1};
	// Empty is none, in both.
	std::string targetEffectID;
	std::string exclusiveGroupId;
	float gridX{0.0f};
	float gridY{0.0f};
};

struct NamePoolDefinition
{
	std::string pool;
	// In draw order.
	std::vector<std::string> names;
};

// Selections are mappings, not bare IDs, so that a patch can merge into them by ID
// (!merge=itemId).
struct AgeSelection
{
	std::string ageId;
};

struct ItemSelection
{
	std::string itemId;
};

struct SkillSelection
{
	std::string skillId;
};

// A game mode has no ID field: it is named by its file. Each list is in the order the game uses,
// which the registry's sorted names would not give.
struct GameModeDefinition
{
	std::vector<AgeSelection> ages;
	std::vector<ItemSelection> items;
	std::vector<SkillSelection> skills;
};

// A type the registry loads, in the order the content manifest lists the types.
struct DefinitionType
{
	// The reflected name, which a migration chain is named by too.
	std::string_view name;
	std::string_view directory;
	Jettison::Serialise::FormatVersion version;
	entt::meta_type type;
	// The record's authored ID, or null for a type named by its file alone.
	std::string_view (*id)(const entt::meta_any &definition);
};

// Registers the types above with Reflect, once however often it is called; every other call here
// calls it first.
void RegisterTypes();

std::span<const DefinitionType> DefinitionTypes();

// The record name of an authored ID: lower case, with a hyphen before a capital that follows a
// lower-case letter or a digit, and before the last capital of a run followed by a lower-case
// letter, so ItemCopper is item-copper and ItemLPG is item-lpg. data/build.sh names the files by
// the same rule.
std::string RegistryName(std::string_view id);

// Loads every type from its directory, each record's ID checked against its name, so a record
// under another name is refused rather than found by its filename. Throws
// Jettison::Serialise::FormatError naming the file, and leaves the registry partly loaded, to be
// discarded.
void LoadDefinitions(Jettison::Sim::DefinitionRegistry &registry);
} // namespace Manifest::Content
