# Stage 2: validated content (2026-11-18 to 11-19)

Task records as they were written at the time, newest first. Moved from `docs/history.md` on
2026-11-25; the [history index](../history.md) lists every record.

## Stage 2 cross-review — 2026-11-19

Codex reviewed `aa56443..f715040` in two rounds (`build/reviews/stage-2`). Three findings, each fixed:

- **F1:** D-064 moved copper, tin and bronze to the tonne but left `RecipeBronze` at 9 + 1 → 10,
  a thousand times the selected 9 kg + 1 kg → 10 kg. Now `0.009 + 0.001 → 0.01`; the `data` suite
  checks every project and recipe quantity in kilograms.
- **F2:** served regions needed only the venue's system; the regional plan has a port serve its own
  planet's regions. `Content.cpp` now requires the venue's planet, and the C01 row says so.
- **F3:** the gate's skip key could not see cached build options or the formatter and tidy versions.
  The presets now set every option a stage reads (`BUILD_TESTING` included, round 2), the key holds
  `clang-format` and `clang-tidy`'s versions, and CTest fails a run that finds no tests. Jettison's
  gate had the same key and is fixed alike.

Round 2 resolved F1 and F2 and left F3 open on `BUILD_TESTING` alone, fixed after it; the two rounds
being spent, no third ran.

## C08: Stage 2 closed — 2026-11-19

[Delivered](stage-2-content-plan.md#delivered-c08-2026-11-19) maps each of the plan's checks to the
test that holds it. One was missing and is added: two overlays patching copper's price, the later
mount's patch winning in either order (`host`, overlay `brass`). The TUI-off build is not a gate
preset; it was built once (`build/tui-off`, pinned fetch) and passed 32 of 32. No Jettison gap.

## C07: the validation build graph — 2026-11-19

As [content identity](stage-2-content-plan.md#content-identity) records: `manifest_validate_mod`,
`manifest_pack`, `manifest_validate_pack`, each scenario validated on the directory and on the pack,
the manifests compared to the byte. Scenario `trading` with `starterHull: HullNope` failed the build
at `manifest_validate_mod` and left no pack (checked by hand, restored). The `validate_*` tests add
a `--manifest` with no path (usage, 2) and an unwritable one (1, naming it). No Jettison gap.

## Unknown retired — 2026-11-19

[D-071](../decision-ledger.md#d-071): the user retired the empty fallback `CategoryUnknown`. The
legacy `data/core-config/` files, outside the build, keep it as they keep `CategoryCollectibles`.

## C06: the validator and the host's options — 2026-11-19

[D-070](../decision-ledger.md#d-070), built as
[versions, layering and migration](stage-2-content-plan.md#versions-layering-and-migration)
records. The user selected a required `--scenario` and overlays named by their `mod.yaml`. The host
and `manifest-validate` share `Manifest::Host::Options` and `ParseOptions`; `Validate` loads what
`Start` loads with no world, and `Content::LoadScenario` resolves the scenario's game mode and puts
its name in the content manifest, still version 1. One Jettison gap: the session's package rules
without a world, `Jettison::Host::DescribePackages`, provisioned in Jettison `30ea3f3` (its history,
"Content pipeline: package rules without a world"), and the pin moved to it. The shell files did
not change. The development console still loads by game mode.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 31 of 31 in each of
`clang-debug`, `clang-release` and `clang-asan` against the fetched pin, and tidy clean. `host`
fails `Start` and `Validate` with the same report for every refusal, an overlay's included, agrees on
the content manifest for the directory, the pack and an overlay, and reads the options; the
`cli_*` and `validate_*` tests run both executables from an unrelated directory, a broken overlay
naming its patch.

## Station names — 2026-11-19

The user chose Old Norse for the generated universe's station names, beside Latin for systems and
Greek for planets, each unique among its kind: 634 names in `data/universe/station-names.yaml`,
none in the other two pools. Not loaded yet: the world generator deals from every pool
`build.sh` writes, so the file waits for station generation. Committed without the gate, at the
user's word.

## C09: Jettison's ship and station definitions — 2026-11-19

[D-062](../decision-ledger.md#d-062), built as the
[plan](small-trader-and-docking-plan.md#shared-ship-and-station-definitions-c09-done-2026-11-19)
records. The user selected the long-haul trader as a frigate, the sample's 56 x 28 x 14 m
transport, and the compact single-seat as a provisional 6 x 4 x 1.8 m skiff until W05, and allowed
`Jettison::Dock` (with its ai, physics and spatial) into the content module. One Jettison gap: the
fit rule without a world, `Dock::Fits` over `PadClasses`, provisioned in Jettison `15e2561` (its
history, "Content pipeline: fit without a world"), and the pin moved to it. The shell files did not
change.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan` against the fetched pin, and tidy clean. `content`
refuses each broken dock record, the missing dock hull either way, an unknown layout and a home
whose station the starter does not fit, and fits each hull to the highport's pads.

## Collectibles retired — 2026-11-19

[D-069](../decision-ledger.md#d-069): the user retired the empty `CategoryCollectibles`. Committed without the gate,
at the user's word.

## Twenty-four more goods, and the humans — 2026-11-19

[D-068](../decision-ledger.md#d-068): the 21 goods just outside D-066's 30, natural gas by the new
`MMBtu`, and the user's `CategoryHumans` with slaves, passengers, prisoners and tourists in
`humans.yaml` (renamed from `slaves.yaml`, and `build.sh` with it); `drugs.yaml` joins the list.
`data`'s shared-word check takes machinery in place of the retired slaves shelf.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Astralis at 30 goods — 2026-11-19

[D-067](../decision-ledger.md#d-067): the user allowed Astralis 30 goods; the ten added are listed there.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Thirty new goods — 2026-11-19

[D-066](../decision-ledger.md#d-066): the goods that give the empty crafts, categories and ages a
product, at the 2026-11-19 quotes where they trade and estimates where they do not (both in [the
model](../content.md)). Two units join, `CubicMetre` and `Barrel`; five
files join `build.sh`'s list; Astralis's 20 are re-ranked with the new industrial goods. No test
named the old list's six.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Astralis's goods and the alloys — 2026-11-19

[D-065](../decision-ledger.md#d-065): Astralis's placeholder items are replaced by the 20 goods an
industrial-age world trades most, all already defined; brass and bronze move to `CategoryAlloys`.
`content`'s refusal of the trading fixture under Astralis now names lime, the first fixture good
the mode lacks, and the patch test appends silver, which it also lacks.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Units, today's prices and the item files — 2026-11-19

[D-064](../decision-ledger.md#d-064): every good trades in its real market's unit, with three new
units (`Tonne`, `TroyOunce`, `Carat`) and `Gram` retired, at the 2026-11-19 quotes sourced in
[the model](../content.md). The fixtures' markets, the food need (0.5 t per
1,000 a day) and the Bronze project's inputs (0.09 t and 0.01 t) are converted; the recipe's 9:1
is a ratio and stands. `misc.yaml` is eleven family files, `build.sh` lists them, and CMake's glob
already finds them. Seed and draft prices without a market are unchanged in value.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## The item categories and ages reviewed — 2026-11-19

The user left the first-draft categories and ages open to improvement; the revision is
[D-063](../decision-ledger.md#d-063). `misc.yaml` and the Manifest mode's list are regrouped under the
new ages, the unsorted goods among them, and antimony and neodymium move to `metals.yaml`. Units
were not in scope: potassium ingots, bread, flour and home appliances are still authored by the
kilolitre, as are the bulk solids.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean, with no test changed.

## C05: the trading and governance scenarios — 2026-11-19

The user selected drinking water ([D-060](../decision-ledger.md#d-060)): every fluid is traded by the
kilolitre, so `ItemDrinkingWater` is one, and its need is authored as 3 kL. `Manifest.Scenario`
joins the definition types with the `trading` and `governance` fixtures, checked as recorded under
[scenario and frames](stage-2-content-plan.md#scenario-and-frames); the schema there gained a
starter hull, venue access and life-support capacity, and a region's skills, facilities and
projects, which the starting-access and project checks read. No Jettison gap: a `glm::dvec3`, an
`int64` and a registered enumeration were already archive fields. Which scenario runs, and its name
in the content manifest, are C06's.

Evidence, on Lythir, 24 jobs: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean. `content` checks both fixtures'
shape and refuses each scenario rule, each lack of starting access alone; `data` checks the water
item and the need's 75 kL for 25,000 people; `mod` counts the staged scenarios.

## C04: items, hulls and the balance record — 2026-11-19

The user selected C04's values ([D-059](../decision-ledger.md#d-059)), recorded under
[item authoring](stage-2-content-plan.md#item-authoring-c04-2026-11-19) and
[tuning](stage-2-content-plan.md#tuning-t4-c04-2026-11-19). Three commits: the eight goods and
`CategoryLifeSupport` (`67ed4b0`); the hulls as `Manifest.Hull` definitions with
`LongHaulTrader()` retired and the pin moved to `4d60ea0` (`fdb90a8`); and T4's `Manifest.Tuning`
record. One Jettison gap: a reflected map field, for a hull's `baseStats`, provisioned in
Jettison `4d60ea0` with `std::array` reading fixed (its history, "Content pipeline: map fields").
The shell files did not change.

Evidence, on Lythir, 24 build and test jobs, 16 tidy: `./scripts/ci.sh --werror --asan --tidy`
passed 21 of 21 in each of `clang-debug`, `clang-release` and `clang-asan` against the fetched
pin, and tidy clean once three hull copies in the effects suite became references. `data` checks
the goods, the three hulls and the balance record against the plans' worked examples; `content`
refuses each broken hull and tuning rule, patches a hull's stats by stat, and checks that every
`EffectStat` has a registered name. Before C05: the drinking-water item, unit and conversion.

## C03: one validated content set — 2026-11-18

The host, the console and the data suite load one content set through
`Manifest::Content::LoadContent`, checked as a whole and built away from the caller's state, so a
failed load leaves nothing behind; what was built is recorded under
[atomic loading and validation](stage-2-content-plan.md#atomic-loading-and-validation). The
legacy loaders, `YamlFieldReaders.h`, the name-pool reader, `ConfigTests` and the merged
`core-config` documents are retired with their last callers, and the package is `mod.yaml` and
the records. No Jettison gap: layering, migration chains and per-mount layers are public SDK calls
at pin `e53681e`. Host overlays wait for C06's option contract.

Evidence, on Lythir, 24 build and test jobs, 16 tidy: `./scripts/ci.sh --werror --asan --tidy`
passed 21 of 21 in each of `clang-debug`, `clang-release` and `clang-asan`, and tidy clean (the
first run's two `modernize-use-auto` findings fixed). `content` has 14 cases, 1100 assertions:
every semantic rejection, registry refusals (a future version, an unknown field, an orphan patch),
whole-file replacement, patch merge and `!append`, and a two-version test format's migrations.
`host`, `console` and `data` load through the content set; `console` shows a late type's failure
keeping the session and directory and pack loading the same content. The `sim` replay hashes did
not move: none is pinned over the shipped categories, whose order is now the registry's.

Codex cross-review, two rounds (`build/reviews/c03/`): round 1 found an unselected skill cycle
passing, an empty pool ID through a `.yaml` record, and diagnostics naming discarded layers; all
three were fixed and round 2 confirmed them. Round 2 found whole-graph skill failures reported
without record, field and layer; they are now reported against the cycle's skill with the
smallest prerequisite closure. That fix had no third round; the gate passed on it.

## C02: definitions through the engine registry — 2026-11-18

The six definition types load through `Jettison::Sim::DefinitionRegistry` from records that
`data/build.sh` writes, and the canonical content manifest is defined; what was built is recorded
in the [schema contract](stage-2-content-plan.md#records) and under
[content identity](stage-2-content-plan.md#content-identity). Nothing consumes the records yet:
the host, the TUI and the model still read `core-config` through the legacy loaders, which C03
moves and retires. No Jettison gap: the SDK's Reflect, Serialise, Sim and Vfs at pin `e53681e`
cover it, and are now in the SDK import's required list.

Evidence, on Lythir, 24 build and test jobs, 16 tidy: `./scripts/ci.sh --werror --asan --tidy`
passed 22 of 22 in each of `clang-debug`, `clang-release` and `clang-asan`, with the new `content`
(10 cases, 4757 assertions) and `packaging` suites, and tidy clean. The `sim` replay hashes did not
move: no generator reads the records yet.

## C01: the Stage 2 schema contract — 2026-11-18

The [schema contract](stage-2-content-plan.md#schema-contract-c01-2026-11-18) settles the record
types, field defaults and null handling, ordering, units, the scenario schema and its frames, by
reading the authored data, the current loaders and the SDK at pin `e53681e`. Documentation only: no
code, data or gate changed. No Jettison gap; A01's seams fit. The open authoring decisions it lists
are the user's, before C04 and C05. Next: C02.
