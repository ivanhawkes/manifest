# Stage 1: the host and the SDK import (2026-11-16 to 11-18)

Task records as they were written at the time, newest first. Moved from `docs/history.md` on
2026-11-25; the [history index](../history.md) lists every record.

## Completed prerequisites

[T1/T9](planning-2026-11-15.md#t1t9-local-verification--2026-11-16) established the
model-library baseline and fixed the two tidy findings. Do not repeat them as open
tasks.

## Status, 2026-11-18

Done: M01–M04, H01, H06 and H07 in Manifest ([session handoff](../session-handoff.md)); H02–H05, and
the Jettison rows A01 and W01 ahead of their place in this order, in Jettison
([history](../../../jettison/docs/history.md#neutral-sdk-and-manifest-integration)). **Stage 1 is
closed** (H07): the SDK pin is `e53681e`, which answers the engine cross-review of H02–H05, A01 and
W01, and the acceptance matrix passed against its fetch with no sibling checkout. C01 is done
(2026-11-18): the [schema contract](stage-2-content-plan.md#schema-contract-c01-2026-11-18) names
no Jettison gap, and its open authoring decisions gate C04 and C05. C02 is done (2026-11-18):
the definitions load through the engine registry beside the legacy loaders, which they reproduce,
and the content manifest is defined; no Jettison gap. C03 is done (2026-11-18): the host, the
console and the data suite load one validated content set through the engine, and the legacy
loaders and merged documents are retired; no Jettison gap. C04 is done (2026-11-19): the user selected its
authoring values ([D-059](../decision-ledger.md#d-059)); eight items, three hull definitions on
Jettison's map fields (pin `4d60ea0`) and T4's balance record are authored and validated. C05 is
done (2026-11-19): drinking water is authored ([D-060](../decision-ledger.md#d-060)), and the trading
and governance [scenarios](stage-2-content-plan.md#scenario-and-frames) are validated, starting
access and the Bronze project's references included; no Jettison gap. C09 is done (2026-11-19): the
shared ship and station definitions ([D-062](../decision-ledger.md#d-062)) are authored as Jettison's
own records and checked as the engine checks them, venues name a station layout, and starting
access asks the engine's fit rule, provisioned without a world in pin `15e2561`. C06 is done
(2026-11-19): `manifest-validate` takes the host's options ([D-070](../decision-ledger.md#d-070)),
the package, ordered overlays named by their `mod.yaml` and a required scenario, and loads what the
host loads with no world, through the engine's package rules provisioned in pin `30ea3f3`. C07 is
done (2026-11-19): the build validates the staged directory with each scenario, packs it, and
validates the pack against the directory's manifest to the byte; no Jettison gap. C08 is done
(2026-11-19): **Stage 2 is closed**, its checks each held by a gated test
([delivered](stage-2-content-plan.md#delivered-c08-2026-11-19)); no Jettison gap. Next: Codex's
cross-review of the stage's commits, then A02.

- Jettison's G01, a generic rendered shell with the space gameplay moved to its sample, is done
  (2026-11-18, [history](../../../jettison/docs/history.md#neutral-sdk-and-manifest-integration)),
  with a game of no space run in its smoke test. It did not gate Stage 1; L15's graphical client
  stands on it.

A01 and W01 were built against these plans before C01 settled the planet, region and company
identities. C01 confirmed that a Jettison frame (`Sim::InFrame`, the star system) and a save's
details (`SaveInfo::details`) still fit, unchanged. M05 is done (2026-11-18, [session handoff](stage-1.md#m05-the-editor-examples--2026-11-18)); its
launches moved to CodeLLDB under NixOS `nix-ld`, and the user confirmed them from the editor
(2026-11-19).

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

T7 as [planned](plan-notes.md#t7-selected-editor-example-plan), with the setup in
[build and gates](../build-and-gates.md#editor-setup). The examples' tasks are shell tasks on the
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
`manifest --content <directory-or-pack> --check-startup`. [Architecture](../architecture.md) has the
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
`feature-base` and manual dispatch ([D-053](../decision-ledger.md#d-053)).
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
moves the tidy stage after the Release and ASan/UBSan stages ([D-052](../decision-ledger.md#d-052)).
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
