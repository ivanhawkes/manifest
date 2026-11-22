# Build and gates

Run inside `devenv shell` or the direnv-loaded shell. The SDK source pin is
`MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`, and the shell's three files
match it ([T5](#t5-selected-environment-parity-plan)). To test a local engine,
configure with `-DJETTISON_SOURCE_DIR=/path/to/jettison` on `feature-base`.

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
./scripts/ci.sh --werror --asan --tidy --log build/gate.log
```

Presets are `clang-debug`, `clang-notui`, `clang-release` and `clang-asan`. Each has its own build
tree, generated data and staged mod. `clang-notui` is Debug with `MANIFEST_BUILD_TUI=OFF` and the engine's
`JETTISON_PRESENTATION=OFF`, which holds the game's command and query coverage to a build without
the terminal interface or the engine's renderer, UI and developer console. The gate
runs formatting, Debug build and CTest, the same for `clang-notui`, Release build and CTest, and
optionally ASan/UBSan build and CTest. `--skip-release` supports a shorter local pass;
CI runs the full gate. `CMAKE_BUILD_PARALLEL_LEVEL`, `CTEST_PARALLEL_LEVEL` and
`MANIFEST_TIDY_JOBS` control concurrency. Clang-format must be version 21.

A gate that passed is not run again on the same inputs, as Jettison's is: a digest of the working
tree's files (tracked or untracked and not ignored, `*.md` aside), the versions of clang,
clang-format, clang-tidy and cmake, and the options (the presets set every build option a stage
reads, so no cached setting escapes the digest), kept under `build/gate-passed` when it is the same at the end of
the run as at the start; `--force` runs it anyway. The engine is in that digest by its pin, so a
preset configured with `JETTISON_SOURCE_DIR` runs every time and is never kept. `scripts/tidy.py`
skips a unit that was clean and whose compile command, includes (`ninja -t deps`), clang-tidy and
`.clang-tidy` are unchanged since, recorded under `<build>/tidy-clean`.

The CTest suites are `types`, `config`, `universe`, `progression`, `sim`, `ecs`, `market`,
`host`, `content`, `effects`, `remote`, `data`, `packaging`, `mod`, `fixture` (its longest cases
apart, as `fixture_cruise`, `fixture_alone`, `fixture_prices` and `fixture_burn`, so the five run side by side) and `sdk_import`, plus `console` when the TUI is enabled. The
`cli_*` tests run the `manifest` executable from an unrelated directory with no display: both
content forms start, and a missing source and bad command lines fail. `cli_serve` runs the server
with no client, checks that it stepped, and stops it with SIGINT; the `token_*` tests run
`manifest-token`'s refusals.

The SDK import boundary is `source/external/CMakeLists.txt`. Whether it fetched the pin, took
`JETTISON_SOURCE_DIR` or found a parent's SDK, configuration fails unless every target and helper
Manifest uses exists, naming each one missing. `sdk_import` configures Manifest under a parent with
stand-in targets, complete and partial (`source/test/sdk-import`), without fetching or building.
Module libraries publish their own header sets and dependencies;
integration tests link the aggregate `Manifest::Game`. Existing assertions run under the engine's
doctest version. The simulation suite pins generated and replayed state hashes, and the fixture suite the trading
fixture's whole loop ([W09](stage-4-trading-plan.md#delivered-w09-2026-11-20)). The mod suite
mounts the staged directory and generated pack through the engine VFS. The default test build
builds that pack first, validated before and after packing
([content identity](stage-2-content-plan.md#content-identity)). The TUI executable is `build/<preset>/source/tools/tui/manifest_tui`.

`manifest_data` writes the definition records from the authored YAML with mikefarah/yq v4 (tested
with the devenv version, 4.54.1). Missing yq fails configuration when tests are enabled; with tests
disabled the C++ library can build, but packaging still requires yq. Generated records live in the
build tree. Running `bash data/build.sh` manually writes to the source-tree `data/definitions`;
`bash data/build.sh /absolute/output/path` selects an explicit output directory.

The console suite checks command registration, quoted paths and names, completion and history,
typed errors, numeric bounds, configuration reloads and the load/generate/tick/trade-planning
workflow. Run it without a terminal using `ctest --preset clang-debug -R '^console$'`.
`step-budget` runs one of its cases alone (`RUN_SERIAL`): a cruising hosted step, timed in an
optimised build without sanitisers and not on CI, fails above half as much again as the figure
measured on Lythir ([L26](stage-4-trading-plan.md#delivered-l26-2026-11-20)). A change that makes the
step dearer on purpose measures it again and moves the figure.

`MANIFEST_BUILD_TUI=OFF` omits the terminal and FTXUI; an SDK without presentation also omits the
development console, which is built on the engine's.
`BUILD_TESTING=OFF` or `MANIFEST_BUILD_TESTS=OFF`
omits the suites. Manifest tests and the TUI default to off when a host consumes the project.
The presets set both to on, so the gate builds that configuration only.
`MANIFEST_WERROR=ON` makes project warnings fatal. `MANIFEST_CCACHE=OFF` and
`JETTISON_CCACHE=OFF` disable compiler caches. C++ module scanning is disabled because this
project uses headers, keeping compile commands directly usable by clangd and clang-tidy.

The engine is added with `EXCLUDE_FROM_ALL`: configuring loads its complete SDK, but building
Manifest compiles only the engine modules and dependencies it links, plus its packer. Renderer
smoke tests remain Jettison's gate. Do not configure Jettison's unsupported install/export path.

Clang-tidy is opt-in (`./scripts/ci.sh --tidy`), since it is expensive on the development laptop.
CI enables it, and so does a gate on Lythir: with its record of clean units, a small change costs
little.

### Gating beside the work

On Lythir a commit is gated in a second checkout, so the next change is written while the gate
runs. `build/gate-tree` is a detached worktree with its own presets' build trees, made once with
`git worktree add --detach build/gate-tree <commit>`; for each commit gated,
`git -C build/gate-tree checkout --detach <commit>` and then `build/gate-tree/scripts/ci.sh` with
the gate's options. While it runs nothing else builds, runs tests or runs clang-tidy (the user's,
2026-11-21), so the work beside it is editing alone. A failure is fixed by a later commit, before
the push.

## Machine-specific concurrency

[D-051](decision-ledger.md#d-051) distinguishes the laptop's resource limit from other
machines. Limit compilation on the laptop to two workers. Lythir has 12 cores and 24
threads and is not subject to that laptop limit; choose concurrency to suit its available
resources and other running work.

The gate's conservative default remains two compile and test workers, and the tidy runner
also defaults to two. Override `CMAKE_BUILD_PARALLEL_LEVEL` for compilation,
`CTEST_PARALLEL_LEVEL` for tests and `MANIFEST_TIDY_JOBS` for tidy on a suitable machine.
These settings are independent; increasing compile workers does not require increasing
test or tidy workers. CI has its own explicit limits in the workflow.

## T5 selected environment-parity plan

[D-054](decision-ledger.md#d-054) resolves the approach to the open D-006 deviation.
Desktop-only tools stay out of the shared shell: `lazygit` was removed from both repositories'
`devenv.nix` on 2026-11-18 and is installed on the desktop instead.

Implemented in M04 (2026-11-18): the pin is `fafa758`, and the three files match it byte for
byte. `scripts/check-env-parity.sh [repository]` reads the pin, fetches only that commit (from the
pin's `GIT_REPOSITORY`, or a given clone, whose committed objects alone are used) and diffs each
file against it; it never reads `JETTISON_SOURCE_DIR`. `scripts/test-env-parity.sh` runs it on
copies against a throwaway repository: a match, each file differing or missing, a file absent at
the pin, no pin, two pins, an abbreviated pin, an unknown pin, an engine's uncommitted edit and a
`JETTISON_SOURCE_DIR` at matching files. CI runs both straight after checkout, before Nix and
compilation. The handoff records the evidence. The plan as selected follows.

Review every intervening Jettison commit before selecting the new SDK revision; a newer sibling
checkout alone is not a reason to move the pin. Then update Manifest's immutable pin and copy all
three environment files from that exact committed revision together. Preserve the
byte-identity rule without a Manifest-only package exception.

Add a parity check to Manifest's CI before compilation. Its revision authority is
`MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`, not a second hard-coded SHA,
a moving branch or the current sibling checkout. Compare all three Manifest files against
the committed blobs at that revision. The checker may reuse an available engine clone that
contains the exact revision or obtain it independently; a local SDK override or uncommitted
engine edits must not substitute a different baseline. Missing files, an unresolved or
ambiguous pin, unavailable reference content and any byte difference fail with a useful
file/revision diagnostic rather than silently skipping the check. Keep the lock file intact
unless the selected SDK change itself requires updating it.

Implementer ownership: Jettison's shared environment and its verification first; Manifest's
SDK pin, matching environment files, parity tooling, CI wiring and workflow documentation
second. Keep README and AGENTS guidance aligned with the final verified state. This is T5's
parity deliverable, also referenced by T8; implement one check rather than separate copies.

Acceptance evidence includes a passing comparison of all three files against the selected
commit and rejection of a controlled difference in each file separately, including missing
reference content. Show that local overrides cannot bypass the comparison. Record the engine
verification and run Manifest's full gate from a clean checkout using its pinned fetch with no sibling-engine override. Retain
the replay expectations and record exact revisions, stages and logs under D-007. Apply T10's
selected CI order when its implementation has landed and use D-051's host-specific concurrency.
Environment parity alone does not establish Stage 1's CPU-only consumer or gameplay integration.

## Verification evidence

A gate is recorded, not implied. After running it, add a short entry to
[session handoff](session-handoff.md) with the commit, the presets and suites that passed, the
three state hashes (generation, 100-tick, raw price) and the log path. Gate logs are ignored build
artifacts under `build/`, never committed fixtures. "Configure succeeded", mounted content
equality and documentation edits are not verification. Record which stages actually ran: a partial
run is not a gate.

The newest local evidence is the
[T1/T9 verification record](history.md#t1t9-local-verification--2026-11-16), including
the tested tree, full command, presets, suites, replay hashes and log path. Both tidy findings
identified in the [historical CI analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16)
are fixed. This does not establish engine integration or gameplay acceptance.

The CI [workflow](../.github/workflows/ci.yml) runs the full gate with `--tidy` on each push to
`feature-base`, on each pull request targeting it and on a manual dispatch, and uploads
`build/ci.log`. A push to `main` or to another branch runs nothing (M02, 2026-11-18). Report CI
from the exact commit's run and fetched logs, separately from local evidence. Tidy runs last,
after every requested build/test preset, and still fails the gate (M01, 2026-11-18). Preserve
named-stage reporting, and prefer a combined local gate over repeated pushes.

### Promoting to main

`scripts/promote.sh [--check] [commit]` fast-forwards `main` to a commit, by default
`origin/feature-base`, and `--check` stops before the push. It refuses a commit that does not
descend from `origin/main` (divergence: integrate on `feature-base` and verify there), one that
is not on `origin/feature-base`, and one without a completed, successful CI run of its own from a
`feature-base` push. A cancelled run is re-run from Actions; a PR, manual or ancestor run does
not count. The push is a plain fast-forward, so git also rejects a race with another update.

The script is the procedure, not an enforcement. Required status checks would not enforce it
either: a pull request's merge-candidate result is reported on its head SHA. The repository's one
rule is the ruleset "main: fast-forward only" (27518688, active, applied 2026-11-18, no bypass
actors), which blocks force pushes to `main` and its deletion. `feature-base` has none.

### T10 selected gate and promotion policy

[D-052](decision-ledger.md#d-052) and [D-053](decision-ledger.md#d-053) select the following
implementation contract. The gate order (D-052) is implemented in `scripts/ci.sh` (M01,
2026-11-18); the triggers and promotion (D-053) in the workflow and `scripts/promote.sh` (M02,
2026-11-18), described [above](#promoting-to-main).

Run formatting, Debug build/CTest, Release build/CTest, ASan/UBSan build/CTest, then
clang-tidy in that order for the full gate. Keep `--warnings-as-errors=*` for tidy; do not
introduce a warning baseline or suppress checks. Preserve the existing optional local flags:
`--skip-release` omits Release, `--asan` includes sanitizers and `--tidy` includes tidy.
Keep ordinary failure propagation: a tidy failure must fail the gate, after the requested
build/test stages have completed. This does not require continuing after an earlier build or
test failure, and a shortened local pass remains partial evidence.

`feature-base` is the integration branch. Its push runs the full gate on the resulting
commit. `main` receives only fast-forward promotions to an exact commit already tested on
`feature-base`; it receives no independent changes or promotion merge commits. A promotion
must verify a successful, completed full gate for that exact SHA under the applicable workflow.
An ancestor's result, a PR merge candidate's result, a partial gate, or a missing, running,
failed or cancelled run is insufficient. If the update cannot fast-forward, stop and resolve
the divergence through integration and verification rather than rewriting either branch.

Avoid the second full gate triggered solely by promoting the same SHA to `main`. Retain
PR candidate verification for changes targeting `feature-base`; ordinary feature-branch pushes
need not also run a full gate when the PR already provides that check. The integrated commit
still needs its own `feature-base` result. Explicit manual diagnostic runs remain available.
This policy removes duplicate promotion work; it does not equate distinct PR and integrated
commits or cancel one branch's required run when another branch advances.

Implementer scope: `scripts/ci.sh`, `.github/workflows/ci.yml`, the promotion procedure and
their workflow documentation. Before changing triggers, inspect repository rules and required
checks, and confirm that the verified integration result can authorize promotion without
weakening protection or inventing a passing result for skipped work. Any required repository
administration must be identified explicitly. `scripts/tidy.py` needs no warning-policy change.
No SDK or gameplay changes are required. Concurrency follows [D-051](decision-ledger.md#d-051).

Acceptance evidence must include a controlled tidy failure after successful requested
build/test stages, with a nonzero gate result and logs showing those stages ran first; a
passing full gate on the final implementation; and promotion checks covering exact-SHA success,
unverified changes and divergence. Confirm that a normal verified fast-forward to `main`
does not start another full gate, while a new integration commit and an explicit manual run
still do. Record actual stages, logs and exact-commit CI evidence under D-007 before closing T10.

Preset builds live in `build/clang-debug/`, `build/clang-notui/`, `build/clang-release/` and
`build/clang-asan/`. A
top-level `build/` containing `CivSim*` artifacts is the ignored pre-rename configure tree; remove
it rather than reading it as a current build. `.vscode/` is ignored, so the tracked
`.vscode/*.example` files are the reference for a fresh clone; they are currently stale and are
repaired by handoff T7.

### T7 selected editor-example plan

[D-055](decision-ledger.md#d-055) selects repair of the tracked
`.vscode/launch.json.example` and `.vscode/tasks.json.example`, with setup instructions here.
Delivered 2026-11-18 (M05): the setup is [below](#editor-setup), the evidence in the
[session handoff](history.md#m05-the-editor-examples--2026-11-18).

Provide two launch entries using these Debug executables, confirmed against CMake targets and
the existing generated CTest commands:

| Launch | Executable relative to the workspace |
| --- | --- |
| Simulation unit tests | `build/clang-debug/source/game/sim/manifest_sim_tests` |
| Development TUI | `build/clang-debug/source/tools/tui/manifest_tui` |

Remove the legacy launch arguments; the simulation suite requires no YAML path arguments.
The TUI uses its configured data paths and normal startup interface. Make configure, build
and test tasks explicitly use the `clang-debug` presets, with configure before build and
build before tests or debugger launch. Task labels and `preLaunchTask` references must agree.
Preserve Ninja-compatible targets and build dependencies for generated content; do not fall
back to the pre-rename top-level build tree or source-tree generated YAML.

Document copying the examples to ignored personal files, required editor/debugger extensions
and launching the editor/tasks in the development shell. Validate the chosen debugger adapter's
configuration fields rather than mixing adapters. Compilation follows D-051: the laptop stays
within its limit and the tasks respect overrides appropriate to Lythir. Preserve existing
personal editor files when updating the tracked templates.

Acceptance: copy the examples in a fresh checkout, configure and build through the tasks,
run the Debug CTest suites, and launch both entries through the debugger. Reach a source
breakpoint in each and continue the simulation suite to success and the TUI to normal exit.
Record the debugger/extension used and observed results. Inspecting JSON or finding an old
binary alone does not establish working editor integration. No SDK, gameplay or test-source
change is needed for this repair; this check is separate from the full gate's evidence.

#### Editor setup

The tracked files are `.vscode/tasks.json.example`, `.vscode/launch.json.example` and
`.vscode/settings.json.example`; the personal copies without `.example` are ignored. In a checkout:

```sh
cp .vscode/tasks.json.example .vscode/tasks.json
cp .vscode/launch.json.example .vscode/launch.json
cp .vscode/settings.json.example .vscode/settings.json
devenv shell -- code .
```

Start the editor from the development shell, so the tasks find `cmake`, `ctest` and clang on
`PATH` and inherit D-051's concurrency overrides (`CMAKE_BUILD_PARALLEL_LEVEL`,
`CTEST_PARALLEL_LEVEL`). The tasks are shell tasks on the `clang-debug` presets: `CMake:
configure`, then `CMake: build` (the default build task, the `all` target), then `CTest: test`
(the default test task). Both launches build first.

The debugger is **CodeLLDB** (`vadimcn.vscode-lldb`) with the development shell's LLDB 21,
matching the compiler, which the settings example names as `lldb.library` (CodeLLDB expands
`${workspaceFolder}`; LLDB finds its `lldb-server` beside the library). The example also points
the C/C++ extension at the shell's clang. On NixOS, CodeLLDB's adapter is a generic Linux binary,
so the system needs `programs.nix-ld.enable = true`; without it the adapter fails with "Could not
start dynamically linked executable". *Simulation unit tests* runs the suite in the debug console (`terminal:
console`); *Development TUI* runs in the integrated terminal (`terminal: integrated`), which the
full-screen interface needs.

Proposed checks, not present (handoff T8): local Markdown link and heading-anchor integrity across
`docs/`, `README.md` and `AGENTS.md` — an ad-hoc scan at the 2026-11-16 assessment covered 31
documents and roughly 300 local targets, found two stale heading anchors in `docs/coding-standards.md`
pointing at Jettison headings, and those links now use `../../jettison/docs/…`; the
checker lives in the analysis appendix until an implementer commits it to `scripts/` and CI. Also
proposed: a `devenv.nix`, `devenv.yaml` and `devenv.lock` parity check against the pinned engine
revision, and the CMake dependency-closure
boundary checks in the [boundary assessment](engine-game-boundary-assessment.md#enforcement-to-implement-later).
