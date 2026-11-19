# Session handoff

## Next, on resuming — 2026-11-19

C08, closing Stage 2, as `delivery-order.md` has it; then Codex's cross-review of the stage's
commits together.

## C07: the validation build graph — 2026-11-19

As [content identity](stage-2-content-plan.md#content-identity) records: `manifest_validate_mod`,
`manifest_pack`, `manifest_validate_pack`, each scenario validated on the directory and on the pack,
the manifests compared to the byte. Scenario `trading` with `starterHull: HullNope` failed the build
at `manifest_validate_mod` and left no pack (checked by hand, restored). The `validate_*` tests add
a `--manifest` with no path (usage, 2) and an unwritable one (1, naming it). No Jettison gap.

## Unknown retired — 2026-11-19

[D-071](decision-ledger.md#d-071): the user retired the empty fallback `CategoryUnknown`. The
legacy `data/core-config/` files, outside the build, keep it as they keep `CategoryCollectibles`.

## C06: the validator and the host's options — 2026-11-19

[D-070](decision-ledger.md#d-070), built as
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

[D-062](decision-ledger.md#d-062), built as the
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

[D-069](decision-ledger.md#d-069): the user retired the empty `CategoryCollectibles`. Committed without the gate,
at the user's word.

## Twenty-four more goods, and the humans — 2026-11-19

[D-068](decision-ledger.md#d-068): the 21 goods just outside D-066's 30, natural gas by the new
`MMBtu`, and the user's `CategoryHumans` with slaves, passengers, prisoners and tourists in
`humans.yaml` (renamed from `slaves.yaml`, and `build.sh` with it); `drugs.yaml` joins the list.
`data`'s shared-word check takes machinery in place of the retired slaves shelf.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Astralis at 30 goods — 2026-11-19

[D-067](decision-ledger.md#d-067): the user allowed Astralis 30 goods; the ten added are listed there.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Thirty new goods — 2026-11-19

[D-066](decision-ledger.md#d-066): the goods that give the empty crafts, categories and ages a
product, at the 2026-11-19 quotes where they trade and estimates where they do not (both in [the
model](model.md#units-prices-and-storage)). Two units join, `CubicMetre` and `Barrel`; five
files join `build.sh`'s list; Astralis's 20 are re-ranked with the new industrial goods. No test
named the old list's six.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Astralis's goods and the alloys — 2026-11-19

[D-065](decision-ledger.md#d-065): Astralis's placeholder items are replaced by the 20 goods an
industrial-age world trades most, all already defined; brass and bronze move to `CategoryAlloys`.
`content`'s refusal of the trading fixture under Astralis now names lime, the first fixture good
the mode lacks, and the patch test appends silver, which it also lacks.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## Units, today's prices and the item files — 2026-11-19

[D-064](decision-ledger.md#d-064): every good trades in its real market's unit, with three new
units (`Tonne`, `TroyOunce`, `Carat`) and `Gram` retired, at the 2026-11-19 quotes sourced in
[the model](model.md#units-prices-and-storage). The fixtures' markets, the food need (0.5 t per
1,000 a day) and the Bronze project's inputs (0.09 t and 0.01 t) are converted; the recipe's 9:1
is a ratio and stands. `misc.yaml` is eleven family files, `build.sh` lists them, and CMake's glob
already finds them. Seed and draft prices without a market are unchanged in value.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean.

## The item categories and ages reviewed — 2026-11-19

The user left the first-draft categories and ages open to improvement; the revision is
[D-063](decision-ledger.md#d-063). `misc.yaml` and the Manifest mode's list are regrouped under the
new ages, the unsorted goods among them, and antimony and neodymium move to `metals.yaml`. Units
were not in scope: potassium ingots, bread, flour and home appliances are still authored by the
kilolitre, as are the bulk solids.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 21 of 21 in each of
`clang-debug`, `clang-release` and `clang-asan`, and tidy clean, with no test changed.

## C05: the trading and governance scenarios — 2026-11-19

The user selected drinking water ([D-060](decision-ledger.md#d-060)): every fluid is traded by the
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

The user selected C04's values ([D-059](decision-ledger.md#d-059)), recorded under
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

## H07: Stage 1 closed, SDK pin `e53681e` — 2026-11-18

The pin moves from `fafa758` to Jettison `feature-base` `e53681ef16878b8eea70af9f59f87d774adb50f3`,
which answers the engine cross-review of H02–H05, A01 and W01 and adds G01, the generic shell. The
headers Manifest includes (`core`, `devtools` Console, `host` Package and Session, `math` Random,
`sim` SimWorld, `vfs` FileSystem) are unchanged between the two, so no Manifest code changed for
the move. The three environment files are byte-identical at both commits: `check-env-parity.sh`
passes against the new pin.

Evidence, on Lythir in the development shell, with D-051's host concurrency (24 build and test
jobs, 16 tidy):

- **Pinned fetch, no sibling:** a clean clone of `9256409` with the new pin (`build/h07-clean`),
  `JETTISON_SOURCE_DIR` unset and empty in each cache, `_deps/jettison-src` at `e53681e` in all
  three presets, fetched from GitHub (LFS included). `./scripts/ci.sh --werror --asan --tidy`:
  `clang-debug`, `clang-release` and `clang-asan` each passed 20 of 20, the `sim` suite's pinned
  replay hashes and `sdk_import` (complete and partial SDK) included. Tidy then found one
  `modernize-use-auto` in `HostTests.cpp`, from H06, whose gate ran without `--tidy`; fixed with
  a `static_cast`, after which the Debug build, the `host` suite and tidy were clean. Release and
  ASan were not rebuilt for that line, the same type on this platform.
- **Local override:** `build/trial-head`, `JETTISON_SOURCE_DIR` at the sibling (clean, `e53681e`),
  passed 20 of 20.
- **Packaging with tests and TUI off:** a fresh tree (`build/h07-notests`) with
  `MANIFEST_BUILD_TESTS=OFF` and `MANIFEST_BUILD_TUI=OFF` built `manifest_pack` and `manifest`,
  no FTXUI in its build graph; `manifest --content <abs>/manifest.pack --check-startup`, run from
  an unrelated directory with `DISPLAY` and `WAYLAND_DISPLAY` unset, reported ready and exit 0.
- **Engine side:** Jettison's independent consumer (`source/test/consumer`) fetched with
  `JETTISON_GIT_TAG=e53681e` passed; the lifecycle, ownership, game-selection, `clang-cpu` and
  sample checks are the engine gate's at that commit, which left it green. H06's content and CLI
  cases are in the 20 suites above.

The [Stage 1 acceptance matrix](stage-1-host-plan.md#acceptance-and-validation-handoff) is met.
Limits that remain: CI on the pushed commit is not yet inspected, and a run with no controlling
terminal is not exercised (as H06). Next: C01.

## M05: the editor examples — 2026-11-18

T7 as [planned](build-and-gates.md#t7-selected-editor-example-plan), with the setup in
[build and gates](build-and-gates.md#editor-setup). The examples' tasks are shell tasks on the
`clang-debug` presets; the launches are the simulation suite and the TUI under `lldb-dap`, in
place of Native Debug's `gdb` entry for the removed `ManifestCoreTests` and its YAML arguments.
`lldb-dap` was chosen for its documented integrated-terminal launch, which the full-screen TUI
needs. The personal `.vscode/launch.json` and `tasks.json`, tracked until now with the
CivSim-era targets, are ignored and untracked; local copies stay on disk, but a pull into another
checkout removes its copies there, so copy the examples again.

Evidence, a clean clone of `f82451d` with the new examples copied to the personal files,
`JETTISON_SOURCE_DIR` unset, on Lythir in the development shell:

- The tasks' commands as written: `cmake --preset clang-debug` fetched the SDK at `fafa758`
  (90 s); `cmake --build --preset clang-debug`, 353 steps (40 s); `ctest --preset clang-debug`,
  20 of 20 suites passed.
- Each launch entry's fields sent to the shell's `lldb-dap` (LLVM 21.1.8) by a minimal Debug
  Adapter Protocol client: the simulation suite stopped at `SimulationTests.cpp:181` and
  continued to exit code 0; the TUI, launched in a pseudo-terminal through the adapter's
  `runInTerminal` request (as the editor's integrated terminal is), took `quit`, stopped at
  `ManifestConsole.cpp:93` and continued to exit code 0.
- Not done: the launch from the editor itself. The client stands in for the editor's side of the
  protocol, so the extension's own handling is unchecked; the user confirms it once.

## H06: the host adapter and the start-up check — 2026-11-18

Built against the pin `fafa758` itself (`_deps/jettison-src`), with no local override.
`Manifest::Host` (`source/game/host/`, outside `Manifest::Game`) composes Manifest into
`Jettison::Host::Session`; `source/apps/manifest/` is
`manifest --content <directory-or-pack> --check-startup`. [Architecture](architecture.md) has the
contract. `Jettison::Host` joins the SDK import list, and `sdk_import` expects it.

Content evidence, the `host` suite (8 cases): the staged directory and `manifest.pack` both start
with an empty world and shut down twice; a session starts after a failed one and beside another.
Each failure is a `StartupError` naming its phase and the source: missing source and a damaged
pack index (mount); a damaged zstd payload of a required file, malformed `mod.yaml`, format version
2, no `mod.yaml`, absent and empty name, wrong identity, absent release, absent engine requirement,
engine 99.0.0, an unmet `requires`, a missing required file and an unreadable one (content). The
unreadable case is skipped, with a message, for a user that reads a file without permissions.

CLI evidence, seven `cli_*` tests run from `build/<preset>/source/apps/manifest/elsewhere` with
absolute paths and `DISPLAY` and `WAYLAND_DISPLAY` unset: directory and pack exit 0; a missing
source logs "Start-up failed in the mount phase, at ..."; no arguments, `--content` with no path,
no `--check-startup` and an unknown argument exit nonzero. Not exercised: a run with no
controlling terminal (nothing in the path opens one).

Gate: `./scripts/ci.sh --werror --asan --log build/gate-h06.log`, with build and test
parallelism at `nproc`; `clang-debug`, `clang-release` and `clang-asan` each passed 20 of 20.
TUI-independence: a fresh tree with `-DMANIFEST_BUILD_TUI=OFF` and the pin's sources built
`manifest_host_tests` and `manifest` and passed `host` and `cli_*`, with no FTXUI target in it.

Fixed step 1 s and seed 0 are placeholders (a `shortcut:` in `Start.cpp`) until A03 selects
the cadence and a scenario its seed. Next: H07.

## H01: the SDK import contract — 2026-11-18

Inventory, from Manifest's CMake: `Jettison::Core`, `Math`, `Vfs` and (with the TUI) `Devtools`;
`jettison_warnings`; the `jettison_pack` tool and the `jettison_add_pack()` helper; the shared
`EnTT::EnTT` and `yaml-cpp::yaml-cpp`, and with tests `doctest::doctest` and
`doctest::doctest_with_main`. `source/external/CMakeLists.txt` previously checked only
`Jettison::Math`, and that only to decide whether to fetch. It now checks the whole list after any
source (pin, `JETTISON_SOURCE_DIR` or a parent) and fails naming each missing one. The future host
target is not required yet: Manifest does not consume it before H06.

Consumer-local packaging: `jettison_add_pack` writes `manifest.pack` into Manifest's own build
tree, and the `mod` suite mounts both it and the staged directory, unchanged. The moving-reference
fallback is already gone in Jettison: at `fafa758`, `source/test/consumer` requires
`JETTISON_SOURCE_DIR` or `JETTISON_GIT_TAG` and fails configuration without either.

`sdk_import` (CTest, about 7 s) configures `source/test/sdk-import` with fetching disabled. With
stand-ins for the whole SDK, Manifest configures. With only `Jettison::Math`, configuration fails,
and the test matches the exact list of ten missing names. With the helper check disabled
temporarily, the test failed.

Gate: the working tree on `ea3edad` plus this change. The pinned fetch was used, with
`_deps/jettison-src` at `fafa758` in every preset. The run used M04's command, logging to
`build/gate-h01.log`. `clang-debug`, `clang-release` and `clang-asan` each passed 12 of 12, then
tidy was clean. The local override `build/trial-head` (`JETTISON_SOURCE_DIR` at the sibling,
`fafa758`) rebuilt and passed 12 of 12.

## M04: SDK pin `fafa758` and the environment parity check — 2026-11-18

SDK review: Jettison `f850920..fafa758` has 78 commits. The headers Manifest includes (`core`
Assert, Hash, Log and Version; `devtools` Console; `math` Random; `vfs` FileSystem) changed only
in comments and blank lines. The build gained `JETTISON_PRESENTATION` (default `ON`, so the
configure is unchanged) and the `host`, `space` and `spacescene` modules, which Manifest does not
link yet. `fafa758` changes only `devenv.nix` (lazygit out) and docs over `c1fe6e7`. A trial
Debug build against `c1fe6e7` (`build/trial-head`) passed 11 of 11 with no Manifest change.

The pin is `fafa758f77bb87b9c79bc4d9246d2b70fd2deea5`. The three environment files were already
byte-identical to it (M03), so none changed. `scripts/check-env-parity.sh` passes against the
pin's `GIT_REPOSITORY` (GitHub) and against the sibling clone. `scripts/test-env-parity.sh` passes
all 14 cases: the match, each file differing (three cases), each file missing (three cases),
`devenv.lock` absent at the pin, no pin, two pins, an abbreviated pin and an unknown pin all
reject, while an uncommitted engine edit still passes against the committed blobs and a
`JETTISON_SOURCE_DIR` at matching files does not stop a difference failing. CI runs both before
Nix.

Gate: a clean clone of `a47816a` (`build/m04-clean`), the pinned fetch, `JETTISON_SOURCE_DIR`
unset and empty in each cache, and `_deps/jettison-src` at `fafa758` in all three presets. On
Lythir, inside the development shell, with D-051's host concurrency:

```sh
env -u JETTISON_SOURCE_DIR CMAKE_BUILD_PARALLEL_LEVEL=24 CTEST_PARALLEL_LEVEL=24 \
  MANIFEST_TIDY_JOBS=16 devenv shell -- ./scripts/ci.sh --werror --asan --tidy --log build/gate.log
```

`clang-debug`, `clang-release` and `clang-asan` each built and passed 11 of 11, the `sim`
suite's pinned replay hashes included; tidy was then clean, and the gate exited 0. CI run
44234896283 (`push`, `feature-base`, `ea3edad`) passed, its environment-parity step first. T5 is
closed.

## M02: feature-base triggers and exact-SHA promotion — 2026-11-18

Repository rules, inspected with `gh api` before the change: `main` and `feature-base` have no
branch protection, the repository has no rulesets, and workflow tokens default to read. Before
M02 every push ran the full gate, so promoting `49a761b` ran it twice (runs 43802514814 and
43802711116). The workflow now runs on `feature-base` pushes, pull requests targeting
`feature-base` and manual dispatch ([D-053](decision-ledger.md#d-053)).
`scripts/promote.sh` does the promotion.

`scripts/promote.sh --check`, run before this commit was pushed, with `origin/main` at `49a761b`:

| Commit | Case | Result |
| --- | --- | --- |
| `f671dd7` (`origin/feature-base`) | own push run 44186843328, success | can fast-forward, exit 0 |
| `3c11f01` | own push run 43853869237, cancelled | refused, exit 1 |
| `ca68066` (local, unpushed) | not on `origin/feature-base` | refused, exit 1 |
| `874df0d` (probe: parent `49a761b^`) | does not descend from `origin/main` | refused as divergence, exit 1 |

The local gate is M01's: `scripts/ci.sh` is unchanged.

CI, after `f99b444` was pushed to `feature-base` with `ca68066` and `5f0e1a2`:

- While run 44230423612 was in progress, `--check` refused `f99b444` (`in_progress`) and
  `ca68066`, which has no run of its own (`runs: none`).
- Run 44230423612 is a `push` run on `feature-base` at `f99b444`. It took 15 minutes and
  succeeded. Its `ci.log` (artifact `ci-log`, downloaded to `build/ci-44230423612/ci.log`) shows
  `clang-debug`, `clang-release` and `clang-asan` each configured, built and passing 11 of 11
  suites in that order, then a clean tidy, which prints only findings. This is also M01's CI
  evidence: `ca68066` is in the tested tree.
- `scripts/promote.sh` fast-forwarded `main` from `49a761b` to `f99b444`, run by the user. No
  run started for `main`: its newest is still 43802711116, at `49a761b`.
- A manual dispatch on `feature-base` started run 44232829065 (`workflow_dispatch`, `f99b444`),
  which was then cancelled.

Repository administration: the ruleset "main: fast-forward only" (27518688) blocks force pushes
to `main` and its deletion, with no bypass actors. T10 is closed.

Tested tree: `feature-base` at `5f0e1a2` plus this commit's `scripts/ci.sh` change, which
moves the tidy stage after the Release and ASan/UBSan stages ([D-052](decision-ledger.md#d-052)).
Tidy keeps `--warnings-as-errors=*` and stays fatal under `set -euo pipefail`. The CI workflow
calls the same script, so it takes the same order. Ran on Lythir inside the development shell
(Claude Code's sandbox), with the host's 24 threads under D-051:

```sh
env CMAKE_BUILD_PARALLEL_LEVEL=24 CTEST_PARALLEL_LEVEL=24 MANIFEST_TIDY_JOBS=24 \
  ./scripts/ci.sh --werror --asan --tidy --log build/gate-m01.log
```

Controlled failure (`build/gate-m01-controlled-failure.log`): the same command, with a
redundant `std::string` initialisation added to `Decimal::ToString`. Formatting passed, then
`clang-debug`, `clang-release` and `clang-asan` each configured, built and passed all 11 suites.
Tidy then reported `readability-redundant-string-init` at `Decimal.cpp:44` and the gate exited 1.
The line was reverted. Passing gate (`build/gate-m01.log`): exit 0, formatting and tidy clean,
11 of 11 suites in each preset. The `sim` replay case passed in every preset at its pinned
values: generation `10355632731109243635`, 100 ticks `12410383246888723003`, raw prices
`15444164946577136117`. The SDK was the pinned fetch, `f850920`, with `JETTISON_SOURCE_DIR`
empty.

This is local evidence. CI on the pushed commit is not yet checked. T10 remains open for M02:
repository protections, exact-SHA promotion, triggers and their evidence.

## Dependency-ordered task queue and stub permission — 2026-11-16

Manifest remains on `feature-base` at `728bb95`. The four uncommitted Stage 1
closeout documents are retained. This follow-up adds the
[prioritised delivery order](delivery-order.md), workflow guidance and ledger entry;
the accumulated changes are seven documentation files. No implementation or gate ran.

The queue orders maintenance, all five stages, the intervening network slice and
existing later workstreams. Each task names prerequisites and a completion boundary;
stage acceptance and committed SDK provision precede dependent integration. T1/T9
remain complete. T2/T3/T6 travel with affected work, T4 sits in Stage 2 and T8 checks
attach to the boundaries they enforce. This is a scheduling index of existing scope,
not new gameplay selection; unresolved choices retain explicit entry conditions.

The user explicitly authorised useful stubs at the implementer's discretion. This is
recorded as [D-056](decision-ledger.md#d-056), with canonical guidance in
[agent workflow](agent-workflow.md#implementer-discretion-to-use-stubs). Scaffolding
can support development, while prerequisite acceptance still requires implemented
behavior and its checks. OpenAI planning-only restrictions remain unchanged.

Next execution is M01, the first T10 slice, followed by the queue. Documentation
validation passed: 32 files, 403 local links, no missing targets or anchors, and
`git diff --check` clean. A read-only check of all 68 task rows confirmed unique IDs,
ordered prerequisite references and complete stage-exit dependency coverage. No
runtime evidence is claimed.
Context recommendation: `/compact` before another planning topic; the execution
order and implementer permission are now recorded in repository documentation.

## Stage 1 planning complete — 2026-11-16

Baseline: Manifest `feature-base` at `728bb95729c41bec887569ae60334acdd7056ea0`,
clean before this documentation closeout. The SDK pin remains `f850920`; the clean
sibling Jettison is still on `feature-base` at `0564f19`. Read-only comparison again
found no source, CMake or asset changes between the pin and sibling HEAD.

The [Stage 1 handoff](stage-1-host-plan.md) is complete for planning. It records the
public API gaps, ownership and lifecycle constraints, delivery slices and acceptance
matrix. No blocking product question remains for this stage; API naming and bounded
extraction details belong to the implementer. No new product decision is selected by
this closeout. The stage index and roadmap now distinguish completed planning from
pending implementation and point to the verified model-library baseline instead of
presenting the historical CI diagnosis as current status.

Uncommitted work is confined to this handoff, the Stage 1 plan, the development-stage
index and roadmap. No code, tests, assets, scripts, configuration, SDK pin or engine
files changed; no build, gate or consumer configure ran. Documentation validation
passed across 31 files and 376 local links with no missing targets or anchors;
`git diff --check` passed. Live referenced-chat retrieval was unavailable; this
closeout uses the repository's recorded decisions and handoffs.

Next execution: complete the selected [T10](build-and-gates.md#t10-selected-gate-and-promotion-policy),
[T5](build-and-gates.md#t5-selected-environment-parity-plan) and
[T7](build-and-gates.md#t7-selected-editor-example-plan) handoffs, then follow the
Stage 1 delivery slices. Implementation and gate execution belong to the user or a
user-selected non-OpenAI implementer under the planning-only policy. Stage 1 remains
unimplemented until its final committed SDK/Manifest pair passes the acceptance matrix.

Context recommendation: `/compact` before Stage 2 planning; the Stage 1 contract is
recorded and the next investigation concerns loaders rather than host composition.

## Stage 1 handoff review — 2026-11-16

Reviewed the refreshed [Stage 1 plan](stage-1-host-plan.md) against the public SDK,
sample entry points, registration sites and archive contract. Corrected delivery
ordering so the new host is provided and proven in Jettison before Manifest requires
it, with the final Manifest bootstrap and compatible SDK update committed together.
The CPU configuration/consumer proof now belongs explicitly to the engine host slice.

The handoff distinguishes explicit Jetsam/sample selection from neutral Flotsam
startup. It also records the existing trader registration's by-value system input:
moving registration earlier with an empty list would break routes despite successful
startup. Acceptance includes actual sample routing and preserved save registrations.
Required metadata checks remain Manifest policy beyond generic archive decoding;
failed startup must not run later hooks or prevent a subsequent independent instance.
These clarify existing Stage 1/boundary requirements without selecting new gameplay.

Only planning documentation changed. Branch/HEAD, SDK pin and the six uncommitted
documentation files are unchanged from the preceding refresh; no implementation or
build/gate execution occurred. Stage 1 implementation remains pending. Its handoff is
ready for the selected implementer after the maintenance prerequisites, beginning with
the existing [T10 contract](build-and-gates.md#t10-selected-gate-and-promotion-policy).
Documentation checks passed: 31 files, 364 local links, no missing targets or anchors;
`git diff --check` passed.

Context recommendation: `/compact` before a separate Stage 2 loader investigation;
the Stage 1 findings and implementation sequence are now recorded here. No additional
Stage 1 planning round is needed unless review raises a specific question.

## Stage 1 planning refresh — 2026-11-16

Manifest remains on `feature-base` at `b105d7c`; the SDK pin is unchanged. The clean
sibling engine remains at `0564f19` on `feature-base`. Read-only comparison found no
implementation changes between the pin and sibling under source, CMake helpers or
assets. The newer integration documentation describes the target boundary; it does
not establish an implemented host seam.

The refreshed [Stage 1 handoff](stage-1-host-plan.md) replaces its obsolete unknown-build
baseline, links the selected maintenance prerequisites, and adds the
[capability map](stage-1-host-plan.md#capability-map-and-demonstrated-gaps) with existing
APIs, demonstrated gaps, ownership and acceptance evidence. It preserves the existing
D-003/D-005 scope and lifecycle contract. Graphics-free configure, explicit sample
composition/content, public metadata parsing and a complete SDK import check remain
implementer work. Broader spatial/schema extraction retains its later-stage ordering.
The development-stage status now links the current baseline rather than treating T1
as uncompleted.

Accumulated uncommitted changes are planning/workflow documentation in README,
build/gates, the decision ledger, this handoff, development stages and the Stage 1 plan.
No implementation, configuration, tests, assets or engine files changed. No build,
gate, debugger or consumer configure ran. T10, T5 and T7 remain pending implementation,
followed by Stage 1; this refresh does not mark any of them complete or select an
implementation provider. The next planning step is review of this bounded Stage 1
handoff; the implementer starts with its prerequisite and consumer-contract slice.
Documentation validation passed across 31 files and 362 local links, with no missing
targets or anchors; `git diff --check` passed.

Context recommendation: continue this session for Stage 1 review; the relevant SDK
evidence and ownership constraints are current and preserved here.

## T7 planning decision — 2026-11-16

Manifest remains on `feature-base` at `b105d7c`. The user accepted
[D-055](decision-ledger.md#d-055); the canonical [T7 handoff](build-and-gates.md#t7-selected-editor-example-plan)
records the affected templates, actual target paths, task ordering, environment requirements
and debugger acceptance checks. Read-only inspection confirmed the targets against CMake and
existing generated CTest commands. The tracked task template already uses `all`; do not carry
forward the historical analysis's `ALL_BUILD` finding as a current tracked-file defect.

The accumulated T10, T5 and T7 planning edits affect README, build/gates, the decision ledger
and this handoff, and remain uncommitted. No implementation, editor configuration, SDK pin or
engine file changed, and no build, test or debugger was run. All three tasks remain pending
implementation and acceptance; their planning completion does not establish Stage 1 readiness.
The next planning subject is Stage 1; execution still follows T10, T5 and T7 first.
Documentation validation passed across 31 files and 345 local links, with no missing targets
or anchors; `git diff --check` passed.

Context recommendation: `/compact` before Stage 1 planning. The small maintenance handoffs
are recorded, and the next architecture task needs the stage and engine-boundary documents
rather than the resolved TUI and editor investigation.

## T5 planning decision — 2026-11-16

Planning continues on `feature-base` at `b105d7c` with the preceding T10 documentation edits
still uncommitted. The user accepted [D-054](decision-ledger.md#d-054). The canonical
[T5 handoff](build-and-gates.md#t5-selected-environment-parity-plan) records cross-repository
ownership, pin/parity-check behavior and required acceptance evidence. D-006 remains in force;
the actual deviation is still open until implementation and verification.

Investigation compared the three environment files to committed SDK blobs read-only. Only
the known extra package entry differs. No engine files, SDK pin, environment configuration,
scripts or tests changed, and no shell evaluation or build/gate ran. Current planning edits
are confined to README, build/gates, the decision ledger and this handoff. T10 and T5 remain
unimplemented; T7 is the next bounded planning handoff before Stage 1.
Documentation validation passed across 31 files and 341 local links, with no missing targets
or anchors; `git diff --check` passed. The four planning-document changes remain uncommitted.

Context recommendation: continue this session for T7; its editor-example inspection is small
and shares the current preset/build context.

## T10 planning decisions — 2026-11-16

Planning baseline: `feature-base` at `b105d7c`, initially clean. The user accepted
[D-052](decision-ledger.md#d-052) and selected the promotion workflow in
[D-053](decision-ledger.md#d-053). The canonical implementation contract, affected files,
remaining repository-rule inspection and acceptance evidence are in the
[T10 policy](build-and-gates.md#t10-selected-gate-and-promotion-policy).

T10 remains unimplemented. This change records planning only; no scripts, workflow
configuration, code or tests changed, and no build, CTest, tidy or CI run was performed.
The implementer should complete T10 using that contract, then address T5 and T7 before Stage 1.
The prior T1/T9 evidence below remains the recorded baseline.
Documentation checks passed: 31 files, 337 local links, no missing targets or anchors;
`git diff --check` passed. The three documentation edits are uncommitted.

Context recommendation: continue this session for adjacent planning; the T10 decisions are
bounded and now recorded. Implementation belongs to the implementer under the general policy.

## T1/T9 local verification — 2026-11-16

Tested tree: `feature-base` at `ee67443` plus the two source changes in this commit.
The user explicitly authorised execution of this bounded task as an exception to the
planning-only workflow; the general policy remains. Decimal parsing now widens the
fractional digit before multiplication, matching the integer branch. Marketplace load
errors prepend their existing prefix in place. Numeric behavior and error text are preserved.
No test expectations, authored data, SDK pin, environment files or gate scripts changed.

Ran on Lythir inside the development shell:

```sh
devenv shell -- env CMAKE_BUILD_PARALLEL_LEVEL=2 CTEST_PARALLEL_LEVEL=2 MANIFEST_TIDY_JOBS=2 \
  ./scripts/ci.sh --werror --asan --tidy --log build/gate-tidy.log
```

The combined command exited successfully. Formatting and clang-tidy passed; all 11 CTest
suites passed separately in `clang-debug`, `clang-release` and `clang-asan`. Each preset's
`Testing/Temporary/LastTest.log` confirms the suite results. The gate log contains neither
of the two original tidy findings and no sanitizer errors. Cache inspection confirms
warnings-as-errors and tests/TUI enabled in all presets, and address/undefined sanitizers
with recovery disabled in `clang-asan`. The fetched SDK is the pinned `f850920`, with no
local source override. New preset trees now exist; the old top-level pre-rename artifacts
are still historical.

The `sim` suite's generation/hundred-tick replay case passed in every preset, retaining:

| State | Hash |
| --- | --- |
| Generation | `10355632731109243635` |
| After 100 ticks | `12410383246888723003` |
| Raw prices, both points | `15444164946577136117` |

The current local gate establishes T1 and the local acceptance of T9. CI acceptance must
be confirmed against the exact pushed commit and its fetched Actions log; local success
does not establish it. T10's gate ordering and duplicate-run work, T5 environment parity
and T7 editor examples remain open, followed by Stage 1 under the analysis priorities.
The project analysis's pre-fix CI diagnosis is historical. This gate does not implement
or validate engine-hosted gameplay.

The user's machine-specific build policy is recorded in [D-051](decision-ledger.md#d-051).
The running gate retained its original worker settings; future runs can use the existing
overrides appropriate to their host. Changes are limited to the two source lines and
workflow/status documentation. The gate log stays ignored under `build/`.
`git diff --check` and the local Markdown target/heading-anchor check passed.

Context recommendation: `/compact` after the push/CI follow-up; this section preserves
the baseline evidence, and the lengthy build output is no longer needed for T10.

## Tank correction after review — 2026-11-16

Read the newest files by modification time, including the project analysis, workflow and
decision ledger. The user's tank correction is recorded once in
[D-050](decision-ledger.md#d-050), with the canonical specification, unresolved servicing
details, SDK capability map and deferred re-entry condition in the ship plan. It refines
D-045; older singular-tank descriptions below are historical. Broader prose deduplication
remains T2, and authoring the selected values remains implementer task T4.

Only planning documentation changed. No implementation, authored YAML, SDK/environment
changes or build/CTest/tidy runs were performed. Documentation checks passed across 31
files, 319 local targets and 88 heading/explicit anchors; `git diff --check` also passed.
These establish no new gate evidence. The verification
and implementer priorities in the analysis remain the next development work.

Context recommendation: `/compact` before continuing; the current decision and review
outcomes are recorded, and the old incremental balance discussion is historical.

## Project analysis and planning consolidation — 2026-11-16

Baseline: Manifest `feature-base` at `f63a3a4`, clean tree, equal to `origin/feature-base` and to
`main`. Engine sibling `../jettison` on `feature-base` at `0564f19`; Manifest pins `f850920`.

This session was a read-only analysis of the repository plus the documentation edits that record it.
No implementation code, tests, fixtures, assets, `data/` YAML, `CMake`, `scripts/`, `devenv.*` or
`.vscode` files were changed, and no build, CTest, clang-format gate or clang-tidy run was
performed, consistent with the planning-only policy in `AGENTS.md`. The documentation changes are
committed and pushed on `feature-base`
(`ac4bca6`, then `ee83ea5`; `origin/main` is two commits behind): new
`docs/project-analysis-2026-11-16.md` and `docs/decision-ledger.md`, plus
references and corrections in `AGENTS.md`, `docs/README.md`, `docs/roadmap.md`,
`docs/development-stages.md`, `docs/build-and-gates.md`, `docs/agent-workflow.md`,
`docs/coding-standards.md` and `README.md`.

Findings, evidence and the T1–T10 handoffs are in the
[project analysis](project-analysis-2026-11-16.md). In short: the model layer (17,398 lines, 8
modules, 11 suites, 111 cases, 89 items and 85 skills) is real and deterministically tested, while
Jettison integration is not (four engine touchpoints, none of them a world host, VFS loader,
script, network or save path), Stage 1 has not started, and documentation is now 8,719 lines
against that code with selected decisions repeated across up to eight files and balance values
living only in prose.

Verification state matters for the next session. The newest recorded full gate is at `5d7f54e`
(formatting plus all 11 suites in Debug, Release and ASan/UBSan; generation `10355632731109243635`,
100-tick `12410383246888723003`, raw price `15444164946577136117`). Since then nothing outside
`docs/`, `README.md`, `AGENTS.md` and `.gitignore` changed, except `devenv.nix` adding `lazygit`,
which alters the locked shell; the merge `2716380` is titled "Not sure of build status". No preset
build tree exists locally and the top-level `build/` is the ignored pre-rename `CivSimLibrary`
tree. Treat the gate as unverified at HEAD until handoff T1 records it.

**CI is not the substitute.** The CI failure analysis added in the same change shows the
[project analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16): CI has failed on
thirteen consecutive runs of the live branches, the format gate, the Debug build and all 11 suites pass
in every one of them, and the runs abort in the CI-only clang-tidy stage on two findings —
`source/game/types/include/manifest/types/Decimal.h:99`
(`bugprone-implicit-widening-of-multiplication-result`, 16 reports because the header is scored per
TU) and `source/game/market/src/Marketplace.cpp:1688`
(`performance-inefficient-string-concatenation`). `scripts/tidy.py` forces
`--warnings-as-errors=*`, which `.clang-tidy` itself does not. Because `scripts/ci.sh` runs tidy
between the Debug and Release stages under `set -euo pipefail`, CI has produced no Release, ASan or
UBSan evidence since 2026-11-14. `AGENTS.md` now carries this status in a section every agent reads
before building, and the same facts are in [build and gates](build-and-gates.md#verification-evidence)
and ledger row D-007.

**Repository-history integrity check.** A push rejection reported by tooling during this session,
naming `9ea337b..7c0a307` and "Docs: record the running-world economy decisions", plus two CI run
records timestamped 2026-11-16T04:21Z, resolve to nothing on GitHub: `gh api` answers *No commit found
for SHA* for both SHAs, no forced push event exists in the retained events window, `FETCH_HEAD` from
that period records the remote advertising `feature-base = 531295e`, and the remote commit list is
linear — `531295e` ← `ee83ea5` ← `ac4bca6` ← `f63a3a4` ← `2094b55`. Nothing was lost and no history
was rewound; those identifiers are excluded from the CI counts. Tool output in this session also
emitted file content and an edit confirmation that did not exist, which is why every claim in these
documents is now phrased from `git show`, `gh api` and fetched logs. `AGENTS.md` and the agent
workflow carry the second-source rule.

The prediction was confirmed on the push of the analysis itself: run 43744555932 on `ee83ea5`, a
documentation-only commit, passed the format gate, the Debug build and all 11 suites, then failed the
tidy stage with the same 16 + 1 reports and emitted no `clang-release` or `clang-asan` output. Its
wall clock was about 17 minutes, so budget 5–17 minutes per gate.

The [decision ledger](decision-ledger.md) now indexes 37 selected decisions by ID with a single
canonical section, status, owner and stage, and the
[agent workflow](agent-workflow.md#planning-discipline-after-the-2026-11-16-analysis) rules require
future planning edits to link IDs instead of restating rules, and to add a stage/SDK-API/gap
triplet before a mechanic is scheduled. Two stale heading anchors in `docs/coding-standards.md`
that pointed at engine headings inside this repository's own documents were repointed at
`../../jettison/docs/…`; that is the only pre-existing content corrected. The duplicated prose in
`scope.md`, `roadmap.md`, `architecture.md`, `development-stages.md` and this file was **not**
reduced: deduplication is handoff T2, an implementer task, so those documents still restate the
same rules and numeric values and remain the drift risk the ledger is meant to remove.

Next step, in order: T9 plus T1 in one gate run — apply the two one-line tidy fixes and run
`./scripts/ci.sh --werror --asan --tidy --log build/gate-tidy.log`, then record the stages, results,
log path and hashes here and push to confirm CI green; T10 move the tidy stage after Release and
ASan/UBSan and stop the duplicate identical-branch runs; T5 reconcile `devenv.nix` with the pin and
add the parity check; T7 repair the tracked `.vscode/*.example` targets; then begin Stage 1 of the
[development stages](development-stages.md#stage-1-public-engine-host-and-standalone-bootstrap),
starting from its exit criterion (Manifest starts, mounts its package and shuts down through public
APIs, the Jettison sample still works, a clean checkout builds without the sibling repository) and
the D-003/D-005 boundary work in Jettison. Use two compile jobs, keep local clang-tidy opt-in
(T9's single gate run is that documented opt-in), and record the gate result here.

Context recommendation: a **fresh session** for T9/T1 and Stage 1. The analysis, the CI failure
analysis and this handoff carry the conclusions, the next task needs only `AGENTS.md`, the ledger
and the stage plan, and this session's investigation output is now historical. Compaction would
keep investigation detail that the documents already preserve.

## Regional population and government correction — 2026-11-16

This section supersedes the planet-scoped population/government assumptions in earlier
handoffs. Baseline: `feature-base` at `2094b55` (`Planning: record economy, market fog and
ship rules`), initially clean. The user corrected the simulation unit: several governmental,
corporate or trading-bloc economic regions can exist on one physical planet, each with
separate population and outcomes. They explicitly selected multiple regions on one planet
in the first playable governance scenario, with each player initially controlling one
region's public budget/development alongside their separate trading company.

The new [regional model](regional-economy-and-governance-plan.md) separates physical planet,
economic region, government/control, corporation, bloc membership, civilisation affiliation
and station market. Regional state includes population, one household pool, needs/shortages,
health/happiness, public accounts, local projects and policies. Organisations can relate to
several regions, but overlapping membership must not duplicate people, money, goods or demand.
Planet-wide totals are derived summaries. Existing wage/rounding/tax/cadence rules remain,
with household/public settlement and tax carry now scoped per employer/region. Employment
across regions needs explicit routing rules before it is enabled. Bronze project values
remain, with the local project/unlock targeted at a region rather than the whole planet.

The user subsequently selected continuous 0–100 scales for each region's health and happiness,
allowing fractional changes. 100 is ideal and 0 the lowest modelled condition. Both start
at 100 in the first balance-testing scenario; joining or acquiring an existing region and
changing governor preserve current values. Save/load retains the saved values. The user then
accepted condition updates after household purchases every 60 simulation seconds: unmet
essentials lower both values, larger shortages cause greater losses and meeting all essentials
allows gradual recovery. Happiness declines and recovers faster than health; bound both to
0–100 without instantly resetting condition when deliveries resume. The user accepted initial
rates per simulation hour: complete shortage loses 6 health/12 happiness, partial-shortage
losses scale proportionally with severity, and full essentials recover 3 health/6 happiness.
Apply fractional changes at the 60-second cadence (complete shortage −0.1/−0.2; full essentials
+0.05/+0.1), retaining bounds. One complete-shortage hour takes 100/100 to 94/88; a following
full-essentials hour yields 97/94. These are balance-test values requiring gameplay validation.
The user selected shortage severity as the weighted average of each required essential need's
percentage unmet, with authored positive importance weights, initially equal (1). Cap each
good's fulfilment at 100%; surplus cannot compensate for another missing essential. Prices
and physical quantities do not set importance. Only actually satisfied current needs count;
exclude optional goods and zero requirements. With no current essentials, there is no essential
shortage; recovery requires all current essentials met, not a rounded display percentage.
Two equally weighted essentials, one fully supplied and one half supplied, give 25% severity
and losses of 1.5 health/3 happiness per simulation hour (0.025/0.05 per normal interval).
The user selected a production work-rate penalty: lower health/happiness slow work, while
completed batches retain declared material requirements, output quantities and total gross
wages. Wage accrual follows completed work, so slower production also slows wage payments.
Condition changes affect subsequent production steps, including remaining work in active
batches, without reversing completed progress or earned/paid wages. Existing interruption,
completion and cancellation rules remain. An illustrative half work rate doubles a ten-minute
batch to twenty minutes for the same completed goods/wages; this does not choose recipe
duration or wage amounts. The user selected the initial combined work-rate multiplier
`(0.5 + health / 200) × (0.5 + happiness / 200)`, ranging from 25% to 100% of otherwise
eligible rate. Example health/happiness → work rate: 100/100 → 100%, 50/100 → 75%,
50/50 → 56.25%, 0/0 → 25%. This limits condition-induced slowdown to 75%; missing materials,
insufficient funds or inactive facilities can still stop production entirely. No recipe,
funding or operation requirement is bypassed. The values require balance validation.
The user selected economic ordering: each fixed step advances production using condition
from the previous completed step and settles completed batches. On each due 60-second
economic boundary, regular payroll/tax comes next, then household purchases/consumption
(essentials before permitted optional goods), then health/happiness update. The new multiplier
applies from the next fixed step. Production continues on intervening steps; household and
condition updates do not. Completion/cancellation settlement still occurs when processed;
regular payments subtract prior settlement so a completion on the boundary cannot double-pay.
New shortages cannot revise completed work or earned wages. Cadence/settled interval state
must survive replay/recovery; the complete movement/docking/market/pricing graph remains
to be resolved in stage 4. Population-size
changes remain separate. Existing float/double policy applies to condition values.

The user accepted an initial bread/drinking-water/basic-medical-supplies basket, then amended
its food component: add rice, wheat, oats and barley as distinct tradable food commodities.
Their explicit follow-up selects one staple-food need satisfied only by these four grains,
not four separately required essentials and not bread. Water and basic medical supplies
remain the other two needs; each of the three need weights initially equals 1. Bread remains
tradable outside this first basket; seed commodities retain their existing IDs/values.

The economy plan's grain handoff proposes `ItemRice`, `ItemWheat`, `ItemOats`, `ItemBarley`
under existing consumables, with a bounded authoring/packaging/full-mode task for the user
or user-selected non-OpenAI implementer. Current files contain the four seed items but no
food items with those proposed IDs. No commodity YAML, code, build scripts, fixtures or tests
were changed. The user then selected `Kilogram` units for all four food grains, with 1 kg
of any qualifying grain meeting 1 kg of food demand, including mixtures. Each retains its
own price/stock/storage; consumption debits the actual grain without creating a food
commodity or changing another grain's inventory. A 100 kg requirement can use 60 kg rice
plus 40 kg barley, or 100 kg of any one qualifying grain. Existing seed units are unchanged.
Exact prices, physical/age values and remaining water/medical authoring
remain open. The user subsequently selected aggregate population counts of people, with
consumption rates authored per 1,000 people per simulation day (24 simulation hours or
86,400 simulation seconds). Required interval quantity is
`r × (P / 1,000) × (dt / 86,400)` for daily rate `r`, population `P` and elapsed seconds `dt`.
A 25,000-person region needs 25 daily baskets over 1,440 normal 60-second updates, using
one household pool. Retain fractional quantities under the float/double policy. Population
sizes and growth/migration rules remain unselected.
Existing population code still uses abstract world units; any supported migration must
declare an explicit conversion to people and preserve totals, never silently relabel units.
The user then accepted an initial staple-food requirement of 500 kg per 1,000 people per
simulation day across the four qualifying grains. For 25,000 people this is 12,500 kg/day
(12.5 tonnes), or `12,500 / 1,440` kg per normal 60-second interval. Preserve fractional
requirements, one substitutable food need and actual grain consumption once. Finite funds,
physical supply, shortages and no automatic catch-up demand still apply. This is a selected
balance-test setting, not a validated economic result.
The user then accepted initial drinking-water demand of 3,000 litres per 1,000 people per
simulation day, with agricultural and industrial water treated as separate demands.
For 25,000 people this is 75,000 litres (75 cubic metres) daily, or `75,000 / 1,440` litres
per normal 60-second interval. Retain fractional quantities, actual accessible potable
supply, finite household funds, single consumption, shortage consequences and no automatic
catch-up purchasing. This is an initial balance-test rate; no viability result is claimed.
Exact water commodity/inventory-unit/price/physical authoring remains open. Explicitly
convert litre-based needs to the selected inventory unit while preserving volume; no unit
enum or item definition has been added. Agricultural/industrial water quantities/rules remain open.
The user then accepted a distinct Basic Medical Supplies commodity, traded in kilograms
and representing routine medical consumables. Antibiotics, anaesthetics and analgesics
remain separate commodities without automatic substitution. The economy plan's new
medical handoff proposes `ItemBasicMedicalSupplies`, a bounded addition through the existing
medicine authoring/registry/packaging/full-mode paths, and implementer checks. The item ID
is proposed only; category choice, availability, price and physical values remain open.
The user then accepted a basic-supplies consumption rate of 5 kg per 1,000 people per
simulation day. For 25,000 people, this is 125 kg/day, or `125 / 1,440` kg per normal
60-second interval. Preserve fractional quantities, actual physical supply, finite funds,
single consumption, shortages and no automatic catch-up demand. All three first-basket
rates are now selected as initial balance settings, without an economic viability result.
No recipe, patient-level simulation or extra health bonus is selected.
No commodity YAML, code, fixtures, scripts or tests were changed.

The user additionally requested oxygen and carbon scrubbers in the planned item list,
explicitly under a Life Support category, plus space-exploration requirements as follow-up.
Stage 2 records both items and the category. Current trade-item definitions contain none
of these. The user subsequently accepted tradable carbon scrubbers as replaceable
consumable cartridges with finite scrubbing capacity, used by installed life-support
machinery that stays aboard. Oxygen remains a separate consumable; oxygen reserves and
remaining scrubbing capacity are distinct exploration supply constraints, both under
Life Support. The user then selected occupant-days as the scrubbing-capacity unit: one
occupant-day supports one person for one simulation day (86,400 simulation seconds).
An illustrative 10-occupant-day cartridge supports one person for ten days or two for five;
this does not select a cartridge rating. Fractional usage carries forward. For intervals
requiring onboard scrubbing, usage is `C × dt / 86,400` occupant-days for `C` occupants
and elapsed simulation seconds `dt`, summed over occupancy changes. The user then selected
kilograms for oxygen inventory and trading, using the existing `Kilogram` unit. Preserve
fractional mass; tank volume/storage capacity and mass-to-packed-volume conversion remain
separate authoring choices. No tank pressure, storage method, refill/packaged-supply rule
or container ownership was selected. The user then accepted initial oxygen use of 1 kg
per occupant per simulation day while relying on onboard life support. One occupant on
a 30-simulation-day journey requires 30 kg. Applicable interval demand is
`C × dt / 86,400` kg for `C` occupants and elapsed simulation seconds `dt`, summed over
occupancy changes, with fractional quantities retained. Oxygen remains independent of
scrubber capacity. This is an initial gameplay balance value; tank physical volume and
exhaustion consequences remain separate open decisions.
The user subsequently accepted 30 occupant-days per fresh standard cartridge: one occupant
for 30 simulation days or two for 15. This is an initial authored balance setting; retain
fractional use and actual remaining capacity. Cartridge mass, physical volume, price and
trade unit remain open, as do ship installed capacity and spare counts. Category/item IDs,
physical values, endurance, power, replacement/resupply, spent-cartridge handling and
exhaustion consequences remain open. Preserve actual inventory and remaining capacity through
replacement, save/replay and transfers; no reset on docking/reconnect/load. Stage 2 records
the distinction and implementer checks, not an implemented supply system.
The user then accepted that docked ships connected to working station life support pause
both onboard oxygen and scrubber consumption. This supplies ongoing needs without refilling
tanks, restoring cartridge capacity or replacing cartridges; resupply remains separate.
Departure or loss of service resumes applicable onboard consumption from actual remaining
reserves. Docking alone does not establish working service. The user then accepted ongoing
connected life support as included in the berth fee at stations offering it, with oxygen
refills and replacement cartridges priced separately. Preserve that inclusion under accepted
booking terms; no new ongoing life-support surcharge can be added to an accepted agreement.
Numeric tariffs/billing, availability, capacity, connection procedures and station resource/
funding accounting remain open; no unlimited station supply or free replenishment is
selected. Stage 2 and the booking plan record billing, transition and recovery checks.
The user then accepted that new players spawn connected to working life support at their
starting public station. Allocation requires available working service capacity alongside
the existing starter-compatible berth and regional delivery connection. An advertised
service without working capacity is insufficient. Preserve the established connection
through retry/recovery, with onboard oxygen/scrubber depletion paused while service works.
The user subsequently accepted initial supplies for the 20-IBC starter: 30 kg oxygen and
one fresh installed standard scrubber cartridge with 30 occupant-days. At one occupant,
each provides 30 simulation days of onboard life support, not complete journey endurance.
Provision these once during onboarding and preserve actual remaining mass/capacity through
reconnect, docking, recovery and transfers. Working station service pauses their use.
The user then accepted dedicated life-support space for the starter's operating oxygen
and installed cartridge, outside the 20-IBC trade hold. Supplies carried for trade or as
loose spares use cargo space under normal packed-volume rules. Keep explicit stock
locations and conserve actual quantities/ownership through transfer or replacement;
service storage cannot duplicate cargo. Physical accommodation must include both spaces.
The user then accepted a 30 kg maximum operating oxygen reserve for the starter, making
its initial tank full. Keep operating mass within 0–30 kg; additional cargo oxygen remains
separate until transferred and cannot increase the operating limit. Tank physical volume/
storage method, upgrades and compact-ship capacity remain open. Compartment dimensions,
transfer procedures, cartridge physical values, spare provision,
compact-ship reserves, station provisioning, numeric berth fees and later outages remain
open; this does not select permanent service uptime or unlimited station capacity.
This is an exploration/life-support design note; it does not add a fourth regional
household essential or change the compact ship's separate four-IBC cargo allocation.
The user additionally requested LPG as a tradable commodity and explicitly selected
kilograms as its unit. Stage 2 records `Kilogram` inventory/trading under existing
`CategoryFuels`, with proposed `ItemLPG` ID and the ordinary authoring/registry/packaging/
full-mode implementer path. No LPG item was found in current trade-item definitions.
Price, availability, physical storage/packing, container handling, production and
consumption uses remain open; no ship-propulsion fuel role is selected. No YAML or
implementation changes were made.
Severity now weights essential needs: aggregate qualifying food consumption in its selected
common measure, then count food once alongside water and medicine. Surplus food cannot offset
missing water or medicine. Full food/water but no medicine gives severity one third and hourly
losses of 2 health/4 happiness; a single qualifying grain can satisfy food without the others
at the selected food rate. No automatic production/processing recipe was chosen;
equal grain substitution is an initial game-balance rule.

Fleet observation is still station-specific, and the 20-IBC starter and separate four-IBC
compact ship requirements remain. One regional government does not imply whole-planet
sovereignty. The user subsequently accepted regional acquisition protection: all
player-governed regions, including acquired regions, are protected from purchase or seizure
through the AI-acquisition mechanic. Otherwise eligible AI regions on the same planet remain
purchasable without a sibling governor's veto; onboarding reserves stay outside the pool.
Purchase transfers public decision rights, with treasury funds staying public and privately
owned companies, ships and stations retaining their owners. Purchase pricing, payment recipient
and voluntary player-to-player transfers remain open. The user also accepted regional frontier
concessions: each names one region and covers its eligible resources, leaving neighbours
independently available under their own eligibility rules. Keep the operational-port/concession
activation trigger and 72-hour server-running duration; government control remains separate.
Activation/expiry in one region must not start another's timer or change its resource rights.
The user accepted one initial port-construction opportunity per eligible region: own, eligible
AI or eligible unclaimed regions qualify; neighbouring ports do not block construction.
Other players' governed regions are protected against unsolicited builds. Keep the funded
construction race and competing ports at frontier concession expiry. A regional association
sets construction eligibility while ports remain physical orbital stations. The user then
accepted shared ports: a port may serve several regions on that planet under its owner's
offered services, fees, priorities and access terms, honouring accepted bookings. Deliveries
name a destination region and require physical transport; inventories, household funds and
public budgets remain separate. Serving neighbours grants no government/resource rights,
consumes none of their initial construction opportunities and reveals no other station's live
market data. Transport costs, capacity and delivery times remain to be designed.
The user also accepted a starting-access guarantee: allocate a region only with an operational
public station supporting the starter ship and a physical delivery connection to that region.
The subsequent life-support decision also requires an available working connection and
spawns the new player connected, as recorded above.
Basic docking, trading and regional delivery access remain available; corporate players cannot
revoke this baseline. Service charges and finite capacity apply, and goods/spending funds
remain finite. Private ports retain commercial policies. Exact fees, capacity and starting
funds still need balancing; access does not establish profitable routes or affordable supplies.
Control changes during construction and compatibility with onboarding
reservations still need rules.

The user additionally requested cultural and corporate character in station design, with
customisable appearances, corporate colours/logos and distinct individual stations. This is
recorded as a presentation requirement in the development plan, with detailed art direction,
assets, controls and rebranding outside this economy/governance design. No visual assets or
customisation implementation were produced.

Shared station services and inter-region physical supply must not merge local inventories
or funds. Generation/migration must allocate old totals
explicitly, without copying a planet's people or treasury into every district.

Current work is uncommitted planning documentation: the regional plan and aligned economy,
government/gameplay, architecture/scope, ship requirements, stages 2–5, host/script, market visibility, procurement,
stargate funding, development-stage, roadmap, index and handoff guidance. No implementation,
assets/configuration, SDK/environment change, builds or gameplay/recovery checks ran.
The implementer must demonstrate regional
isolation on the same planet, public/company permission separation, regional payroll/tax,
local condition/production and projects, conserved migration and saved/replayed relationships.
Acquisition checks must reject another player's home and acquired regions, permit otherwise
eligible AI neighbours without a sibling veto, and preserve public money/private ownership.
Concession checks must isolate resource scope, activation and expiry across regions on the
same planet, retaining original scope/timers and separate government control through recovery.
Port checks must allow separate regional opportunities on one planet, preserve same-region
funded races, reject unsolicited construction in player-governed regions and allow frontier
competition at expiry. Shared-port checks must preserve named destinations, physical transport,
separate regional economies/rights/opportunities and station observation rules, including
protected bookings across new access denials. Detailed transport and construction/control-change
cases need their separate rules. Onboarding checks must reject missing/incompatible/nonoperational
public service or missing physical delivery links, retain basic public access under later
corporate restrictions, and preserve finite capacity/goods/funds and private-port policies.
Also reject unavailable working life support/capacity and verify successful spawn starts
connected, with no onboard depletion or duplicate allocation/resupply through retry/recovery.
Condition checks must establish both initial test values at 100, bounded fractional values
within 0–100 and retention across joining, acquisition, governor change and save/replay.
Demonstrate post-purchase updates once per due economic interval, larger shortages causing
greater losses and full essentials permitting gradual recovery; happiness changes faster
than health away from bounds. Restored deliveries cannot instantly reset condition, and
retry/recovery must not duplicate responses. Include the selected hourly and fractional
per-interval examples, partial-shortage scaling, bounds and elapsed-simulation-time semantics.
Include the two-good 25%-severity case, noncompensating surpluses, weights independent of
price/quantity, optional/zero-required goods excluded and recovery only after full essentials.
With controlled work rates, check identical completed material/output/payroll totals, twice
the baseline duration at half rate, and only subsequent progress/wage accrual changing after
mid-batch condition changes. Preserve completed progress and earned/paid wages through recovery.
Verify the four selected multiplier examples, independent/symmetric condition effects and
material/funding/operation blocks even at the 25% condition floor.
At economic boundaries, prove old condition governs current-step work, completion and regular
payroll cannot double-pay, payroll/tax precedes purchases and resulting condition affects only
the next fixed step. Intervening steps must not repeat household/condition updates; recovery
retains cadence and settled interval state.
The grain implementer must establish four distinct tradeable food definitions, unchanged seed
IDs, valid references/physical/price fields, full-mode and directory/pack visibility, separate
inventory/trades/cargo and the single substitutable food need without double consumption.
Check selected `Kilogram` units, equal substitution and the 100 kg single-grain/60+40 kg
mixture examples, with distinct inventories and no synthetic food commodity.
Verify per-1,000-person daily scaling, fractional 60-second requirements and no demand
at zero population. With a fixed population, 1,440 normal intervals must match the authored
daily requirement within the declared numeric tolerance. Supported migration must apply
its explicit legacy conversion once and preserve totals when splitting regions.
Check 500 kg/day for 1,000 people and 12,500 kg/day for 25,000 people, including one grain
and mixtures, without multiplying demand by the number of grain alternatives. Insufficient
funds or physical stock must produce recorded shortages rather than synthetic consumption.
Verify 3,000 litres/day per 1,000 people and 75,000 litres/day for 25,000 people, fractional
60-second water requirements and volume-preserving inventory-unit conversion. Keep
agricultural/industrial uses separate, with no double consumption of physical stock.
Verify one distinct kilogram-based Basic Medical Supplies definition with separate stock,
trades and cargo capacity; preserve specialised medicine definitions, directory/pack and
full-mode visibility. Verify demand of 5 kg/day per 1,000 people and 125 kg/day for 25,000
people, fractional intervals and daily totals. Consume actual accessible supplies once
and retain shortages without silent substitution or automatic catch-up demand.
For scrubbers, verify a fresh standard cartridge provides 30 occupant-days: one occupant
for 30 simulation days or two for 15. After 15 occupant-days used, 15 remain without a
reset on removal/docking/recovery. Check fractional use and
daily/interval consistency, zero occupant-driven usage at zero occupancy, no double
depletion or capacity gain through save/retry, and retained partial capacity. Supplied
capacity cannot exceed the actual remaining reserve; exhaustion consequences are open.
Verify oxygen uses `Kilogram`, with fractional mass, separate stock/transfers and conserved
mass through the selected trade/loading/refill paths; physical storage checks must use
the authored conversion. Save/recovery must preserve kilogram quantities without resets.
Verify the selected 1 kg/occupant/day oxygen rate and 30 kg journey example, proportional
occupancy/time scaling, fractional daily/interval consistency and zero occupant-driven
usage at zero occupancy, with no double depletion through retry/recovery. Actual supplied
mass cannot exceed the reserve; exhaustion rules remain open. Verify the selected connected
station-service pause for both onboard supplies, continued onboard use without working
service, and resumed use after departure/service loss. Split intervals at authoritative
service transitions, with no double debit, skipped use or automatic replenishment through
connection, docking, retries or recovery. Preserve separate resupply stock/ownership.
Verify the starter initially receives 30 kg oxygen and one fresh installed cartridge
with 30 occupant-days once; retain actual remaining supplies through retry/recovery,
reconnect, docking and transfers. Show each covers 30 simulation days at one occupant
onboard use, with depletion paused under station support. Physical accommodation must
be validated after storage/tank/cartridge definitions are authored. Compact initial
reserves and complete journey endurance remain open.
Verify starter operating reserves/installed cartridge do not consume its 20 m³ trade
hold; trade supplies and loose spares do use their authored cargo volume. Preserve stock
locations, quantities and ownership through transfer/retry/recovery. Once destination
limits and procedures are selected, reject overflow without duplication or loss.
Verify the starter begins full at its 30 kg operating oxygen cap and retains actual
remaining mass after use. Refills/transfers/recovery cannot exceed that cap or count
cargo oxygen as installed reserve; overflow rejection must not lose source stock.
Verify connected life support is covered by the berth fee at stations offering it, without
a separate ongoing-service charge. Oxygen refills/cartridges need separate authorised
purchases and actual supply. Preserve accepted service/fee inclusion across owner policy
changes; billing cadence, numeric charges and service-failure settlements remain open.
For LPG, verify a distinct `Kilogram` fuel item, directory/pack/full-mode visibility,
separate trades/stock, fractional mass and physical cargo checks, preserving ownership
and quantities through recovery.
OpenAI agents supply the plan only under AGENTS.md; implementation/gates belong to the implementer.
OpenAI agents remain planning-only. Earlier planning is committed in `2094b55`; no push
occurred during that commit request.

Documentation validation: `git diff --check` and 191 local Markdown targets, including
24 heading anchors, across 19 changed/new planning documents passed. No missing targets,
trailing whitespace or missing final newlines were found. These are documentation checks,
not evidence of a working regional simulation or compatible migration.

Next choose onboard transfer/replacement procedures, remaining storage limits, station provisioning/capacity and
numeric tariffs, remaining commodity authoring values and within-basket purchase allocation.
Region generation/allocation, organisation powers,
cross-region employment/tax, ground transport and shared technology remain open.
Continue this session for the Life Support/exploration requirements, which build on the
recorded ship and item decisions. Do not treat the older one-home-planet allocation
formula or employer/planet tax key below as the current contract.

## Household rules, population condition and market intelligence — 2026-11-15

Resumed on `feature-base` at `4e78dbbf99b2951aa7da8de3591965802a65716d`
(`Planning: record household circulation and payroll funding`), matching local
`origin/feature-base`, with a clean working tree. The preceding household/payroll session
is committed; its references below to planned commits, unresolved payment timing and tax
remainders are historical. This follow-up selects payroll timing/cancellation, wage/tax
rounding, recipe wage policy, the initial regular payroll/household cadence, household
spending/shortage accounting, tracked health/happiness consequences and market information
rules. Current changes are planning documentation in this handoff, the
[economy plan](economy-and-development-plan.md), the new
[market-information plan](market-information-plan.md) and aligned gameplay/server,
development-stage, authority/trading, host/script, planetary, roadmap and docs-index guidance.
The subsequent [small-ship/berth plan](small-trader-and-docking-plan.md) adds the user's
compact one-occupant hull and matching dock requirements, also aligned with those plans.

The user accepted wages earned in proportion to completed production work and paid in
batches on economic updates from the full gross payroll reserved before starting. Apply
the selected 10% withholding when paying. Facility interruptions pause work and wage
accrual, preserving completed progress and remaining payroll. Resuming continues the same
batch without paying twice. Cancellation pays earned but unpaid wages, then releases
unearned payroll to the employer's available funds. Completion settles the remaining wages;
previously paid wages and taxes stay settled.

Example: cancelling a 100-credit payroll batch at 40% completion means 40 credits earned
in total. With no interleaved payroll for that employer/planet, this splits into 36 to
households and 4 to the local treasury, including earlier payments. Pay only what remains
unpaid and release the unearned 60. Numeric wage amounts remain unresolved, as do material
consumption, unfinished goods and salvage rules on production cancellation.

The user accepted an authored gross wage bill per production recipe, shown before starting
and fixed for that batch. Employers fund it from their available funds; market-price changes
affect margins while the batch's quoted wage bill stays fixed. Select actual starting wages
through balance work against input costs, handling/other fees and expected sale proceeds.
The explanatory bronze example's 20-credit wage bill and its input/fee/revenue figures were
illustrative, not selected balance values or evidence of profitability. Recipe wage policy
is selected; numeric wage costs remain open.

The user accepted whole-credit wage rounding: during production, round cumulative earned
gross down and subtract earlier gross payments, retaining fractional work progress for later
payments in the same batch. Completion pays the full declared payroll less earlier payments.
Cancellation rounds cumulative earned gross up once, capped at the originally reserved total,
then subtracts earlier payments before releasing the remaining reservation. Zero completed
work earns zero wages. A 100-credit batch cancelled at 40.5% owes 41 gross in total; if 40
was already paid, pay one more and release 59. Cancellation can cost less than one extra
credit above exact proportional earnings, drawn from reserved funds. Withhold tax on the
gross actually paid; spendable balances stay whole credits.

The user also accepted carrying tax remainders across payments for each employer and planet.
Every ten cumulative gross credits actually paid produces one treasury credit; smaller
payments carry the remainder forward. Completion/cancellation does not reset it. Starting
from zero, nine one-credit gross payments go to households; the tenth goes to the treasury.
The remainder is separate accounting state, not fractional spendable money. Preserve it
with the matching settlements across retries/restarts, independently for each employer/planet.
This prevents splitting payroll across small payments or production batches from avoiding tax.

The user accepted regular payroll and household purchases every 60 seconds of simulation
time as an initial balance-testing setting. Production continues on fixed steps; completion
and cancellation settle remaining wages when processed. Shutdown pauses simulation/cadence;
disconnection does not stop eligible world progress. Preserve the schedule's position through
recovery. The interval is simulation time, with real elapsed duration depending on pacing.
The implementer must measure cash flow and workload before tuning it. The full production/
market schedule remains open; no workload or circulation result is claimed.

The user accepted an authored essentials basket scaled by population and elapsed simulation
time. Settle scheduled payroll before household purchases. Buy essentials within available
household funds and physically delivered local stock; optional purchases wait until that
interval's essential needs are met. Carry unspent credits forward. Exact basket contents/
quantities, allocation among essentials, optional demand, additional savings targets and
detailed health/happiness consequences remain open. This is aggregate planet accounting, with no
individual citizen/family shopping or accounts.

The user accepted fresh consumption needs each interval. Record missed consumption as unmet
needs; the next interval uses its new needs, without automatic catch-up purchases for earlier
missed consumption. Missing food in one interval does not automatically double the next
food target. Retain shortage measures for future consequence rules; fresh purchase targets
do not erase past shortages or count missing stock as consumed. The user subsequently
selected tracked health/happiness losses and their production effects below; detailed
response/recovery rules remain open.

The user required aggregate population health and happiness as authoritative simulation
state per inhabited planet. Shortages lower these values; both reduce production output.
Save/hash/replay them with world state and preserve them across needs intervals. No
individual citizen entities are introduced. Value scales, starting values, which needs
affect each, decline/recovery rates, combined penalty, bounds, work-rate versus yield
mechanism and exact update ordering remain open. Preserve physical recipe accounting and
the accepted payroll rules. Existing population/shortage and body-effect code does not
implement these values; `MedicineCoverage` is service coverage, not population health.
The economy plan records public SDK state/system/persistence provision and implementer
checks, without claiming a new generic engine gap or any execution evidence.

The user required market fog of war. Exact current bids, asks, sales data and related
station trade information are available only where a member of the player's fleet is
docked. Multiple fleet members provide simultaneous coverage; when the last leaves a
station, freeze its last-known report. Retain remembered reports and their observation
time, distinguishing live/stale/unknown and showing stale-data age in both TUI and GUI.
The fleet's server-side docking state, not a client connection, determines coverage.

At a new player's spawn, their starting station is live because their ship is docked.
Also seed one-time spot reports for every station market in every directly connected
system; the user explicitly chose every station market. Reports reflect spawn-time values
but grant no ongoing feed. They are hints, not guaranteed arrival prices. Reconnect/retry
cannot reseed them; later expansion does not repeat the starting grant. Beyond-neighbour
markets remain unknown unless observed. Fleet coverage is an intended competitive advantage.

The new market-information plan maps raw unfiltered quotes/global `PlanTrade`, public SDK
docking, saved world state and globally shown snapshots to required Manifest observation
and query work. Filter before frontend replication and before native/script option building,
ranking and scoring; hidden current data must not leak through route suggestions or derived
facts. NPC trading companies use their own fleets/observations. Normal authoritative market
settlement still uses actual state. Keep privileged sandbox diagnostics distinct from
ordinary player queries. Concrete scheduler dependencies, future company-level sharing,
atomic transfer mechanics and topology discovery remain open.

The user accepted simulation-time report age, shown with explicit simulation units.
Disconnection does not pause age; server shutdown does. Starting reports remain stale
even at zero age because they have no ongoing feed. Save/load preserves the report's
original observation time. At each completed step's end, refresh markets with qualifying
fleet members docked at that boundary using completed market state. Newly docked ships
receive that step's report; when the last ship leaves, retain the previous report without
revealing later changes from the departure step. Another docked ship maintains updates.
Age runs from the last observation, not the departure notification. The market plan and
stage 4 ordering record the selected boundary; concrete scheduler dependencies and
atomic ownership-transfer mechanics still need implementer design and evidence.

The user accepted per-good, per-station sales quantities over the previous 60 simulation
minutes as an initial balance setting, counting completed trades once rather than both
sides. Show the last sale price and sale time separately, including when it predates the
window. Distinguish no sales from unknown data and show shorter coverage for markets less
than an hour old. A stale report freezes its original volume, reporting window and last-sale
facts until another permitted observation; it never silently becomes a current rolling-hour
figure. Save/load must preserve those facts and the live aggregation history. The copper
example is illustrative only. The existing cumulative volume field does not implement the
selected window; aggregation/storage and exact window-endpoint conventions remain for
implementer design and evidence.

The user accepted auto-traders using stale reports without automatic expiry, showing
estimated profit alongside report ages. On arrival, use docking's completed live report
and check configured trading limits; execution still enforces limits against actual state.
Skip out-of-limit transactions and reconsider routes only from available fleet reports.
Retain aboard cargo while seeking an acceptable sale; selling below the configured minimum
requires an authorised fallback policy. Unknown markets can be scouting destinations but
provide no invented prices, volumes or profit estimates. Standing policies operate while
disconnected. The market, stage 4 and host/script plans record these rules and distinguish
trading fallback authority from the absent-script native fallback. Detailed scouting/scoring
remains open. The copper arrival-price example is illustrative only.

The user accepted preserve-capital default trading limits: start only on positive estimated
profit after purchase/transport/handling costs, cap the purchase price before buying and
require sale proceeds to recover purchase and attributable trading costs. Players can set
a higher profit target. Waiting and inaccurate reports do not authorise relaxing limits.
Blocked jobs retain cargo, reconsider every 60 simulation seconds and may reconsider on new
permitted fleet observations; notify the first blockage and show waiting reasons/holding
costs. Continued charges mean profit is not guaranteed. An explicitly authorised loss
allowance applies to the entire cargo across partial sales; default allowance is zero.
The 100-credit allowance example is illustrative. Preserve limits and shared allowance
accounting across retries, replanning and recovery. Exact cost attribution, rounding,
allowance accounting/reservations, policy edits and retry scheduling dependencies remain
implementer design work. These defaults are recorded in the market and stage 4 plans.

The user accepted player-owned market memory: ship transfers preserve the former owner's
reports and do not transfer historical observations to the recipient. Coverage follows
qualifying fleet ownership at the completed observation boundary. Losing the last docked
ship freezes the former owner's report; a recipient acquiring a ship still docked there
gains live coverage at the next completed step. Another qualifying ship preserves coverage.
Initially allies do not automatically share intelligence. Future company-level sharing and
information-sale mechanics need separate rules; atomic ship sale/transfer mechanics remain
design work.

The user requested a new small ship class with space for one occupant on a long journey,
like a modern truck's tiny living cabin, and only four IBC units of cargo, plus a dock sized
for that class. Existing units define this as 4 m³ usable cargo, separate from accommodation
and equipment. Do not infer a four-tonne payload or integer-only commodity quantities.
The new small-ship plan maps Manifest hull/stats/cargo work and a demonstrated Jettison gap:
the pinned SDK's smallest S pad is fixed at 35 × 40 m, with no per-pad size field. Provision
a reusable smaller-berth capability in Jettison before updating the SDK pin; game hull/cabin,
cargo and station values belong in Manifest. Dimensions, endurance, price/operating costs,
fitting details and the compact ship's primary role remain open. No engine/content implementation or
validation ran. The existing long-haul hull and authored IDs remain unchanged.

The user clarified that the lone-trader entry/starter ship has a 20-IBC (20 m³) cargo hold.
The new four-IBC compact ship is separate: its primary purpose is currently unspecified,
trade is secondary, but carrying some cargo must be possible. Do not label it a trader,
scout or courier as a selected role. The compact cabin/long-journey and matching small-dock
requirements remain. Starter upgrades, dimensions/endurance/costs for both hulls and berth
compatibility for the 20-IBC starter remain open. The ship plan and aligned gameplay,
stage 4, development-stage, roadmap and index guidance distinguish the two capacities.

The economy plan records implementer checks for partial work, interruption/resumption,
cancellation/completion after earlier payments, fractional progress and rounding boundaries,
tax-remainder carry, the quoted recipe wage bill, cadence boundaries and schedule continuity,
payroll before household purchases, essential-first optional eligibility, unspent-credit
carry, fresh interval targets with retained shortage measures and no catch-up purchases,
health/happiness production effects, and recovery around accrual, payment and release.
The market-information plan adds distinct-player/fleet views, stale prices/volume,
every-market starting reports, observation/AI/script filtering, simulation-time age,
arrival/departure step boundaries, sales-window accounting/freezing, arrival limit checks,
cargo retention/fallback authority, default profit/price limits, blocked-job retries,
shared cargo loss allowances, unknown-market scouting, ship-transfer intelligence and
recovery/scale checks. The small-ship plan adds four-IBC capacity, berth-fit/public SDK,
content and recovery acceptance requirements for the implementer, plus separate 20-IBC
starter onboarding/capacity checks.
No implementation, SDK/environment change, build, gameplay or performance check
occurred. OpenAI agents remain planning-only; execution belongs to the user or their selected
non-OpenAI implementer. At the user's request, these 12 planning documents are included
in the commit containing this handoff, titled `Planning: record economy, market fog and ship rules`.
Resolve its resulting hash from Git history when resuming. No push was requested.
Documentation validation passed: `git diff --check` and 134 local Markdown targets,
including 12 heading anchors, across all 12 changed/new planning documents. No missing
targets, trailing whitespace or missing final newlines were found.

Next choose detailed consumer quantities/allocation, health/happiness decline/recovery and
production penalties, numeric wage amounts, remaining system order and explicitly funded
bootstrap balances. Market-information follow-up choices include atomic ship-transfer
mechanics, detailed scouting/scoring and precise cost/loss-allowance accounting. Small-ship
follow-up choices include dimensions/berth geometry, endurance, fittings, costs, starter
upgrades and the compact ship's primary purpose. Starter capacity/identity is selected.
These choices remain open. Recommend `/compact` before the next design choice: the accepted population-condition
and market-information contracts are preserved in the focused plans, while earlier payroll
discussion and repeated source excerpts are no longer needed in full.
Earlier stargate and household/monetary decisions remain in force.

## Session close: household circulation and payroll funding — 2026-11-15

Branch: `feature-base`. Base commit: `08b819d` (`Planning: record stargate lifecycle and
competing access`), in sync with origin when this follow-up began. This session's changes
are this handoff and the [economy plan](economy-and-development-plan.md), to be committed
together as `Planning: record household circulation and payroll funding` and pushed at the
user's request. Resolve the resulting commit hash from Git history when resuming and check
the working tree. Earlier sections' references to uncommitted gate work are historical;
the gate decisions are already committed in `08b819d`.

OpenAI agents remain planning-only under `AGENTS.md` and `docs/agent-workflow.md`.
Implementation and build/gate execution belong to the user or their selected non-OpenAI
implementer. All changes in this follow-up are planning documentation.

The user accepted the household/wage foundation and explicitly confirmed its performance
model: exactly one aggregate household pool per inhabited planet, independent of population.
Track pooled credits, population, commodity demand and needs fulfilment. Employers pay wages
from available funds; households buy physically delivered consumer goods within budget,
prioritising essentials. Consumption removes goods once; missing money/stock leaves unmet
needs. Sales replenish the actual sellers through existing broker/producer accounting.
Household, public and business funds remain separate, with explicit generated starting funds.
GDP measures activity rather than automatically awarding spendable development credits.

The user accepted 10% income-tax withholding on wages actually paid, as an initial
balance-testing rate. Split gross payroll between the household pool and local planetary
treasury at settlement: an employer pays 100 credits, households receive 90 and the treasury
receives 10. Unpaid wages generate no tax. Record this in the same authoritative payroll
settlement, with one treasury credit per payroll batch and no per-citizen work. Whole-credit
rounding/remainder treatment and later rate-setting powers remain unresolved. This transfers
existing credits and does not establish sustainable public revenue or create new currency.

The user accepted no periodic currency creation in the first economy scenario: declare all
starting balances; wages, taxes, purchases and fees transfer existing credits. Expose cash
shortages rather than automatically replenishing accounts. The implementer must measure
whether accumulation drains household, employer or government spending capacity before a
continuing currency source is chosen. This does not promise sustainable fixed-money play
or select later join/new-system funding. The economy plan records balance reconciliation,
liquidity/turnover measurements and recovery evidence required from the implementer.

The user accepted pre-funded production payroll. Each batch declares a gross wage cost;
reserve that payroll alongside required inputs before starting. Insufficient funds block
new batches while existing stock remains sellable. Reserved payroll cannot fund another
project or purchase. Reservation does not pay wages or generate tax. Wage amounts, payment
timing, interruptions/cancellation and release of unused payroll remain unresolved; neither
payment at start nor payment at completion has been selected.

Batch payroll and purchases on an economic schedule within the authoritative fixed-step
world. There are no individual citizen/family agents or accounts and no per-person orders;
population changes quantities, not household entity count. Work is intended to scale with
planets, facilities/employers, commodities and transactions. Cadence, wage/employment policy,
consumer baskets/savings, GDP accounting, rounding, transaction bounds and actual performance
limits remain unresolved. No throughput or sustainable-circulation result is established.

The economy plan records required implementer accounting/recovery checks and separate
population-magnitude and planet/employer/commodity scale measurements. No implementation,
SDK/environment change, build or gameplay/performance check occurred. The first public-revenue,
initial monetary and batch payroll-funding rules are selected. Consumer policy and long-running
circulation still need further design and implementer evidence; later currency issuance remains
unselected.
Documentation validation passed: `git diff --check` and 21 local Markdown targets across the
two changed documents. These are the only files in this session's commit scope.

Next session: read this handoff and the selected household/payroll/tax/monetary sections of
the economy plan. First choose payroll payment timing and interruption/cancellation terms,
then wage amounts and whole-credit tax rounding. Continue with economic cadence, consumer
baskets/savings and explicitly funded bootstrap balances before implementation validation.
Do not treat any of these open choices as accepted. Existing stargate decisions remain in
force; their unresolved recipes, repair/site/transfer rules and SDK integration proof are
separate later work in the stargate plan below.

The user ended this session at its context limit. Recommend a fresh session with `/clear`
or `/new`, starting from `docs/session-handoff.md`: accepted decisions and remaining choices
are preserved here, and the repeated investigation output is no longer needed.

## Stargate lifecycle and competing-access follow-up — 2026-11-15

Resumed planning on `feature-base` at `df12cbf`, pushed to origin, with a clean working tree.
The user selected a fixed pair of stargates per connection. Each endpoint serves its named
connection; another destination needs another pair. This does not select a per-system gate
cap or resolve later retargeting/transfer rules. The prior decisions below remain in force.

Current uncommitted changes are planning documentation in this handoff and the
[stargate plan](stargate-expansion-plan.md). The user also accepted manufacturing both
endpoints at an accessible origin, installing one and sending the other with self-contained
unmanned propulsion/power/deployment equipment. Activate only when both endpoints are
commissioned/ready; disclose flight arrival separately from trade opening. Initial ownership
is both endpoints held by the government/corporate sponsor, as public/company assets
respectively. Equipment costs, operating supply quantities and repair terms remain unresolved; no destination
workers or existing port are required for
the selected autonomous deployment model.

The user also accepted the launch equipment/funding model: authorise/reserve a capped project
budget, paying for delivered equipment and completed services during preparation. Launch only
after the origin gate is commissioned and the remote gate, carrier, FTL drive, power, controls,
deployment equipment and consumables are physically ready. Fund remaining contracted flight/
deployment services before departure; the launched mission needs no routine cash top-ups.
Pause preparation overruns for an explicit budget increase. Pre-launch cancellation releases
uncommitted funds while preserving equipment ownership, paid work and awarded supplier
obligations. Exact quantities and credit costs remain balance work; post-launch recall/
cancellation remains unresolved. Initial operating rules are selected below.

The user accepted two server-running hours for remote deployment/commissioning as an initial
balance-testing value quoted before launch. Work starts after arrival when equipment can
operate; shutdown/equipment interruption pauses progress, disconnection does not. Preserve
completed work through interruptions/restarts. An unavailable origin delays opening after
remote commissioning; both endpoints must be ready. The carrier is a one-way deployment
package whose useful power, controls and installation hardware become part of the remote
gate, with its cost included in the expedition budget. Initial upkeep is selected below.

The user accepted six server-running flight hours per light-year as the initial balance-testing
rate, freezing each expedition's quoted rate/duration at launch; later tuning affects future
launches. Examples: 2/4/8 light-years take 12/24/48 running hours in flight, plus two hours of
uninterrupted commissioning when otherwise ready. Preparation and interruptions add time.
Authored speed/pacing and unit conversions must realise the selected rate; no settings changed.

The user also selected one protected return crossing for the same ship after entry through
an operational gate, even if its owner later bans the company. This is ordinary gate traffic,
not the one-way delivery carrier. The user accepted the detailed permit model: quote and reserve
outward/return fees together before entry; charge each once at transit commitment. Honour
accepted terms after later price increases/bans; new permits use current policy. Bind the
return to the same ship/pair, without time expiry while it remains in the destination system.
The first departure closes the visit. Return through that pair consumes the right without
creating another; leaving by another route releases unused return funds. Ownership transfers,
detailed repair/compensation and quote validation/fee amounts remain unresolved. The right
does not grant unlimited re-entry or recursively create another return permit.

The user accepted initial per-crossing energy/supply costs, deferring recurring idle upkeep.
At admission the owner reserves actual physical supplies for the protected return until the
visit closes; the ship's fee reservation does not substitute for that stock. Shortages stop
new admissions while preserving return supplies. Genuine equipment outages visibly block
service, preserving rights/funds until repair. Owners may close to new business immediately,
but honour accepted passages and returns; dismantling waits for their discharge. With no
return expiry, retirement can wait indefinitely for a visiting ship. No forced departure,
random breakdown model or safe topology removal is established; detailed repair/compensation,
resource quantities, maintenance and salvage remain design work.

The user accepted independent rival gate pairs into already-connected systems, without
incumbent gate-owner permission. Several independently owned pairs may serve a system,
including parallel links between the same systems. A rival may deliver its endpoint by its
own funded unmanned FTL expedition under the same travel/commissioning rules; an incumbent
gate ban cannot block that flight. Each owner controls its own access/fees. Gate construction
does not bypass protected planets or active resource concessions. Site eligibility/placement,
capacity limits and possible delivery through existing gate networks remain unresolved.

Additional pinned-SDK evidence: public `Gates::DefineStarSystem` rejects redefinition and
`MakeGates` rejects repeat creation for a system. Further endpoints in an existing system
therefore need a demonstrated public lifecycle or minimal reusable Jettison provision, not
private app access or a duplicate loader. The capability map and required consumer/recovery
checks are updated; no engine change or execution evidence is claimed.

No implementation, SDK/environment changes, builds or gameplay validation occurred.
The main stargate lifecycle/access decisions are recorded, with exact materials/prices,
operating/repair recipes, site rules, transfer/recall, quote validation and SDK integration
still pending. Documentation checks: `git diff --check` and 18 local Markdown link/heading
targets across the two changed documents passed. Only this handoff and the stargate plan are
uncommitted; `df12cbf` remains the pushed baseline.

Recommended next planning topic: long-running demand/public-income circulation, including
funded consumer demand, producer income and public-budget replenishment. Those rules remain
unselected; no household model, tax or currency issuance is implied by the gate decisions.
Recommend `/compact` before that topic: the decisions are preserved here, while earlier gate
API excerpts and repeated documentation-check output are no longer needed for the next design.

## Contract, frontier, first-tech and stargate follow-up — 2026-11-15

Resumed from Manifest `feature-base` at `066b5cc`, with a clean working tree. This follow-up
updates supplier security, construction-site, exclusivity, first-tech and stargate decisions in the product-planning
handoff below; its other constraints and decisions remain in force. Current changes are
planning documentation only in this handoff, the contract, planetary-development and
economy/development plans, the development-stage index, roadmap, architecture, gameplay/server
plan and docs index, plus the new stargate-expansion plan.
No implementation, SDK/environment change or build/gate execution occurred.
Documentation checks: `git diff --check` and local Markdown link/heading-target checks passed.

The user accepted a supplier-funded performance bond, initially 10% of the winning offer
subject to balance testing. Publish its requirement with the tender; lock it from unreserved
company funds at award, considering the next qualified bidder if funding fails. Hold it until
contract close while continuing immediate payment for accepted partial deliveries. Complete
delivery returns it all. Abandonment or missed deadline forfeits the undelivered proportion
to the public treasury and returns the rest; accepted goods and payments remain settled.
NPCs and the governor's company use the same rules; disconnecting changes none of them.
See [performance security](procurement-and-station-contracts-plan.md#performance-security-selected).

Whole-credit bond rounding/minimums and issuer/service-failure handling remain design work;
the bond is not a guarantee of replacement-supplier affordability. No economic validation
has occurred.

The user also accepted the [construction-site reservation rules](planetary-development-plan.md#company-built-stations-and-control-of-shipping):
the first eligible company starting a funded project secures the site, paying meaningful
non-refundable mobilisation and reserving construction funds. A fixed completion deadline
cannot be renewed indefinitely through token deliveries. Abandonment/expiry reopens the site;
unspent funds and unused goods remain company property. Completed-work salvage, exact amounts,
funding scope, mobilisation beneficiary, deadline clock/duration and intervening control changes
remain unresolved. The reservation grants construction priority only; resource exclusivity and
government control need separate grants. No anti-squatting or recovery result has been tested.

The user accepted the [exclusivity trigger and provisional term](economy-and-development-plan.md#real-time-exclusive-exploitation):
start when the first eligible frontier spaceport becomes operational and its exploitation
concession activates. Publish the duration before construction; use 72 hours of actual
server-running time as the initial balance-testing value. Shutdown pauses it; idle facilities
and disconnected owners do not. Ownership transfers, rebuilding and government changes cannot
restart or extend it. At expiry rivals may exploit and build their own ports; incumbents keep
their infrastructure and may refuse new bookings at their own port. Protected homes keep
separate rules, and the concession does not transfer government control. Construction costs,
extraction capacity and pacing must validate the term; no such validation has occurred.

The user accepted [Bronze metallurgy](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
as the first material/facility tech example. Require basic smelting knowledge and an operational
local metalworking workshop, seeded explicitly for the separate governance fixture. Deliver
90 kg copper and 10 kg tin into public project inventory, with local copper and competitively
procured/physically imported tin from an explicit external supplier. Consume the project inputs
once, then advance fixed-step work; workshop interruption pauses progress. Completion unlocks
local production, with each fresh batch consuming 9 kg copper plus 1 kg tin into 10 kg bronze.
Knowledge survives workshop loss; production needs an operational workshop. Reuse existing
`SkillSmelting`, `ItemCopper`, `ItemTin` and `ItemBronze` IDs, preserving authored data and the
separate eight-good trading fixture. Work duration, throughput, profitability and detailed
facility/batch terms remain unresolved; the quantities are initial test values. No content,
fixture, code or tests changed, and no gameplay validation ran.

All four initial follow-up decisions are recorded. The user subsequently required government
or corporate funding of physical stargate delivery before opening a new system to trade and
development. Flights are fully automated/unmanned at a multiple of lightspeed, with meaningful
server-running wall time. The user chose distance-based duration, tuned through configurable
FTL speed/pacing to hours or days, and owner control of operational gate access/fees, including
refusing rivals. See the new [stargate handoff](stargate-expansion-plan.md).

Gate delivery/activation does not grant government control or start planetary resource
exclusivity. SDK inspection at the existing pin confirms that `PlotRoute`/`EnterLane` require
an existing lane, while `OnLane` uses simulation-step arrival; they do not establish the
required bootstrap expedition or server-running timing contract. Gate toll refusal/charge
callbacks exist; per-owner policy integration and atomic topology/recovery remain to prove.
Jettison stays unchanged at `0564f19`; the SDK pin and environment are unchanged. Only read-only
source investigation and documentation checks occurred, with no builds or gameplay validation.

Next stargate choices: endpoint pairing/deployment, launch cost/equipment, exact speed/pacing,
gate commitments/exit rights and rival access alternatives when a gate owner refuses service.
Target generation/reveal triggers, detailed construction/colonial/transfer contracts and
long-running demand/public-income circulation remain open. Continue this session while the
physical economy and access constraints are fresh; no context reset is needed yet.

## Current product-planning handoff — 2026-11-15

This section supersedes the earlier planning/repository state below. The planning baseline was
`feature-base` at `8f02d54c2935dc929052a4e753c66bcf9f777a5c`; Jettison is on `feature-base`
at `0564f19ed41bc7e0a278952153567d4a137a2f5b`. Both started clean. The SDK pin remains
`f85092027eda49c29001f7589608df1f0b8827fb`; no SDK/environment changes were made.
The user requested committing and pushing the Manifest documentation at session close;
the resulting planning commit is the commit containing this handoff. Jettison is unchanged.

Current work is Manifest planning documentation only: a new
[planetary development/corporate-power assessment](planetary-development-plan.md),
[supply/station contract handoff](procurement-and-station-contracts-plan.md),
[physical economy/development plan](economy-and-development-plan.md) and aligned
roadmap, gameplay/server, development-stage, architecture, scope, stage 3–5 and index/handoff
guidance. No implementation, assets, fixtures or configuration changed. No builds, gates,
benchmarks, economy simulations or recovery checks ran; build validity is not established.

Confirmed direction to preserve:

- Dedicated headless server; fixed-step world, NPCs, autopilot, enabled auto-trading and
  development progress with zero clients. Server downtime still pauses simulation initially.
- Existing TUI becomes a required remote gameplay client with `connect`/`disconnect`; GUI
  shares the command/query layer and uses selected public engine features. Manual TUI
  spaceport trading is a stretch goal. Administrative permissions remain separate.
- Each player initially directs one planet's development and public budget alongside a
  separate trading company. AI governs others; disconnecting does not relinquish control.
- Acquire more control through eligible AI-planet purchases or frontier settlement; protect
  other players' planets from that acquisition mechanic. Keep initial/late-join starter
  capacity separate from the acquisition pool and an AI economy.
- Endlessly accumulate corporate wealth, power and control, seeking a stranglehold over
  trade, economies, governments and media. There is no win condition or automatic victory
  reset. Days versus months is unresolved server lifespan/pacing, not a victory timer.
- First development controls: projects, investment budgets and public procurement. First
  corporate leverage: infrastructure, essential supplies and access contracts. Powerful
  companies can build/own stations, set fees and shipping priorities, and refuse rivals
  access. Government financing and media mechanics follow later.
- Public procurement requires competitive bids, including the governor's own company.
  Plan a simple government corruption trait and abstract bribery after clean tendering works;
  detailed officials/political simulation is deferred. This supersedes the earlier open
  question about directly awarding public contracts to one's own company.
- Pay suppliers for accepted partial deliveries as they arrive. Honour accepted station
  bookings under their agreed access/fee/priority terms; owner changes affect future requests.
  The new [contract/booking handoff](procurement-and-station-contracts-plan.md) proposes sealed
  bidding, a clean price-based award, cumulative total-contract payment and bounded visits.
  Those mechanics are proposals, not implementation or changes to marketplace settlement.
- Credits move instantly between authorised accounts; goods move physically from extraction/
  industry through haul, station warehouse, ship, destination warehouse and local delivery.
  Brokers trade their own finite funds/stock and earn spreads; warehouses charge handling
  separately at both ends. Production limits new output, stock limits offers, scarcity/demand
  determine prices; explicit production-based price bands were not selected.
- Planet tech advancement needs exploited/imported materials and actual production facilities.
  Generate higher-tech/resource-depleted inner worlds and richer, less developed outer worlds.
- Frontier resource rights are initially exclusive for elapsed real server-running time,
  pausing during shutdown. Afterwards rivals may exploit and build their own spaceports even
  under the incumbent's puppet government. Starting homes remain protected. This supersedes
  the permanent no-second-port rule on reopened concessions. The operational-port/concession
  trigger, provisional 72-hour term and construction-site reservation are selected in the
  follow-up above.
- Corporations may build a very expensive orbital spaceport at any planet without one that
  is not owned/controlled by another player. This enables resource exploitation and colonies
  with company-controlled puppet governments, supporting long-term corporate returns.
  Resource/colonial authority and control of a pre-existing AI government still need design.
- Later frontier systems extend the existing map with generally less developed and some
  uninhabited worlds. Terraforming, settlement, expansion triggers and capacity are unproven.
- Keep player/company/planet/civilisation/control identities and public/company finances
  explicit. Retain price-only Decimal, floating continuous quantities and whole-credit ledgers.
- Preserve genre-neutral engine provision, public SDK boundaries, pinned dependency/environment
  parity, existing YAML/archive/VFS/registry mechanisms and the OpenAI planning-only policy.

Proposed delivery amendment: retain the five foundation plans as work packages, but prove a
real TUI connection after minimal authority; then idle physical trading, governed development
and recovery. Multi-planet acquisitions, specific government/media influence and frontier
expansion follow. The tiny two-venue fixture remains useful; add a separate governance fixture.
The old optional administration-only TUI and permanent AI-only development assumptions are
superseded. The exact extended implementation schedule is still a planning proposal.

Inspection found no implemented networking/session layer in Jettison. System registration,
route plotting and saves exist, but do not establish atomic live topology expansion.
Physics system definitions are not saved/hashed; generated topology must be restored through
a versioned world-data contract before loading entities/transit. See the new capability map.
Docking lacks generic admission/owner-priority hooks; normal ship authority checks are not
station entry permission. Reusable policy seams belong in Jettison, corporate rules in Manifest.

Documentation validation for this update: `git diff --check` passed, and a read-only check of
14 changed Markdown documents found no missing targets among 114 local links and two heading
anchors, no trailing whitespace and no missing final newlines. Jettison remains unchanged.

The running-time host bridge must persist monotonic elapsed observations with authoritative
state/journal batches and record expiry events for deterministic replay. Do not use an
absolute UTC deadline that deducts offline time or read a clock from deterministic systems.
Specify checkpoint loss after a crash rather than promising exact crashed-process uptime.

Next planning cycle, updated by the follow-up above: supplier security and construction-site
reservation rules, exclusivity trigger/provisional term and first material/facility example
are selected; continue with long-running demand/public-income circulation. Station booking
protection, partial payment and post-exclusivity rival
ports are approved; detailed settlement, service windows, corruption scoring, colonial powers
and long-running demand/public-income circulation remain design work. Initial starter
protection/viability still needs explicit allocation policy; unassigned AI planets with no
ports are eligible for initial corporate construction under the rule.
Acquisition pricing, influence rules, abandonment policy, frontier triggers and server pacing
remain open. Implementation/build/gate work belongs to the selected implementer; no such work
is authorised for OpenAI agents. The user ended this planning session. Start a fresh session
next time using this handoff and the linked plans; decisions are preserved and earlier
repository excerpts are no longer needed. No implementation or gameplay validation is complete.

## Earlier foundation-planning handoff — superseded state

This section supersedes the historical resume instructions below. Stages 1–5 have
implementation plans, not completed implementations. OpenAI agents remain planning-only;
implementation and build/gate execution belong to Claude under the user's policy.

- Manifest is on `feature-base` at `2716380` (`Merging in WIP. Not sure of build status.`).
  The working tree was clean before this planning session; current build validity is unknown.
- Jettison is on `feature-base` at `826c4bec125b6631a6ba8ee822c90463316c3511`, initially clean.
  Manifest still pins `f85092027eda49c29001f7589608df1f0b8827fb`; the committed difference
  from that pin is only `.gitignore`. Jettison now has the uncommitted documentation below.
- Uncommitted Manifest work: five new `docs/stage-{1..5}-*-plan.md` files and
  `docs/engine-game-boundary-assessment.md`; updates to `AGENTS.md`, architecture,
  agent workflow, roadmap, development stages, standalone/host-script plans and this handoff.
- Uncommitted Jettison work: new `docs/game-integration.md`; updates to `CLAUDE.md`
  (`AGENTS.md` is its existing symlink), README/docs index, scope, architecture, roadmap,
  sample-game, coding standards and build/gates docs. Both repositories now instruct
  implementers to inspect Manifest requirements and provision reusable SDK gaps while
  retaining a separate small example for testing. No code, configuration or assets changed;
  no commits were made.
- Read [development stages](development-stages.md) for the index and execution order.
  Begin with [stage 1](stage-1-host-plan.md), including merged-baseline validation.
- User correction: retain `lazygit` as a development-machine tool for terminal change
  review. It is not a runtime requirement. Reconcile environment parity through the
  shared SDK development shell; do not simply remove it from Manifest.
- User direction: replace Manifest's custom YAML readers with Jettison's YAML archive,
  migration and definition mechanisms. Stage 2 specifies this migration; no parallel
  legacy runtime deserialiser is planned.
- User direction: Jettison must provide genre-neutral capabilities; Manifest owns its
  production gameplay and content. Engine-local gameplay for automated testing is allowed.
  The [boundary assessment](engine-game-boundary-assessment.md) finds partial separation
  and adds sample isolation, neutral consumer/build checks and optional space-module
  extraction to the stage plans. Do not remove useful engine gameplay tests or assume
  that moving files alone establishes separation.
- Preserve price-only Decimal, floating continuous quantities and whole-credit balances.
  Stages 3–4 specify company ownership, venue-local inventory and one authoritative world.
  Stage 5 proposes durable step batches and recovery; none of its guarantees is validated yet.
- Validation in this planning session: read-only source/API inspection, `git diff --check`
  in both repositories, and 157 local Markdown link-target checks with no missing targets.
  No builds, gates, benchmarks or crash/recovery tests ran. Detailed plans contain required
  implementer checks and explicitly identify proposed choices and remaining decisions.

Recommended next context: start a fresh implementation session with Claude using this
handoff and stage 1. The plans retain the decisions; the exploratory source excerpts
in the planning conversation are no longer needed for the first delivery slice.

## Historical handoff — superseded repository state

Prepared for `/clear` on 2026-11-15. Read `AGENTS.md`, `docs/coding-standards.md`,
`docs/architecture.md`, `docs/build-and-gates.md` and `docs/agent-workflow.md` before editing.

## Repository state

- Workspace: `/home/ivan/projects/manifest`.
- Branch: `feature-scaffold`.
- HEAD and `origin/feature-scaffold`: `5d7f54e84b854cbcc9ff43a3b133bc592c639bc6`,
  `Market prices: use decimal values while preserving floating quantities`.
  Commit and push succeeded before finalising this handoff.
- Previous baseline: `11de8f3`, shared-console TUI integration.
- GitHub reports the repository moved to `git@github.com:ivanhawkes/manifest.git`. The
  existing origin URL still names `civilisation-simulation.git`; its redirect accepted the push.
  The remote URL was not changed.
- Roadmap item 1 is committed as `09e01d8`; item 2 is committed and pushed as `5d7f54e`.
  All implementation and project documentation changes are committed. The only uncommitted
  file is this handoff, `docs/session-handoff.md`; keep it for resuming after `/clear`.
- SDK: Jettison `feature-base`, pinned at `f85092027eda49c29001f7589608df1f0b8827fb` in
  `source/external/CMakeLists.txt`. The sibling `/home/ivan/projects/jettison` matches the pin.
  Development environment files and SDK dependency definitions were not changed.

## Decisions that must survive the clear

The user's explicit correction was: **only market prices use decimal types; physics and other
continuous values use floats/doubles.** This supersedes the original all-integer economy plan.
Existing whole-credit balances, reservations, gross amounts and fees remain integers.
Quantities, population, production/demand, effect stats, ratios and freight/profit estimates
retain doubles. The abandoned broad fixed-point migration was reverted; do not reinstate it.

[Host/script boundary](host-script-boundary.md): authoritative economy, progression, effects
and settlement run in linked native game modules, eventually in one Jettison `SimWorld`.
Wasm supplies bounded policy inputs, starting with trade preference; initial event scripts
observe only. Reuse Jettison's VFS, definition registry, reflection, utility AI, command/replay,
save and script infrastructure. There is no native C++ mod ABI. Mounting content is not yet
playable integration.

[Decimal market prices](market-prices.md) records the implemented numeric contract:

- `Manifest::Core::Decimal`, in `source/game/types/include/manifest/types/Decimal.h`, stores
  signed 64-bit millionths of a credit per authored item unit. Integral construction means
  whole credits; `FromRaw` means microcredits; `_dec` literals parse decimal text exactly.
- Only base/market/local prices, route unit prices, order limits, venue minimum price,
  quote prices and fill unit prices use it. Authored IDs, YAML keys, units and prices are unchanged.
- `TryParse` rejects malformed, nonfinite, exponent, excessive nonzero precision and out-of-range
  text. `Generate` validates nonnegative base prices before replacing a world, with headroom
  for the maximum local multiplier. Configuration still retains authored price strings.
- Scarcity/local coefficients remain double. `TryFromDouble` explicitly rounds computed prices
  to the nearest microcredit, halfway away from zero. Price comparison and levels are exact.
- Mixed quantity/price settlement explicitly uses `price.ToDouble() * quantity`, preserving
  existing double-product rounding. Market buy reservations ceil whole credits, fill gross
  rounds nearest, and fees ceil. The separate development simulation still truncates route
  costs/revenue. No microcredit balances or cumulative per-order rounding were introduced.
- Checked cost/fee conversion and ledger preflight prevent overflow before writes. Cancellation
  and expiry batches preflight combined refunds and process stable order IDs. Representable
  high fee rates are preserved; there is no new silent fee cap.

## Committed implementation

New files: `docs/market-prices.md`, the Decimal header and implementation under
`source/game/types/`, and `source/game/types/tests/DecimalTests.cpp`.

Modified code: price fields in ECS components; simulation and market APIs/implementations/tests;
their CMake dependencies; the types module/test registration; data assertions and the shared
test helper; and TUI price display types. Effects implementation and physical representations
are unchanged. Documentation, including the roadmap and coding-standard override, is updated.
Use `git show --stat 5d7f54e` for the implementation file list.

Review findings about double-product rounding, uncapped fees and batch refund atomicity were
fixed and re-reviewed with no remaining material findings. New tests cover exact authored and
adjacent prices, invalid-price rollback, rounding, overflow, refunds and conservation.

## Validation

The complete local gate finished successfully:

```bash
devenv shell -- env CMAKE_BUILD_PARALLEL_LEVEL=2 CTEST_PARALLEL_LEVEL=2 \
  ./scripts/ci.sh --werror --asan --log build/gate-prices.log
```

Formatting and all **11 suites passed in Debug, Release and ASan/UBSan**. No local clang-tidy
was run. The log is an ignored build artifact, not a committed fixture. `git diff --check` and
local Markdown link checks also passed. Subsequent changes only marked completion in docs.

Original simulation state hashes remain unchanged: generation `10355632731109243635`,
100 ticks `12410383246888723003`. The new raw-price hash is `15444164946577136117` at both
points; current prices depend on unchanged production/demand rates. All presets agree.

Use the locked devenv shell for builds. The ordinary tool shell lacks compiler/Python commands;
`.devenv/profile/bin/python3` and `.devenv/profile/bin/clang-format` work for small local tasks.
Stale compiler paths initially forced a sanitizer cache refresh and dependency rebuild; that
is resolved. The final sanitizer cache has Debug, `-fsanitize=address,undefined`,
`-fno-sanitize-recover=all` and `MANIFEST_WERROR=ON`. Do not repeat the full gate merely to
reconstruct conversation context; rerun checks when new changes warrant them.

## Next work

Roadmap items 1 and 2 are complete under the corrected price-only scope. The next implementation
task is item 3: replace host-path configuration loading with Jettison VFS and typed definition
registry loads, including layered patches, schema versions, migrations and directory/pack tests.
Current loaders still take host paths. Fixed-step host state/commands/saves remain item 4;
scripts/assets and a playable engine-hosted scenario remain item 5.

Start item 3 from the committed, validated price patch. Delegate bounded inventories when useful,
keep design/integration with the primary, use at most two workers, schedule builds only from the primary with two
compile jobs total, and avoid local clang-tidy. No worker from the previous session is needed.

Suggested resume prompt:

> Continue Manifest on feature-scaffold at 5d7f54e (pushed). Read AGENTS.md and
> docs/session-handoff.md. Roadmap items 1 and 2 are complete and validated.
> Only market prices use Decimal; quantities, physics and other continuous values remain
> floats/doubles, with existing whole-credit ledgers unchanged. Continue roadmap item 3
> using Jettison's existing VFS/definition infrastructure.
> Avoid local clang-tidy and limit builds to two jobs.
