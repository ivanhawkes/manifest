#pragma once

#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/vfs/Mount.h>

#include <manifest/config/SimulationConfig.h>
#include <manifest/content/ContentIdentity.h>
#include <manifest/content/Definitions.h>
#include <manifest/effects/ShipEffectEngine.h>
#include <manifest/universe/Universe.h>

namespace Jettison::Vfs
{
class FileSystem;
}

namespace Jettison::Sim
{
class DefinitionRegistry;
}

namespace Manifest::Content
{
// A rule the loaded definitions break, naming the record's virtual path, its type and ID, the field
// and the mounts that gave the record, bottom first.
class ContentError : public std::runtime_error
{
  public:
	using std::runtime_error::runtime_error;
};

// What a game runs on: every definition loaded and checked, resolved for one game mode, and the
// identity of the whole set. It holds no reference to the files or the registry it was read from.
struct ContentSet
{
	// In the mode's order.
	Core::Ages ages;
	// Every category, in registry order: their order has no meaning to the game.
	Core::Categories categories;
	// The mode's items and skills, in its order.
	Core::GameModeNode gameMode;
	Core::UniverseNamePools names;
	// Every hull, in registry order.
	std::vector<Core::ShipClassSpec> hulls;
	// The balance record, tuning/balance.
	TuningDefinition tuning;
	// Every scenario by its name, checked against its own game mode, which need not be this set's.
	std::map<std::string, ScenarioDefinition, std::less<>> scenarios;
	// The scenario selected, by its name; empty for a set selected by game mode alone.
	std::string scenario;
	// Over every definition loaded, the mode's or not, and the scenario selected.
	ContentIdentity identity;
};

// Loads every definition type into the registry, which reads through the file system, holds the
// whole set to the schema's rules (docs/stage-2-content-plan.md, "Fields, defaults and null"),
// every game mode included, and builds the content set for the mode, a game-mode record's name.
// The defaults one field takes from another (an empty skill display name is its ID) are filled in
// the set, not the registry. Throws Jettison::Serialise::FormatError for a record that does not
// read, and ContentError for a broken rule; the registry is then to be discarded.
ContentSet LoadContent(
    const Jettison::Vfs::FileSystem &files, Jettison::Sim::DefinitionRegistry &registry,
    std::string_view gameMode);

// The same for a scenario, a scenario record's name: the set is for its game mode, and its name is
// in the set's identity. What a host and the validator run.
ContentSet LoadScenario(
    const Jettison::Vfs::FileSystem &files, Jettison::Sim::DefinitionRegistry &registry,
    std::string_view scenario);

// The same as LoadContent, over the sources mounted read-only in order, a later one over an earlier, in
// a file system and a registry of its own that are gone when it returns: a failure anywhere leaves
// nothing behind, so a caller's working content is kept by keeping its own set.
ContentSet LoadContent(std::span<const Jettison::Vfs::Mount> sources, std::string_view gameMode);
} // namespace Manifest::Content
