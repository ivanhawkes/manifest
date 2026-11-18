#pragma once

#include <entt/entt.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Migration.h>

#include <manifest/config/SimulationConfig.h>

namespace Jettison::Sim
{
class DefinitionRegistry;
}

namespace Manifest::Content
{
// The definitions Manifest loads through Jettison's DefinitionRegistry, one versioned file each,
// the schema settled in docs/stage-2-content-plan.md ("Schema contract"). Ages, categories, items
// and skills are the configuration records themselves (manifest/config/SimulationConfig.h); the
// types below are those only content has. Each field is read under its authored YAML key, so a key
// is the one a file and a patch name.

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
	// Named by the type's reflected name, and at its current version. A type that has never
	// changed has no steps.
	Jettison::Serialise::YamlMigrations migrations;
	std::string_view directory;
	entt::meta_type type;
	// The record's authored ID, or null for a type named by its file alone.
	std::string_view (*id)(const entt::meta_any &definition);
};

// Registers the definition types with Reflect, once however often it is called; every other call here
// calls it first.
void RegisterTypes();

std::span<const DefinitionType> DefinitionTypes();

// The record name of an authored ID: lower case, with a hyphen before a capital that follows a
// lower-case letter or a digit, and before the last capital of a run followed by a lower-case
// letter, so ItemCopper is item-copper and ItemLPG is item-lpg. data/build.sh names the files by
// the same rule.
std::string RegistryName(std::string_view id);

// Loads every type from its directory through its migrations, each record's ID checked against
// its name, so a record under another name is refused rather than found by its filename. Throws
// Jettison::Serialise::FormatError naming the file, and leaves the registry partly loaded, to be
// discarded.
void LoadDefinitions(
    Jettison::Sim::DefinitionRegistry &registry,
    std::span<const DefinitionType> types = DefinitionTypes());
} // namespace Manifest::Content
