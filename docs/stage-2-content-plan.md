# Stage 2: Jettison definitions and validated scenario content

Planning handoff for Claude, prepared 2026-11-15. Depends on the completed
[stage 1 host](stage-1-host-plan.md) and refines stage 2 of
[development stages](development-stages.md). No implementation or validation runs
are claimed. Inspection used Manifest `feature-base` at `2716380` and Jettison
`feature-base` at `826c4bec125b6631a6ba8ee822c90463316c3511`; recheck after stage 1 lands.

Before freezing schemas, apply the [regional correction](regional-economy-and-governance-plan.md).
Define separate physical planet, economic region, government/company/bloc relationships
and station/venue references. Region definitions and generation allocate population,
household/public funds and local project/facility scope without duplicating planet totals.
Keep the small trading fixture; the first governance fixture must demonstrate multiple
independently governed economies on one planet, with player home regions and AI neighbours.
Validate links and effective directory/pack equivalence through the same engine pipeline.

## Required direction

The user explicitly requires Manifest to switch to Jettison's YAML loading mechanisms.
Use `Vfs::FileSystem`, reflected definitions, `Serialise::YamlArchive`,
`Serialise::YamlMigrations` and `Sim::DefinitionRegistry` directly. Manifest owns
its schemas and gameplay validation, not a second YAML reader, archive or patch engine.

Retire `YamlFieldReaders.h` and the custom marshalling code when callers migrate.
Any retained host-path development convenience must mount content and call the same
engine-backed pipeline. Do not retain a separate legacy YAML interpretation behind it.
Versioned migration callbacks may manipulate YAML nodes through the engine migration
API; that does not justify a parallel deserialiser.

## Current boundaries and affected modules

| Area | Current behaviour | Planned responsibility |
| --- | --- | --- |
| `config/SimulationConfig.*` | Three host-file loaders, custom required/default field handling; numeric item properties retained as strings. | Reflected definition types, explicit defaults, engine registry loads and config-level semantic checks. |
| `config/YamlFieldReaders.h` | Bespoke null, scalar and numeric readers. | Remove after replacing its callers with engine deserialisation. |
| `universe/Universe.*` | Host-file YAML mapping of pool names to lists; ordered runtime pools. | Reflected name-pool definitions and conversion to the existing ordered runtime representation. |
| `progression/ProgressionManager.*` | Duplicate, prerequisite, tier and cycle validation. | Reuse these rules on candidate definitions without publishing runtime state. |
| `effects/EffectCatalogue.*` | Native effect definitions and skill-to-effect coverage. | Reuse catalogue validity and coverage checks; effects remain native in this stage. |
| `sim/Simulation.cpp` | Validates Decimal base prices and multiplier headroom during generation. | Share this validation with content loading; do not generate a world merely to validate prices. |
| New content module | No aggregate content transaction today. | Load, validate and publish one immutable content set; expose the same entry point to host and validator. |
| `tools/tui/ManifestConsole.cpp` | Assembles host paths for four loaders and replaces development state. | Use the shared content loader while preserving failure rollback and development commands. |
| `data/build.sh`, root CMake, `mods/manifest/` | yq merges authored groups into aggregate files; package contains six files. | Produce/stage versioned engine-readable definitions, retain full content and validate staged and packed output. |
| Config/universe/progression/data/mod/console tests | Several callers depend on old loader signatures and aggregate fixtures. | Migrate meaningful coverage to engine loading and the common validation contract. |

Place aggregate content validation above config, universe, progression and effects in
the module graph. Those lower modules must not depend back on the content module or
host. Keep the validator independent of TUI and runtime world construction.

## Schema and packaging proposal

Finalised by the [schema contract](#schema-contract-c01-2026-11-18).

Jettison's registry expects one versioned reflected object per file and derives its
registry name from the relative filename. Adopt that format for hosted definitions:

- Generate records under `manifest/definitions/ages`, `categories`, `items`, `skills`,
  `name-pools` and `game-modes`; author scenarios under `manifest/scenarios`.
- Retain existing authoring groups as source inputs if useful. The packaging transform
  may split them into engine records, but must preserve authored IDs, field names,
  values and ordered selections. It must not implement runtime validation or layering.
- Use kebab-case generated filenames and retain original case-sensitive IDs inside the
  records. Validate uniqueness of both path and authored ID; reject filename collisions.
  References resolve by authored ID, not a silent conversion to a filename-derived ID.
- Keep `requires` bound to the prerequisite vector, and `targetEffectID` spelled exactly
  as authored. Register `NodeType` with its existing `Tech` and `Skill` names.
- Each name-pool record carries its existing pool name and ordered list of names.
  Registry filename ordering must not change the pool iteration or name draw order.
- Game-mode definitions retain ordered item/skill references. The Manifest and Astralis modes
  remain available; order of ages, selected items, skills and names is preserved explicitly
  instead of inferred from the registry's sorted names.

Generated record paths are a new package interface. Document their mapping from source
files. Preserve existing namespaced aggregate paths as compatibility artifacts where
needed, but hosted loads have one authoritative source: the versioned definitions.
Remove obsolete artifacts only with explicit caller/test migration; do not load both
representations and allow them to disagree. Runtime patch authors target the new
definition paths. Verify generated records reproduce the existing selected values.

Jettison's reader rejects unknown fields and uses default member initialisers for
missing fields. Set defaults explicitly before reflection: for example `nodeType=Skill`,
`resourceCostPerTier=1`, `maxTiers=1` and zero UI coordinates. Keep required strings
empty by default and reject them semantically when absent. Apply existing dependent
defaults, such as display name falling back to identity, in one normalisation step.

Legacy authoring uses null for optional fields. Jettison's scalar reader rejects null;
the packaging conversion must omit optional null fields to express their defaults.
Required null fields remain errors. A runtime patch's null retains the engine meaning:
remove the field so it takes its default. Do not globally relax Jettison's reader.

Price text must reach `Decimal::TryParse` without a floating-point round trip. Keep
`basePrice` as a reflected string in authored definitions and derive Decimal prices
after loading. Quote it in generated YAML, and verify exact microcredit values survive
the packaging transform. Continuous properties remain floats/doubles at runtime and
existing ledgers stay whole-credit integers. Add no Decimal archive codec solely for
this authoring path.

## Schema contract (C01, 2026-11-18)

Settled against Manifest `aa56443` and SDK pin `e53681e` by reading the authored data, the
loaders and the SDK's `DefinitionRegistry`, `YamlArchive`, `Reflect`, `FileSystem`, `InFrame` and
`SaveInfo`. It finalises the proposal above for C02; where they differ, this section governs. No
code, data or gate changed. **No Jettison gap is demonstrated:** each need below maps to a public
SDK call, so the engine's content-pipeline row has no work until C02 or C03 finds one.

### Records

One versioned file per record, every format at version 1. A registry name is the authored ID in
kebab case: lower-cased, with a hyphen at each lower-to-upper or digit-to-upper boundary and before
the last capital of a capital run followed by a lower-case letter (`ItemCopper` → `item-copper`,
`ItemLPG` → `item-lpg`). The loader checks every record's ID against its name, so a mod's file at
the wrong path is refused rather than resolved by filename; packaging rejects two IDs with one name.
References resolve by authored ID through an index built at load.

| Type | Directory under `manifest/` | Source | Count today |
| --- | --- | --- | --- |
| `Manifest.Age` | `definitions/ages` | `base/ages.yaml` | 14 |
| `Manifest.Category` | `definitions/categories` | `trade-items/categories.yaml` | 42 |
| `Manifest.Item` | `definitions/items` | `trade-items/*.yaml`, `item-base` | 152 |
| `Manifest.Skill` | `definitions/skills` | `skill-tree/*.yaml`, `skill-base` (age gates included) | 85 |
| `Manifest.NamePool` | `definitions/name-pools` | `universe/solar-system-names.yaml`, one per key | 2 |
| `Manifest.Hull` | `definitions/hulls` | `ships/hulls.yaml`, `hull-base` (C04) | 3 |
| `Dock.PadClassDefinition` | `definitions/pads` | `docks/pads/*.yaml`, authored as records (C09) | 5 |
| `Dock.HullDefinition` | `definitions/dock-hulls` | `docks/hulls/*.yaml`, each named by its `Manifest.Hull` (C09) | 3 |
| `Dock.StationLayout` | `definitions/stations` | `docks/stations/*.yaml`, authored as records (C09) | 1 |
| `Manifest.GameMode` | `definitions/game-modes` | `game-mode/manifest.yaml`, `astralis.yaml` | 2 |
| `Manifest.Scenario` | `scenarios` | `scenarios/*.yaml`, authored as records (C05) | 2 |

**Built in C02 (2026-11-18).** `Manifest::Content` (`source/game/content`) registers the six types
(`Definitions.h`) and loads them with the name check (`LoadDefinitions`); `data/build.sh` writes
the records to `data/definitions/`, staged at `manifest/definitions/`. A mode's `ages` list is
written from `base/ages.yaml`'s order, so it is authored once. Packaging refuses a null outside
the optional fields and two IDs with one name (the `packaging` suite); the `content` suite holds
every record to the legacy loaders' values, both modes' order and exact price text.

### Fields, defaults and null

The reader starts from default member initialisers and rejects null in a whole file, so every
default below is an initialiser, and every "required" is a semantic check on the default. A legacy
null is omitted by packaging only in the fields marked optional; a null elsewhere fails packaging
with file, ID and field. A patch's null keeps the engine meaning (the field takes its default).
Rules the current loader applies by raising a value silently to its floor become rejections; no
authored value is below one.

| Type | Field (YAML key) | C++ type, default | Rule |
| --- | --- | --- | --- |
| Age | `ageId`, `displayName` | string, empty | required |
| Age | `description` | string, empty | optional |
| Category | `categoryId`, `displayName` | string, empty | required |
| Category | `type` | string, empty | empty takes `categoryId` (existing normalisation) |
| Category | `description` | string, empty | optional |
| Item | `itemId`, `displayName`, `categoryId`, `ageId` | string, empty | required; links resolve |
| Item | `description` | string, empty | optional |
| Item | `unitType` | `std::optional<TradeUnit>`, empty | required. Enumerators are the authored names: `Carat`, `TroyOunce`, `Kilogram`, `Tonne`, `Litre`, `Kilolitre`, `Unit`, `Sheet`, `SquareMetre` ([D-064](decision-ledger.md#d-064)) |
| Item | `unitsPerCubicMetre`, `density` | double, 0 | required, finite, above 0 |
| Item | `packFactor` | double, 0 | required, finite, in (0, 1] |
| Item | `basePrice` | string, empty | required, quoted; `Decimal::TryParse`, nonnegative, with the multiplier headroom |
| Skill | `skillId` | string, empty | required |
| Skill | `displayName` | string, empty | empty takes `skillId` (existing normalisation) |
| Skill | `description` | string, empty | optional |
| Skill | `nodeType` | `NodeType`, `Skill` | `Tech` or `Skill`; a Tech node's ID names an age |
| Skill | `maxTiers`, `resourceCostPerTier` | int, 1 | at least 1 |
| Skill | `requires` → `requiredSkills` | vector of `{nodeId, requiredTier}`, empty | `nodeId` resolves; `requiredTier` default 1, from 1 to the target's `maxTiers`; no cycle |
| Skill | `targetEffectID`, `exclusiveGroupId` | string, empty | optional, empty is none (31 and 82 legacy nulls); an effect resolves |
| Skill | `gridX`, `gridY` | float, 0 | finite |
| NamePool | `pool` | string, empty | required, equals the record's name |
| NamePool | `names` | vector of string, empty | at least one, none empty; order kept |
| GameMode | `ages`, `items`, `skills` | vectors of `{ageId}`, `{itemId}`, `{skillId}` | each resolves, none twice; order kept |
| Hull | `hullId`, `className` | string, empty | required |
| Hull | `description` | string, empty | optional |
| Hull | `baseStats` | `std::map<EffectStat, double>`, a mapping by stat name | at least one; each finite, not below 0; a stat left out is none |
| Hull | `fitableEffects` | vector of string, empty | each names an effect; empty is no upgrade |
| Hull | `occupants`, `scrubberCartridges` | `std::optional<int>`, empty | at least 1; empty is unselected |
| Hull | `oxygenTanks` | `std::optional<std::vector<double>>`, kilograms a tank | at least one, each above 0; empty is unselected |

Every shipped item carries its physical fields today, so none is optional: test fixtures that omit
them gain them. Selections are mappings rather than bare strings so that a patch can use
`!merge=itemId`. Age order is the mode's `ages` list (full and simple both list all 14), not the
registry's sorted names. Category order has no gameplay meaning and takes registry order; the
development generator draws specialities over it, so its pinned replay hashes move once in C03,
shown to be the same set in another order. Name pools are already held sorted by name.

### Units and time

Simulation time is seconds of the fixed step. Rates are authored per simulation day (86,400 s), and
population-scaled rates per 1,000 people per simulation day ([D-022](decision-ledger.md#d-022)).
A good's quantity is a double in its `unitType`; its packed volume and mass follow
`unitsPerCubicMetre`, `density` and `packFactor` as the field notes in `data/trade-items/` define
them, never the trading unit alone. Since [D-078](decision-ledger.md#d-078) and
[D-083](decision-ledger.md#d-083), goods held, moved or traded are counted in totes, an eighth of a cubic metre each
([D-087](decision-ledger.md#d-087)), so a venue's market `stock` and a hull's `CargoVolume` are whole totes (W03); rates stay doubles
in the unit, per day. Prices are
Decimal text, ledgers whole credits (int64).
Positions are metres, `glm::dvec3`, in a system's frame.

### Scenario and frames

A scenario is one `Manifest.Scenario` record whose worlds are sequences of mappings keyed by their
own IDs, so a patch merges with `!merge=<id field>`. IDs are unique within each list and, being
save identity, stable.

| List | Fields | Rules |
| --- | --- | --- |
| (root) | `gameMode`, `items: [{itemId}]` | the mode resolves; items are a subset of its items, order kept |
| `systems` | `systemId`, `displayName` | each a Jettison frame, `Sim::FrameId(systemId)`; two IDs with one frame ID are refused |
| `planets` | `planetId`, `systemId`, `displayName`, `position` | a physical body; no population or account of its own |
| `civilisations` | `civilisationId`, `displayName`, `ageId` | cultural and technology identity only |
| `organisations` | `organisationId`, `kind` (`Polity` or `Corporation`), `displayName` | owns accounts and assets from Stage 3 |
| `regions` | `regionId`, `planetId`, `civilisationId`, `governorId`, `playerHome`, `population`, `householdFunds`, `publicFunds`, `health`, `happiness` | the one population owner; `governorId` names an organisation, empty only when `playerHome`; population finite and nonnegative; funds whole credits; condition 0–100 |
| `venues` | `venueId`, `systemId`, `planetId` (optional), `position`, `operatorId`, `regions: [{regionId}]`, `market: [{itemId, production, demand, stock}]` | a named planet is in the venue's system; served regions are on that planet ([shared ports](regional-economy-and-governance-plan.md#control-construction-and-discovery-boundaries)), so a venue with no planet serves none; market items are scenario items; quantities finite and nonnegative, rates per simulation day |

The eight-good trading fixture is then one system, two planets, two regions (one civilisation each)
and two venues each serving one region; the governance fixture puts several regions on one planet.
Trading blocs are left out until a rule or fixture needs one, since their shared powers are
unselected.

**Built in C05 (2026-11-19).** `ScenarioDefinition` (`Definitions.h`) is the table above with what
C05's starting-access and project checks need: the root names a `starterHull`; a venue is `public`
and `operational` or not, names the station `layout` it docks on (a `hulls` list until
[C09](small-trader-and-docking-plan.md#shared-ship-and-station-definitions-c09-done-2026-11-19)) and has a `lifeSupportCapacity`, ships it can
keep on working life support; a region lists the `skills` it knows, its seeded `facilities`
(`{facilityId, kind: Workshop, operational}`) and its public `projects`, each a tuning project.
`LoadContent` checks every scenario against its own mode, whichever the set is built for, and the
set holds them all by name. Beyond the table's rules: a player's home is served by an operational
public venue that docks the starter hull with life-support capacity of at least 1 (D-047); a
region's project needs its prerequisites known, an operational workshop, and every project and
recipe input produced by some venue of the scenario. Two fixtures, staged as they are authored at
`manifest/scenarios/`, their values recorded with them ([ledger](decision-ledger.md#numeric-values)):

- `trading`: the eight goods in system Anvil, Ore and Brine each a region of its own civilisation
  with a public highport, each good made at one and wanted at the other. Ore's region is a
  player's home, Brine's governed by its council.
- `governance`: four regions on Forge, two players' homes and two AI neighbours, all served by one
  public highport; Cassiter, an Industrial region across the system, sells tin, basic medical
  supplies and the life-support goods from a corporation's private dock. Forge East knows smelting,
  has the seeded workshop and works the Bronze project on local copper and Cassiter's tin.

**A01's seams fit, unchanged.** Manifest's frame partition is the star system: planets, venues and
ships are placed in its frame, an entity in stargate transit is in `kNoFrame`, and a region is
jurisdiction located by its planet, not a frame. `SaveInfo::details` carries `scenario`, `content`
(the fingerprint), `content-format` and `game-version`; the canonical content manifest itself, which
a load compares exactly, goes in a game `SaveSection`, since a detail is a string a list shows.
Diagnostics list contributing layers with `FileSystem::ReadLayers`, so per-field provenance needs no
new engine API yet.

### Item authoring (C04, 2026-11-19)

The user selected the item values ([D-059](decision-ledger.md#d-059)); the numbers are the data's,
in `data/trade-items/`. The four food grains are `CategoryConsumables`, of the Stone Age with their
seeds ([D-064](decision-ledger.md#d-064)), and keep their seed's kernel density and packing; rice is milled white rice. Basic Medical
Supplies is `CategoryMedicalEquipment`, so the specialised medicines stay alone in
`CategoryMedicine`. LPG is `CategoryFuels`. Oxygen, as the refrigerated liquid, and the scrubber
cartridge are the new `CategoryLifeSupport`; one cartridge is one `Unit`, a fresh 30-occupant-day
charge of 41 kg, 16 to the cubic metre, two to the tote. The medical supplies, LPG and both life-support goods are of
the Industrial Age, which bounds production, not trade: an earlier region imports them. All eight
are selected in the Manifest mode; the Astralis mode and the eight-good fixture are unchanged.

The hulls are `Manifest.Hull` definitions (`data/ships/hulls.yaml`), read as
`Core::ShipClassSpec`, so a hull's `baseStats` is a mapping by stat name, merged by a patch stat by
stat: the map fields Jettison provisioned for C04. The long-haul trader moved there from code
unchanged. The lone-trader starter has its 20 m³ hold, one occupant, three 10 kg oxygen tanks and
one cartridge (D-045, D-050); the compact ship its 4 m³ and one occupant (D-044). Every other stat,
their fittings and the compact ship's life support are left unset, and the starter's provisioning
(every tank full, a fresh cartridge) is A04's.

### Tuning (T4, C04, 2026-11-19)

The balance inputs are one `Manifest.Tuning` record, `manifest/definitions/tuning/balance.yaml`
(source `data/tuning/balance.yaml`), named by its file and required: the economic cadence and
wage tax, the household needs, condition rates, a new company's starting credits (A04), onboard
life-support consumption, the concession
term, the performance-security rate, and the bronze project and recipe. The keys are indexed in
the [ledger](decision-ledger.md#numeric-values). No rule reads them yet; the content suite holds
each to its range and its references (a need's items in the need's unit, a project's skills,
inputs and recipe), and the data suite to the plans' worked examples. A need, project or recipe
patches by its ID (`!merge=needId`). Drinking water's need is `ItemDrinkingWater`, in kilolitres
([D-060](decision-ledger.md#d-060)).

### Authoring decisions still open

| Decision | Owner, before |
| --- | --- |
| Trading-bloc schema | when a rule needs it |

## Versions, layering and migration

Start each new hosted format at version 1. Keep package metadata version, individual
definition versions and future save versions distinct. Unversioned authoring inputs
are converted during packaging; they are not implicitly accepted as runtime version 1.
Jettison's migration API does not support a version-zero migration, so do not promise one.

Use the engine's semantics unchanged: later mounts win whole-file replacement;
patches apply in mount order above the winning whole file; an orphan patch fails;
mapping fields merge; untagged sequences replace; `!append` appends; `!merge=<field>`
merges by identity; null removes a field. Never reproduce these operations in Manifest.

Register a migration chain for each evolving reflected format. Future steps transform
only fields present in a document, because patches migrate independently and must not
acquire defaults that overwrite the lower layer. Unsupported versions fail with path,
format and version context. Test migration wiring using a dedicated test format with
two versions; do not invent a production schema revision merely to exercise migration.

Expose explicit ordered overlay sources for host and validator, using the same option
contract. Preserve stage 1's mandatory base-package validation; do not add automatic
directory discovery. Identify mounts by stable logical IDs and report that order.

**Built in C06 (2026-11-19)**, as [D-070](decision-ledger.md#d-070) selects. `Manifest::Host::Options`
(`Start.h`) is the contract, read from a command line by `ParseOptions` for both executables:
`--content <directory-or-pack> [--overlay <directory-or-pack>]... --scenario <name>`, each overlay
mounted over the package in the order given. The package mounts as `manifest`, an overlay under
the name its `mod.yaml` gives, which a later overlay's `requires` names; an overlay with no
`mod.yaml` or no name, or a name already mounted, fails in the mount phase. `manifest --check-startup`
starts a session on the options (`Start`); `manifest-validate` loads the same sources with no world
(`Validate`): the same mounts, the engine's package rules (`Jettison::Host::DescribePackages`,
provisioned in Jettison `30ea3f3`), Manifest's package check and `Content::LoadScenario`, failing with
the same `StartupError` text, a rule's failure naming the layers read, overlays among them. On
success it logs each mount in order with its source, the scenario, its game mode and the content
fingerprint, and exits 0; content that would not start exits 1, a bad command line 2. The scenario
is required and names the game mode the set is resolved for.

## Atomic loading and validation

**Built in C03 (2026-11-18).** `Manifest::Content::LoadContent` (`Content.h`) is the one entry
point: it loads every type, holds the whole set to the rules of
[Fields, defaults and null](#fields-defaults-and-null) with every game mode, and returns an
immutable `ContentSet` (the mode's ages, items and skills in its order, every category, the name
pools and the content identity) that holds no reference to its files or registry. The overload
over ordered mounts builds a candidate file system and registry of its own and discards both, so
a failure anywhere, a late type included, leaves the caller's set as it was; the console keeps its
session through a failed `load`, and the host fails start-up in the content phase. A rule's
failure names the record's virtual path, its type and ID, the field and the layers the registry
read for it, the topmost whole file's mount and each patch at or above it (`ContentError`); a record that does not read is the registry's
`FormatError`, naming its file and mount. Rules are reused, not copied: the base price is
`Simulation::IsValidBasePrice`, which generation also calls, the whole skill graph and each
mode's tree are `ProgressionManager::Build`, and an effect is `EffectCatalogue::Default()`. Each type loads
through a migration chain (`DefinitionType::migrations`, no steps at version 1); the wiring is
tested with a two-version test format. The configuration records are the reflected types
themselves (`SimulationConfig.h`, the item's physical fields now typed), so content converts
nothing but the name pools. A category's empty description still takes its display name, as the
retired loader did. The legacy loaders, `YamlFieldReaders.h`, the name-pool reader and the
merged `core-config` documents are gone, with their last callers; the package holds `mod.yaml`
and the records. Host overlays are C06's, under [versions, layering and
migration](#versions-layering-and-migration).

Build a fresh candidate containing all registry types and their derived data. Load all
types, resolve references, normalise defaults, run semantic checks and calculate the
fingerprint before publishing a single immutable content-set owner. Jettison registry
loads are atomic per type only; successful early types must not leak into active content
when a later type fails. The candidate's VFS must outlive its registry. Retain active
content on failed development reload; running hosted worlds do not live-reload content.

Required semantic checks include duplicate/empty IDs, unresolved category/age/item/skill
references, progression validity, effect coverage, valid name pools, supported units,
finite physical values and sensible storage bounds, valid nonnegative Decimal prices
with the existing multiplier headroom, and scenario references/counts. Reuse existing
rule owners or extract shared validation; do not copy their logic into the CLI.
For shipped hosted content require the physical fields needed for cargo calculations;
do not silently fill malformed values with gameplay defaults. Preserve documented
optional fields for older development fixtures only where the common schema permits it.

Report virtual path, authored ID, field and contributing mounts. Jettison currently
exposes typed values and logs overrides, but no public per-field provenance query.
For semantic errors retain a path/ID source index and list contributing layers; do not
claim an exact winning line without evidence. An explain command may show effective
typed values plus ordered source layers. If richer provenance is necessary, add a
bounded reusable engine API rather than writing another patch interpreter.

## Initial scenario

Create a separate scenario; `astralis.yaml` currently selects fifteen goods, so it is not
the approved eight-good scenario. Proposed existing IDs: `ItemCopper`, `ItemTin`,
`ItemGold`, `ItemSalt`, `ItemLime`, `ItemFurs`, `ItemGlassware`, `ItemSoap`.
These are a proposed test selection, not a change to the Manifest or Astralis modes or base prices.

The scenario defines one system, two distinct civilisation identities and two distinct
venues, each linked to a civilisation and the system. Include explicit physical
locations and complementary production/demand/initial stock for the eight goods.
Validate unique IDs, finite values, nonnegative quantities and valid references.
Name the coordinate frame, units and production/demand time unit in its schema; do not
reuse the development generator's civilisation layout as a physical transport model.
Use the existing `SkillShipCargoHolds` as the proposed later company upgrade reference,
with prerequisite closure available. Binding that skill to company ownership is stage 3.

Stage 2 validates scenario definitions only. Company creation, cargo transfers, travel,
settlement and advancement are stages 3–4. Scenario balance numbers and exact locations
are implementation choices to record with the fixture; no economic viability claim
is made until the command-driven loop demonstrates it.

## Staple-grain content follow-up

The user requires rice, wheat, oats and barley as four distinct tradable food commodities.
The [grain handoff](economy-and-development-plan.md#staple-grain-commodities-selected-implementation-handoff)
records proposed IDs, affected authoring/packaging/mode paths, unresolved physical/price
values and implementer checks. Existing seed commodities retain their IDs and values.
Use the current definition/registry/packaging path; no parallel loader or additional generic
SDK capability is demonstrated for these items. No item definitions have been added by this plan.

The governance basket has one staple-food need satisfied only by the four grains, plus
drinking water and basic medical supplies, with each need initially weighted 1. Preserve
the original eight-good trading fixture separately. The selected new grain unit is
`Kilogram`; 1 kg of any qualifying grain satisfies 1 kg of food demand, including mixtures,
while keeping separate prices/stocks/storage and consuming actual grain once without a
new food stock item. Existing seed units remain unchanged. Regional population counts people
in aggregate; consumption rates are per 1,000 people per simulation day, prorated by elapsed
simulation time with fractional quantities retained. The selected initial food requirement
is 500 kg per 1,000 people per simulation day, fulfilled by any qualifying grain mixture.
Drinking-water demand is 3,000 litres per 1,000 people per simulation day, separate from
agricultural/industrial water demand. The exact water item and inventory unit remain open;
declare any litre-to-inventory conversion explicitly, preserving physical volume and
fractional interval requirements. Population sizes,
remaining grain/medical authoring values and the exact water item definition remain open.
Any supported migration needs an explicit conversion from legacy abstract world units;
do not count every grain as a separately mandatory essential.

## Basic medical supplies content follow-up

The selected medical essential is one distinct Basic Medical Supplies commodity, traded
in kilograms and representing routine medical consumables. The
[medical-supplies handoff](economy-and-development-plan.md#basic-medical-supplies-commodity-selected-implementation-handoff)
records the proposed `ItemBasicMedicalSupplies` ID, existing authoring/packaging paths and
implementer checks. Preserve existing antibiotics, anaesthetics and analgesics as separate
commodities without automatic substitution. Initial basic-supplies demand is 5 kg per
1,000 people per simulation day, or 125 kg/day for 25,000 people. Preserve fractional
60-second requirements and verify daily totals. Select remaining item fields; no definition,
recipe or new SDK capability has been implemented by this plan.
Preserve the separate eight-good fixture and select the new item explicitly for governance.

## LPG content follow-up

Add LPG (liquefied petroleum gas) as a distinct tradable commodity, measured and traded
in kilograms using the existing `Kilogram` unit. The user explicitly selected kilograms.
Use the existing `CategoryFuels`; proposed item ID is `ItemLPG`, not an implemented
definition. Read-only inspection found no LPG item in the current trade-item definitions.

The user or a user-selected non-OpenAI implementer owns the addition through the ordinary
definition/registry/packaging/Manifest-mode path. A bounded authoring option is
`data/trade-items/chemicals.yaml`, already included by `data/build.sh`, plus explicit
selection in `data/game-mode/manifest.yaml`. Preserve existing item IDs and the separate
eight-good fixture. Availability/age, base price, density, packing/storage conversion,
container handling, production recipes and consumption uses remain open. Kilogram units
do not determine physical volume or assign LPG as a ship's propulsion fuel.

Implementer evidence: a distinct fuel definition using `Kilogram`, valid references and
finite authored values, directory/pack and Manifest-mode visibility, separate stock/orders/
settlement, fractional mass transfers and cargo checks using the selected physical
conversion. Preserve mass and ownership across transfers and recovery. No commodity
YAML, build script, fixture, code or test was changed by this planning work.

## Life Support and space-exploration content follow-up

Add a Life Support category with the following planned items:

| Planned item | Requirement to define |
| --- | --- |
| Oxygen | Separate consumable traded and measured aboard in kilograms ([D-064](decision-ledger.md#d-064)), required at 1 kg per occupant per simulation day while relying on onboard life support. The starter's operating reserve is capped at 30 kg; tank physical volume and other hull capacities remain open. |
| Carbon scrubbers | Replaceable consumable cartridges used by installed life-support machinery. A fresh standard cartridge provides 30 occupant-days. Trade/storage units, physical values and resupply rules remain open. |

The selected distinction is installed machinery versus consumable supplies. Scrubber
machinery stays aboard when cartridges are replaced; the tradable carbon-scrubber item
is the replaceable cartridge. Cartridges have finite scrubbing capacity and require
replacement. Oxygen is a separate consumable, so oxygen reserves and remaining scrubbing
capacity are two distinct exploration supply constraints. Neither replaces the other.
Both consumables belong under Life Support. A fresh standard cartridge is rated at 30
occupant-days. Ship endurance, consequences of exhaustion and spent-cartridge handling
remain open.

The oxygen trading unit is the kilogram ([D-064](decision-ledger.md#d-064), superseding D-060's
kilolitre), the unit of the tanks and the use aboard. Preserve fractional quantities under Manifest's float/double
policy. Oxygen mass is the quoted, transferred and consumed quantity; tank volume, storage
capacity and physical authoring values are separate choices. Selecting mass units does
not select tank pressure, storage method, density/packing values, container ownership or
whether a trade is a refill or a packaged supply. Declare the eventual mass-to-storage/
packed-volume conversion explicitly and apply it consistently to loading and cargo checks.
Do not infer a ship's oxygen reserve or endurance from its cargo volume or cartridge rating.

The selected initial oxygen requirement is 1 kg per occupant per simulation day while
relying on the ship's life support. One occupant on a 30-simulation-day journey requires
30 kg. During applicable intervals, `C` occupants over `dt` simulation seconds require
`C × dt / 86,400` kg, summed over occupancy changes. Retain fractional requirements and
actual remaining mass; inventory cannot supply more than is physically available. Oxygen
remains independent of scrubber capacity: a fresh cartridge does not replenish oxygen.
This is an authored gameplay balance value, not a selected tank capacity or survival rule.
The selected station-service rule below determines when these onboard requirements pause.

The selected scrubbing-capacity unit is the occupant-day: one occupant-day supports one
person for one simulation day (24 simulation hours or 86,400 simulation seconds). The
selected initial standard-cartridge capacity is 30 occupant-days: a fresh cartridge
supports one occupant for 30 simulation days or two for 15 simulation days. This is an
authored gameplay balance value; cartridge mass, physical volume, trade unit and price
remain open. It does not select a ship's installed capacity or carried spare count.
Fractional usage carries forward under Manifest's float/double quantity policy. During intervals requiring
onboard scrubbing, `C` occupants over `dt` simulation seconds require `C × dt / 86,400`
occupant-days, summed over occupancy changes. Retain actual remaining capacity rather than
rounding each interval to whole days or discarding a partially used cartridge's capacity.
This measures scrubbing service capacity; commodity trade units and physical cargo size
remain separate authoring choices.

The selected docking rule pauses onboard oxygen and scrubber consumption while a ship
is docked and connected to working station life support. Docking alone does not establish
an active service connection. Station support covers ongoing occupant needs; it does not
refill oxygen tanks, restore cartridge capacity or replace cartridges. Refills and cartridge
replacement remain separate resupply operations with actual goods/accounting. When the
connection ends, including departure or service loss, applicable onboard consumption
resumes from actual remaining oxygen and scrubbing capacity; no supply reset occurs.
At stations offering connected life support, its ongoing service is included in the
berth fee. Oxygen refills and replacement cartridges have separate prices and remain
separate resupply operations. Preserve the agreed service inclusion under accepted-booking
terms; an owner cannot add a life-support surcharge to an already accepted berth agreement.
Berth fee amounts/billing basis, refill and cartridge prices, service availability,
capacity, connection procedures and station resource/funding rules remain open. Inclusion
in a berth fee does not establish unlimited station supplies or free replenishment.

The selected onboarding requirement is that each new player spawns connected to working
life support at their starting public station. Allocation therefore requires an available
service connection alongside the operational starter-compatible berth and physical
regional delivery connection. An advertised service without available working capacity
does not qualify. Establish and persist the initial connection with the authoritative
spawn/allocation operation, so the new ship starts under station support. Initial starter
supplies are selected below; numeric berth fees, station provisioning and later service
outages remain open.

The 160-tote starter begins with 30 kg of oxygen and one fresh installed standard scrubber
cartridge with 30 occupant-days of capacity. At one occupant, each provides 30 simulation
days of onboard life support under the selected rates. Treat these as explicit initial
onboarding stocks, granted once; reconnect, docking, recovery or acquisition cannot
restore the starting amount. Retain actual remaining mass/capacity thereafter. The ship
starts connected to station service, so these reserves are preserved while that service
works. Overall journey endurance, tank physical volume, compartment dimensions, cartridge
physical values and spare provision remain open. Do not apply these initial quantities
to the separate compact ship without a product decision.

The starter's modular oxygen bank and initial fill follow
[D-050](decision-ledger.md#d-050). Use its canonical specification for authoring and
per-tank validation; cargo oxygen cannot increase installed capacity. This deferred
requirement does not enter the first playable loop's critical path.

The starter's operating oxygen and installed cartridge occupy dedicated life-support
space outside the 160-tote trade hold. Supplies carried for trade or as loose spares count
toward cargo capacity using their authored packed volume. Keep explicit stock locations;
service storage is not a second copy of cargo inventory. The hull must accommodate both
spaces. Transfers/replacement must debit actual source goods once and obey the eventually
authored destination limits, including the starter's 30 kg operating oxygen cap. Timing,
permissions, physical fields and other storage limits remain open.

Implementer evidence must reject allocation when the required working connection is
unavailable, including capacity exhaustion, and show a successful spawn already connected
with both onboard depletion streams paused. Preserve the connection and actual reserves
across retry/recovery without duplicate allocations, charges or resupply. Existing
starter-berth and physical regional-delivery eligibility checks remain required.
Verify the starter receives exactly 30 kg of oxygen and one fresh installed cartridge
with 30 occupant-days once, preserving used quantities across retry/recovery and transfers.
At one occupant relying on onboard service, each covers 30 simulation days, without
counting their separate coverage as 60 days. Validate physical accommodation once authored;
selected initial stock is not a substitute for storage/capacity definitions.
Verify the starter's operating oxygen maximum is 30 kg and its initial reserve fills it.
Consumption, refills, transfers and recovery must respect the cap, without treating cargo
oxygen as installed reserve or silently losing source stock on an overflow rejection.
Verify operating reserves leave the 20 m³ cargo hold available, while trade supplies and
loose spares use cargo volume. Check conserved stock/location and retry/recovery across
the selected transfer/replacement operations, rejecting overflow without duplicate stock.

Implementer evidence must cover connected working service pausing both onboard depletion
streams, a docked ship without service continuing onboard use, and departure/service loss
resuming from retained reserves. Split elapsed intervals at authoritative connection/
service transitions so neither supply is double-debited or silently skipped. Connection,
docking, reconnect and recovery cannot refill tanks or restore cartridges. Explicit
resupply must conserve actual stock and ownership separately. Persist connection/service
state for replay; transition ordering, service accounting and exhaustion behaviour need
resolution before implementation. No free refill or station resource model is selected.
Verify the berth charge includes connected life-support service without a separate ongoing
life-support charge. Refills and replacement cartridges must have distinct authorised
purchase/resupply accounting and cannot be delivered or billed merely because a ship
connects. Accepted fee/service terms survive later policy changes. Numeric tariffs,
billing cadence and service-failure settlements still require design.

Implementer evidence must distinguish machinery, oxygen inventory and cartridges/remaining
capacity. Replacement must debit an actual accessible fresh cartridge once and apply its
declared capacity without duplicating inventory, removing the machinery or replenishing
oxygen. Preserve remaining supplies/capacity across save/replay and ship transfers rather
than resetting them on docking, reconnect or reload. Resolve replacement timing, access,
partial cartridges and spent-item handling before implementing those operations. Existing
life-support skill/effect IDs remain stable; their presence does not implement consumable
accounting or select these new values.
Verify the oxygen definition uses `Kilogram`, independent oxygen mass in quotes/stock/
transfers, fractional quantities and conservation across trade/loading/refill operations
once those operations are selected. Physical capacity checks must use the authored storage
conversion, and saved kilograms must retain their meaning across recovery. Selecting
oxygen kilograms adds no new generic unit enum or loader.
Verify 1 kg per occupant per simulation day and the 30 kg journey example. Check
proportional occupancy/time scaling, fractional interval totals, no occupant-driven use
at zero occupancy and no duplicate depletion across save/retry. At fixed occupancy relying
on onboard supplies, interval requirements must sum to the daily requirement within declared
numeric tolerance. Record only actual mass supplied; exhaustion consequences remain open.
Verify a fresh standard cartridge supplies 30 occupant-days, supporting one occupant for
30 simulation days or two for 15. After 15 occupant-days of use it retains 15, without
resetting on removal, docking or recovery. Verify proportional fractional
use, no occupant-driven depletion at zero occupancy and no capacity gain from dividing
an interval or saving/reloading. At fixed occupancy requiring onboard scrubbing, interval
usage must sum to the daily requirement within declared numeric tolerance. Never record
more capacity supplied than actually remains; exhaustion consequences remain unselected.

Track space-exploration requirements alongside this item list: identify required supplies/
equipment, occupancy and journey duration, storage/cargo or equipment allocation, endurance,
power dependencies, maintenance/resupply and consequences of exhaustion or failure. These
details remain open beyond the selected machinery/cartridge distinction, oxygen kilogram
unit, initial oxygen rate, occupant-day accounting, 30-occupant-day standard cartridge and
connected station service pausing onboard depletion.
Installed/spare capacity, storage values and survival mechanics remain unselected. Apply
the eventual requirements to the relevant ship classes, including the long-journey compact
ship, without silently taking its 32-tote cargo allocation for cabin/equipment needs.

Read-only inspection found no Life Support category, oxygen item or carbon-scrubber item
in the current trade-item definitions. Category/item IDs and authored fields remain to be
selected. Use the ordinary definition/registry/packaging paths and preserve existing IDs.
This note does not amend the selected three-need regional household basket or add content
definitions, assets, scripts or gameplay implementation.

## Content identity

Define a versioned canonical content manifest from the validated effective definitions,
their format versions, ordered selections and selected scenario. Serialise reflected
values with Jettison's binary archive in a specified type/ID order; frame each record
with its type, ID and length. Preserve meaningful sequence order. Exclude host paths,
timestamps, pack compression and diagnostic provenance. Include all loaded gameplay
definitions conservatively, even those unused by the selected scenario.

Use the engine's pinned `Core::Fnv1a64` for an initial deterministic fingerprint and
record the algorithm/manifest-format version. It is a compatibility aid, not an
authenticity check. Preserve the canonical manifest for exact comparison; future save
compatibility must not rely on a 64-bit match alone. Record engine/game versions
separately, since native effect constants can change without YAML changing.
Directory and pack forms of the same effective content must produce identical manifests
and fingerprints; a change to price, selection order or scenario must change the manifest.

**Defined in C02 (2026-11-18)** as `Manifest.ContentManifest` version 1, its layout in
`ContentIdentity.h`; the `content` suite shows directory and pack agree and that a price, an order
or an added definition changes it. Since C05 the scenarios are a type of the list, so every
scenario is in it; since C06 the selected scenario's name follows the records, empty for a set
selected by game mode alone, still version 1 since no save had recorded it; a different scenario
gives a different manifest (the `host` suite).

**Verified in the build since C07 (2026-11-19).** `manifest_validate_mod` runs `manifest-validate`
on the staged directory with each scenario under `data/scenarios/`; `manifest_pack` packs only
content that passed, and `manifest_validate_pack` validates the pack with each scenario and compares
its manifest with the directory's to the byte (`--manifest <file>` writes it, under
`build/<preset>/validated/`). Content the host would refuse fails the build before the pack, and a
pack that loads differently fails it after; the suites that read the pack build through it. The
validator depends on neither.

## Delivery and acceptance

1. Add reflected schemas and engine loading, generated packaging records and exact-value
   equivalence tests. Confirm defaults/null handling and stable IDs before moving callers.
2. Add the aggregate content transaction and shared semantic validation. Switch host
   and development callers to it; remove custom YAML readers and stale dependencies.
3. Add the scenario and a validator CLI using that same entry point. Its content source,
   scenario and overlay ordering must match host options; failure returns nonzero.
4. Add provenance output and content identity, then packaging/CI validation. Keep the
   build graph acyclic: build validator, stage raw content, validate directory, pack,
   validate pack. The validator executable must not depend on its packaged test input.
5. Update architecture, content authoring and build documentation with actual evidence.

The implementer verifies:

- Engine archive round trips, exact prices, stable selections and full/simple content
  equivalence; no direct `YAML::LoadFile` or bespoke field readers remain in game loaders.
- Directory/pack equality of definitions and manifests, unrelated-working-directory
  loading, and host/validator agreement with TUI disabled.
- Layer replacement, patch ordering, append/identity merge, null removal, orphan patches,
  supported test migration, future-version rejection and duplicate/unknown-field errors.
- Cross-type rollback after a late failure, repeated loads, and unchanged active content.
- Every semantic rejection above, plus one-system/two-civilisation/two-venue/eight-good
  scenario validation and preservation of the full authored content set.
- Existing relevant suites and final repository gates in the locked development shell,
  with each machine's concurrency (D-051), local clang-tidy opt-in. Engine changes require its
  gate too.

No builds, implementation changes or gates were performed for this plan. Proceed with
stage 1 implementation first; refresh engine API details before starting stage 2.

### Delivered (C08, 2026-11-19)

Stage 2 is closed. The delivered contract is the sections above marked built or defined:
[the schema contract](#schema-contract-c01-2026-11-18), [layering and the host's
options](#versions-layering-and-migration), [loading](#atomic-loading-and-validation) and
[identity](#content-identity). Each check above is held by a test that runs in the gate:

| Check | Held by |
| --- | --- |
| Round trips, exact prices, selections in order | `content`: every record reads back as written; a price keeps its text; the content set holds each mode in its order |
| Full and simple content | `data`: the simple game mode is a smaller subset of the full one |
| No bespoke YAML reading | No `YAML::Load` in a game loader; the one left reads the pack's `mod.yaml` in the `mod` suite |
| Directory and pack | `mod` (files), `content` (manifests), the console's two loads, and the build's `manifest_validate_pack`, to the byte for each scenario |
| Unrelated working directory | `cli_*` and `validate_*` run from an empty directory |
| Host and validator agree | `host`: `Validate` gives `Start`'s identity and fails with its error text, overlays included |
| TUI off | Not a gate preset. Checked once for C08: `MANIFEST_BUILD_TUI=OFF`, pinned fetch, 32 of 32 tests, no FTXUI in the build graph |
| Layering | `content`: replacement, patch merge, `!append`, `!merge=`, null to default, orphan patch, two-version migration, future version, unknown field; `host`: two overlays patching one field, the later mount winning in either order |
| Duplicates | A record's file is named by its ID, so a duplicate ID is the same file, replaced by layer; a selection, system, region or venue given twice is refused (`content`) |
| Rollback after a late failure | Console: a game mode failing after the other types read keeps the session, its world and its source; a reload afterwards resets the world |
| Semantic rejection | `content`: each rule a record or scenario breaks, naming record, field and layers |
| The scenarios | `content`: the trading and governance fixtures are those their plans describe |

The [stage's exit criterion](development-stages.md#stage-2-validated-content-and-a-small-scenario)
is met. Final gate: `./scripts/ci.sh --werror --asan --tidy` at Lythir's 24 workers against the
pinned fetch `30ea3f3`, no sibling checkout. No Jettison change in C08, so the engine's gate stands
at its last green run (`3413edb`).
