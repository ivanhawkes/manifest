#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/serialise/Migration.h>
#include <jettison/space/Cruise.h>

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

// A cruise limited by acceleration (D-090): the profile of a hull that carries people, its occupants
// given or left out, and of one whose occupants are 0.
struct CruiseModel
{
	Jettison::Space::CruiseProfile manned;
	Jettison::Space::CruiseProfile unmanned;
};

// A game mode has no ID field: it is named by its file. Each list is in the order the game uses,
// which the registry's sorted names would not give. A mode with no cruise keeps each hull's drive as
// authored, the time-compressed power law (D-088); one with a cruise gives each drive its profile.
struct GameModeDefinition
{
	std::vector<AgeSelection> ages;
	std::vector<ItemSelection> items;
	std::vector<SkillSelection> skills;
	std::optional<CruiseModel> cruise;
};

struct ItemQuantity
{
	std::string itemId;
	// In the item's unit.
	double quantity{0.0};
};

// The balance inputs (data/tuning/balance.yaml), named by its file: values no rule reads yet, held to
// their ranges (docs/stage-2-content-plan.md, "Tuning").
struct EconomyTuning
{
	double intervalSeconds{0.0};
	double wageTaxRate{0.0};
};

struct NeedDefinition
{
	std::string needId;
	double weight{0.0};
	double perThousandPerDay{0.0};
	std::optional<Core::TradeUnit> unit;
	// Each in the need's unit; none while the need's item is open.
	std::vector<ItemSelection> items;
};

struct ConditionTuning
{
	double initial{0.0};
	double healthLossPerHour{0.0};
	double happinessLossPerHour{0.0};
	double healthRecoveryPerHour{0.0};
	double happinessRecoveryPerHour{0.0};
	double workRateFloor{0.0};
};

struct LifeSupportTuning
{
	double oxygenKilogramsPerOccupantDay{0.0};
	double cartridgeOccupantDays{0.0};
};

// A player's trading company as provisioning creates it.
struct CompanyTuning
{
	// Whole credits, in its account.
	std::int64_t startingCredits{0};
};

// The hosted order book (D-083, D-085).
struct MarketTuning
{
	// What a venue takes from each side of a fill, in basis points of its gross, fixed on an order
	// as it is placed.
	int listingFeeBps{0};
	// A resting order's life, in simulation seconds, a whole number of fixed steps; 0 is none.
	double orderLifeSeconds{0.0};
};

// Each venue's broker (D-084): what its account starts with, and how far apart it quotes.
struct BrokerTuning
{
	// Whole credits.
	std::int64_t treasury{0};
	// Between its bid and its ask, in basis points of the native price, half on each side.
	int spreadBps{0};
};

struct ConcessionTuning
{
	double termHours{0.0};
};

struct ProcurementTuning
{
	double performanceSecurityRate{0.0};
};

struct ProjectDefinition
{
	std::string projectId;
	std::vector<SkillSelection> requiredSkills;
	std::vector<ItemQuantity> inputs;
	std::string unlocksRecipe;
};

struct RecipeDefinition
{
	std::string recipeId;
	std::vector<ItemQuantity> inputs;
	std::vector<ItemQuantity> outputs;
};

struct TuningDefinition
{
	EconomyTuning economy;
	std::vector<NeedDefinition> needs;
	ConditionTuning condition;
	LifeSupportTuning lifeSupport;
	CompanyTuning company;
	MarketTuning market;
	BrokerTuning brokers;
	ConcessionTuning concessions;
	ProcurementTuning procurement;
	std::vector<ProjectDefinition> projects;
	std::vector<RecipeDefinition> recipes;
};

// A scenario: the world a game starts from, named by its file (docs/stage-2-content-plan.md,
// "Scenario and frames"). Every list is keyed by its own ID, so a patch merges by it
// (!merge=regionId); an ID is unique within its list and, being save identity, stable. Positions
// are metres in the system's frame, quantities in the item's unit, rates per simulation day.
enum class OrganisationKind
{
	Polity,
	Corporation
};

enum class FacilityKind
{
	Workshop
};

// A star system: a Jettison frame, Sim::FrameId(systemId), its bodies in the Celestial.StarSystem of
// that name (definitions/systems).
struct SystemDefinition
{
	std::string systemId;
	std::string displayName;
};

// A physical body, with no population or account of its own: its regions hold those. Its system's
// body of that name says where it is.
struct PlanetDefinition
{
	std::string planetId;
	std::string systemId;
	std::string displayName;
};

// Cultural and technology identity only.
struct CivilisationDefinition
{
	std::string civilisationId;
	std::string displayName;
	std::string ageId;
};

struct OrganisationDefinition
{
	std::string organisationId;
	std::optional<OrganisationKind> kind;
	std::string displayName;
};

// Seeded explicitly, so that using one never needs the technology it serves.
struct FacilityDefinition
{
	std::string facilityId;
	std::optional<FacilityKind> kind;
	bool operational{false};
};

struct ProjectSelection
{
	std::string projectId;
};

// The one owner of a population and its accounts.
struct RegionDefinition
{
	std::string regionId;
	std::string planetId;
	std::string civilisationId;
	// An organisation; empty only for a player's home, which no one governs until it is allocated.
	std::string governorId;
	bool playerHome{false};
	double population{0.0};
	// Whole credits.
	std::int64_t householdFunds{0};
	std::int64_t publicFunds{0};
	double health{0.0};
	double happiness{0.0};
	// What the region knows.
	std::vector<SkillSelection> skills;
	std::vector<FacilityDefinition> facilities;
	// Its public projects, each a tuning project.
	std::vector<ProjectSelection> projects;
};

struct RegionSelection
{
	std::string regionId;
};

struct MarketEntry
{
	std::string itemId;
	double production{0.0};
	double demand{0.0};
	// Totes (D-087), at most kMaxWarehouseTotes: its broker's at the start.
	std::uint64_t stock{0};
};

// A station or port, delivering physically to the regions it serves: its system's Station body of
// that name, orbiting its planet when it names one.
struct VenueDefinition
{
	std::string venueId;
	std::string systemId;
	// The planet it serves from; empty for one in open space.
	std::string planetId;
	std::string operatorId;
	// Open to every company on equal terms, as a starting region's station must be.
	bool isPublic{false};
	bool operational{false};
	// The Dock.StationLayout it is built to, by record name: its pads are the hulls it docks.
	std::string layout;
	// Ships it can keep connected to working life support at once.
	int lifeSupportCapacity{0};
	std::vector<RegionSelection> regions;
	std::vector<MarketEntry> market;
};

struct ScenarioDefinition
{
	std::string gameMode;
	// The hull a new player starts in: a player's home needs a station that docks it.
	std::string starterHull;
	// A subset of the mode's items, in its order.
	std::vector<ItemSelection> items;
	std::vector<SystemDefinition> systems;
	std::vector<PlanetDefinition> planets;
	std::vector<CivilisationDefinition> civilisations;
	std::vector<OrganisationDefinition> organisations;
	std::vector<RegionDefinition> regions;
	std::vector<VenueDefinition> venues;
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
