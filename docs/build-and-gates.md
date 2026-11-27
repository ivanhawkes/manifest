# Build and gates

How to build Manifest, run its suites, pass the gate before a push, and stage the distribution.
Everything runs inside `devenv shell` or the shell direnv loads, which provides clang 21,
clang-format, clang-tidy, CMake, Ninja and yq. Much of the mechanism follows Jettison's
[build and gates](../../jettison/docs/build-and-gates.md); this page says how Manifest's differs.

## Quick start

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug                                   # every suite
ctest --preset clang-debug -R '^console$'                    # one suite
./scripts/ci.sh --werror --asan --tidy --log build/gate.log  # the gate, before a push
```

## The SDK and the shell

Manifest builds against one Jettison commit, the [pin](glossary.md#pin):
`MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`. Configuring fetches that commit
into `build/<preset>/_deps/jettison-src`. To build against a local engine instead, configure with
`-DJETTISON_SOURCE_DIR=/path/to/jettison`; that is development evidence only, never a gate.

The shell is the engine's: `devenv.nix`, `devenv.yaml` and `devenv.lock` are byte for byte those
of the pinned commit ([D-006](decision-ledger.md#d-006), [D-054](decision-ledger.md#d-054)). A
pin move copies all three from that commit. `scripts/check-env-parity.sh` compares them against
the pin's committed files and never reads `JETTISON_SOURCE_DIR`; `scripts/test-env-parity.sh`
tests the checker itself. CI runs both first.

Beside the engine, `source/external/CMakeLists.txt` fetches one file for the client: Noto's
`NotoSansJP-Regular.otf` and its `LICENSE`, each by URL and SHA-256, never committed
([the Japanese font](client.md#languages)). A new font or other fetched file is pinned the same way and its licence read in the source repository's own file.

The engine is added with `EXCLUDE_FROM_ALL`: configuring loads its whole SDK, but a build compiles
only the engine modules Manifest links, plus the packer. `source/external/CMakeLists.txt` is also
the import boundary: configuring fails, naming each one, unless every engine target and helper
Manifest uses exists, whether the SDK was fetched, given by `JETTISON_SOURCE_DIR` or found in a
parent project. Do not configure Jettison's install or export path, which it does not support.

## Presets

Each preset has its own build tree under `build/<preset>/`, laid out as the distribution is: the
executables in `bin/` and the packed content in `share/manifest/manifest.pack`. So
`build/clang-debug/bin/manifest_tui` runs on its own build's pack.

| Preset | What it is for |
| --- | --- |
| `clang-debug` | Debug, with the tests and the TUI. The everyday build and the compile database for clangd and clang-tidy |
| `clang-notui` | Debug with `MANIFEST_BUILD_TUI=OFF` and the engine's `JETTISON_PRESENTATION=OFF`: proves the game's commands and queries build and pass without the terminal UI or the engine's renderer, UI and developer console |
| `clang-release` | Release with `MANIFEST_DISTRIBUTION=ON`: the build that is shipped ([the distribution](#the-distribution-and-the-bundle)) |
| `clang-asan` | Debug with the address and undefined-behaviour sanitisers, no recovery |

A top-level `build/` holding `CivSim*` files is an ignored tree from before the project's rename.
It is not a current build; remove it.

### Build options

| Option | Default | Effect |
| --- | --- | --- |
| `MANIFEST_BUILD_TESTS`, `BUILD_TESTING` | on at top level | Either off omits the suites. Off when another project consumes Manifest |
| `MANIFEST_BUILD_TUI` | on at top level | Off omits the TUI and FTXUI. An SDK without presentation also omits the development console, which is built on the engine's |
| `MANIFEST_WERROR` | off | Makes project warnings fatal. The gate's `--werror` sets it |
| `MANIFEST_DISTRIBUTION` | off | Links the shipped executables to run outside the build's toolchain (`cmake/Distribution.cmake`) |
| `MANIFEST_CCACHE`, `JETTISON_CCACHE` | on | Off disables the compiler cache |

The presets set the test and TUI options on, so the gate builds that configuration only. C++
module scanning is off (`CMAKE_CXX_SCAN_FOR_MODULES`), since the project uses headers; this keeps
the compile commands usable by clangd and clang-tidy as they are.

### Content in the build

`data/build.sh` turns the authored YAML under `data/` into the engine's definition records, with
mikefarah/yq v4 (the shell's is 4.54.1). The build writes them into the build tree, then stages the
mod and packs it; the pack is validated with each scenario before the suites that read it run.
Without yq, configuring fails when the tests are enabled; with `-DBUILD_TESTING=OFF` the C++
libraries build, but packing still needs yq. Run by hand, `bash data/build.sh` writes to the
source tree's `data/definitions`, and `bash data/build.sh /absolute/path` to the directory given.
How content is authored and checked is in [content](content.md).

## Suites

The suites are CTest tests, named for what they test. `ctest --test-dir build/clang-debug -N`
lists them: 67 in Debug. A module's unit tests live in its `tests/` directory; the cross-module
suites live in `source/test/`.

| Suites | What they test | Source |
| --- | --- | --- |
| `types`, `universe`, `progression`, `sim`, `ecs`, `market` | The [sandbox](sandbox.md) modules. `sim` pins the generated and replayed state's hash | `source/game/<module>/tests` |
| `space`, `content`, `world`, `command`, `travel`, `trade`, `company`, `procurement`, `policy`, `query`, `host`, `remote` | One per [hosted world](glossary.md#hosted-world) module | `source/game/<module>/tests` |
| `effects` | Effects applied across modules | `source/test/EffectsIntegrationTests.cpp` |
| `client_model` | The graphical client's model: the first loop played through it, Ore to Brine, with no window | `source/apps/client/tests` |
| `client_screens`, `client_strings`, `client_smoke` | The client's screens loaded with no problem in each language, every string key in all four languages with English's placeholders, and the engine's render test played by the client offscreen on lavapipe. Not in `clang-notui` | `source/apps/client` |
| `fixture` and `fixture_cruise`, `_alone`, `_prices`, `_burn`, `_traffic`, `_governance`, `_development` | Whole scenarios played through the host, from the content directory and the pack. The seven long cases are suites of their own so they run side by side; `fixture` runs every other case | `source/test/FixtureTests.cpp` |
| `data`, `packaging`, `mod` | The generated records, the packer's refusals, and the staged mod and pack mounted through the engine's VFS | `source/test/` |
| `sdk_import` | Configures Manifest under a parent with stand-in SDK targets, complete and partial, without fetching or building | `source/test/sdk-import` |
| `cli_*`, `validate_*`, `token_*` | `manifest`, `manifest-validate` and `manifest-token` run from an unrelated directory with no display: both content forms start, and bad command lines fail. `cli_serve` starts the server with no client, kills it, restarts it from its journal, stops it with SIGINT and restores its backup (`serve-smoke.sh`) | `source/apps/manifest/CMakeLists.txt` |
| `bundle_smoke` | The build tree run as a distribution is, every place at its default, in a new HOME (`scripts/bundle-smoke.sh`) | `source/apps/manifest/CMakeLists.txt` |
| `console` | The development console's commands, completion, history, errors and the load, generate, tick and trade-planning workflow. Not in `clang-notui` | `source/tools/console/tests` |
| `step-budget` | One [console](console.md) case, run alone: a cruising journey through the console's long step, timed. Not in `clang-notui` | `source/tools/console/tests` |
| `distribution` | `clang-release` only: `scripts/check-distribution.sh` on the shipped executables | `CMakeLists.txt` |

`step-budget` times only in an optimised build without sanitisers, and not on CI, so in practice
in `clang-release` on Lythir, the machine its figure was measured on. It fails when the journey
takes over half as much again as that figure (`kJourneySeconds`). A change that makes the step
dearer on purpose measures it again and moves the figure.

## The gate

The [gate](glossary.md#gate-ci) is `scripts/ci.sh`: the local check every commit passes before a
push. CI does not run it ([CI](#ci)). Its stages, in order:

1. The format check (`scripts/format-check.sh`, clang-format 21 over `source/`).
2. `clang-debug`: configure, build, CTest.
3. `clang-notui`: the same.
4. `clang-release`: the same, unless `--skip-release`.
5. `clang-asan`: the same, with `--asan`.
6. clang-tidy over Manifest's own units in `clang-debug` (`scripts/tidy.py`), with `--tidy`.

Tidy runs last, so a finding fails the gate without hiding the results of the stages before it
([D-052](decision-ledger.md#d-052)). A stage that fails stops the gate. The full gate before a push
is `--werror --asan --tidy`; anything less is a partial run, and is reported as one.

### `ci.sh` options

| Flag | Effect |
| --- | --- |
| `--werror` | Configures every tree with `MANIFEST_WERROR=ON` |
| `--asan` | Adds the `clang-asan` stage |
| `--tidy` | Adds clang-tidy |
| `--skip-release` | Leaves out `clang-release`, for a shorter local pass |
| `--log FILE` | Writes the whole output to the file as well |
| `--force` | Runs even when the inputs are those of a gate that passed |

### A gate that passed is not run again

As in [Jettison](../../jettison/docs/build-and-gates.md#a-gate-that-passed-is-not-run-again), a
gate whose inputs match one that passed exits 0 at once. The key is a digest of the working
tree's files less `*.md`, the versions of clang, clang-format, clang-tidy and CMake, and the
options, recorded under `build/gate-passed` only if it is unchanged at the end of the run. The
engine enters the key by its pin, so a tree configured with `JETTISON_SOURCE_DIR` runs every time
and is never recorded.

### clang-tidy

`scripts/tidy.py <build directory>` runs clang-tidy with the checks in `.clang-tidy`, every
warning an error, on the units under `source/` alone. It skips a unit that was clean and whose
compile command, includes (from `ninja -t deps`), clang-tidy and `.clang-tidy` are unchanged since;
the record is under `<build>/tidy-clean`. So a gate after a small change pays for little tidy. It
is opt-in because it is expensive on the laptop; on Lythir every gate before a push runs it.

### Machine-specific concurrency

Concurrency is set per machine ([D-051](decision-ledger.md#d-051)). `ci.sh` and `tidy.py` default
to two workers each, the laptop's limit. On Lythir (12 cores, 24 threads) pass the machine's width:

```bash
CMAKE_BUILD_PARALLEL_LEVEL=24 CTEST_PARALLEL_LEVEL=24 MANIFEST_TIDY_JOBS=24 \
  ./scripts/ci.sh --werror --asan --tidy --log build/gate.log
```

The three settings are independent. Read `MemAvailable` first: the memory a full-width run needs,
and the user's local AI that can hold most of it, are in Jettison's
[machine-specific concurrency](../../jettison/docs/build-and-gates.md#machine-specific-concurrency).
Run nothing else beside a build or clang-tidy.

### Lanes

Work happens in three kept worktrees beside the engine's three. The primary checkout,
`~/projects/manifest`, stays on `feature-base` for merges; each task is written and checked in
`manifest.lane-b` or `manifest.lane-c`, cut from `feature-base` and merged back by fast-forward, so
one lane can wait on a cross-review while the next task starts in the other. Work runs in batches
across both repositories' lanes: a piece is ready to merge when it builds and its suites pass, and
the gate, tidy and the format check run once on `feature-base` after the batch is merged, before a
push (Jettison's [batches](../../jettison/docs/build-and-gates.md#batches)). The engine's
`scripts/lanes.sh` lists both repositories' lanes, and a task's plan warns of any clash with the
other lanes' work before it begins. A lane builds
against the engine's pin, or against the engine lane of its letter with
`-DJETTISON_SOURCE_DIR=~/projects/jettison.lane-<letter>`. `scripts/heavy.sh <command>` holds the
lock every lane of both repositories shares, and `ci.sh` takes it itself, so builds, suites and
gates take turns. How the lanes are kept is in Jettison's
[lanes](../../jettison/docs/build-and-gates.md#lanes).

### Gating beside the work

On Lythir a commit can also be gated in a second checkout of its own lane, so the next change can be
written while the gate runs. `build/gate-tree` is a detached worktree with its own build trees,
made once with `git worktree add --detach build/gate-tree <commit>`. For each commit gated, run
`git -C build/gate-tree checkout --detach <commit>`, then `build/gate-tree/scripts/ci.sh` with the
gate's options. While it runs nothing else builds, runs tests or runs clang-tidy (the user's rule,
2026-11-21), so the work beside it is editing alone. A failure is fixed by a later commit, before
the push.

## Verification evidence

A gate is recorded, not implied. A task's record in [history](history.md) names the commit, the
presets and stages that ran, and the log path. Gate logs are build artefacts under `build/`,
never committed. A successful configure, equal mounted content or a documentation edit is not
verification, and a partial run is not a gate ([D-007](decision-ledger.md#d-007)).

Report CI from the exact commit's run and its fetched log, apart from local evidence. Confirm any
run ID or commit SHA against a second source (`gh api`, `git ls-remote`, the remote's commit
list) before writing it down: tool output here has invented both
([the 2026-11-16 analysis](history/project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16)).

## CI

CI is not the gate ([D-116](decision-ledger.md#d-116)): the full gate ran past the runner's hour on
its two workers. `.github/workflows/ci.yml` shows instead that a clean checkout builds the
distribution and that the bundle runs on a plain Ubuntu. On an Ubuntu 24.04 runner, within 45
minutes, it:

1. checks the shell's parity (`test-env-parity.sh`, then `check-env-parity.sh`), before Nix;
2. installs Nix and devenv 2.4.0;
3. in the shell, stages the bundle (`scripts/stage-bundle.sh`: the release build, its
   `check-distribution.sh`, the tarball);
4. outside the shell, runs it on the runner's own system (`scripts/test-bundle.sh <tarball> host`);
5. uploads `build/ci.log`.

It runs on each push to `feature-base`, each pull request targeting it, and by hand
(`gh workflow run ci --ref <branch>`). A push to `main` or another branch runs nothing.

### Promoting to main

`feature-base` is the integration branch; `main` only fast-forwards to a commit already verified
there ([D-053](decision-ledger.md#d-053)). `scripts/promote.sh [--check] [commit]` does it, by
default to `origin/feature-base`; `--check` stops before the push. It refuses a commit that does
not descend from `origin/main` (integrate on `feature-base` and verify there), one not on
`origin/feature-base`, and one without a completed, successful CI run of its own from a
`feature-base` push. An ancestor's run, a pull request's or a manual run does not count; a
cancelled run is re-run from Actions.

The script is the procedure, not an enforcement. The repository's one rule is the ruleset "main:
fast-forward only", which blocks force pushes to `main` and its deletion. `feature-base` has none.

## The distribution and the bundle

`clang-release` is the build that ships as a [bundle](glossary.md#bundle): `manifest`,
`manifest-token`, `manifest-validate` and `manifest_tui`, linked to load through
`/lib64/ld-linux-x86-64.so.2` with `libstdc++` static and no `RUNPATH`. Its tests therefore need a
loader at that path: on NixOS, nix-ld. Its `distribution` test fails on any symbol newer than the
[glibc floor](glossary.md#glibc-floor), 2.39 ([D-114](decision-ledger.md#d-114)); a newer symbol
the toolchain brings is bound to an older version in `source/apps/distribution/GlibcFloor.cpp`.
What the bundle holds and where it writes is in [the host](host.md).

```bash
./scripts/stage-bundle.sh                       # from a clean checkout, into build/bundle
./scripts/test-bundle.sh build/bundle/manifest-*.tar.gz            # here, Ubuntu 24.04, Debian 13
./scripts/test-bundle.sh build/bundle/manifest-*.tar.gz host       # here only, as CI does
```

`stage-bundle.sh` refuses a checkout with changes, since a bundle names its commit.
`test-bundle.sh` unpacks the tarball into a read-only directory and runs `bundle-smoke.sh` on it
under `env -i`, on this machine and in the containers named (Docker).

## Cross-review

At a [stage](glossary.md#stage)'s close, Codex reviews its commits adversarially
([cross-review](glossary.md#cross-review); Jettison's
[cross-review](../../jettison/docs/build-and-gates.md#cross-review) describes the method). Manifest
has no script for it: each round's prompt is written by hand and Codex run directly, and the
prompt, the tree's state before, the review, the transcript and the exit status are kept under
`build/reviews/<stage>/` as `round-N.prompt`, `.before`, `.md`, `.log` and `.exit`. The tree
is checked unchanged after a round. Three rounds by default; the user settles what is still open.

## Editors

The tracked examples are `.vscode/tasks.json.example`, `.vscode/launch.json.example` and
`.vscode/settings.json.example` ([D-055](decision-ledger.md#d-055)); the personal copies without
`.example` are ignored.

### Editor setup

```sh
cp .vscode/tasks.json.example .vscode/tasks.json
cp .vscode/launch.json.example .vscode/launch.json
cp .vscode/settings.json.example .vscode/settings.json
devenv shell -- code .
```

Start the editor from the shell, so the tasks find `cmake`, `ctest` and clang on `PATH` and
inherit the concurrency settings. The tasks are shell tasks on the `clang-debug` presets:
`CMake: configure`, then `CMake: build` (the default build task), then `CTest: test` (the default
test task). Both launches build first:

| Launch | Program | Terminal |
| --- | --- | --- |
| Simulation unit tests | `build/clang-debug/source/game/sim/manifest_sim_tests` | the debug console |
| Development TUI | `build/clang-debug/bin/manifest_tui` | the integrated terminal, which the full-screen UI needs |

The debugger is CodeLLDB (`vadimcn.vscode-lldb`) with the shell's LLDB 21, which the settings
example names as `lldb.library`; it also points the C/C++ extension at the shell's clang. On NixOS
CodeLLDB's adapter is a generic Linux binary, so the system needs `programs.nix-ld.enable = true`;
without it the adapter fails with "Could not start dynamically linked executable".

## Checks not yet built

- A Markdown link and heading-anchor check over `docs/`, `README.md` and `AGENTS.md`. A checker
  is in the analysis's
  [appendix](history/project-analysis-2026-11-16.md#appendix-documentation-link-and-anchor-checker),
  not yet in `scripts/` or CI.
- The CMake dependency-closure checks of the
  [boundary assessment](history/engine-game-boundary-assessment.md#enforcement-to-implement-later).
