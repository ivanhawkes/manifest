# Stage 2: Jettison definitions and validated scenario content

Planning handoff for Claude, prepared 2026-11-15. Depends on the completed
[stage 1 host](stage-1-host-plan.md) and refines stage 2 of
[development stages](development-stages.md). No implementation or validation runs
are claimed. Inspection used Manifest `feature-base` at `2716380` and Jettison
`feature-base` at `826c4bec125b6631a6ba8ee822c90463316c3511`; recheck after stage 1 lands.

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
- Game-mode definitions retain ordered item/skill references. Full and simple modes
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

## Atomic loading and validation

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

Create a separate scenario; `simple.yaml` currently selects fifteen goods, so it is not
the approved eight-good scenario. Proposed existing IDs: `ItemCopper`, `ItemTin`,
`ItemGold`, `ItemSalt`, `ItemLime`, `ItemFurs`, `ItemGlassware`, `ItemSoap`.
These are a proposed test selection, not a change to full/simple modes or base prices.

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
  with two compile/test jobs and no local clang-tidy. Engine changes require its gate too.

No builds, implementation changes or gates were performed for this plan. Proceed with
stage 1 implementation first; refresh engine API details before starting stage 2.
