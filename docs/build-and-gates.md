# Build and gates

Run inside `devenv shell` or the direnv-loaded shell. Environment parity with the selected SDK
remains tracked under T5; see the current handoff. The SDK source pin is in
`source/external/CMakeLists.txt`; to test a local engine,
configure with `-DJETTISON_SOURCE_DIR=/path/to/jettison` on `feature-base`.

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
./scripts/ci.sh --werror --asan --log build/gate.log
```

Presets are `clang-debug`, `clang-release` and `clang-asan`. Each has its own build tree, generated
data and staged mod. The gate runs formatting, Debug build and CTest, Release build and
CTest, and optionally ASan/UBSan build and CTest. `--skip-release` supports a shorter local pass;
CI runs the full gate. `CMAKE_BUILD_PARALLEL_LEVEL`, `CTEST_PARALLEL_LEVEL` and
`MANIFEST_TIDY_JOBS` control concurrency. Clang-format must be version 21.

The CTest suites are `types`, `config`, `universe`, `progression`, `sim`, `ecs`, `market`,
`effects`, `data` and `mod`, plus `console` when the TUI is enabled.
Module libraries publish their own header sets and dependencies;
integration tests link the aggregate `Manifest::Game`. Existing assertions run under the engine's
doctest version. The simulation suite pins generated and replayed state hashes. The mod suite
mounts the staged directory and generated pack through the engine VFS. The default test build
builds that pack first. The TUI executable is `build/<preset>/source/tools/tui/manifest_tui`.

`manifest_data` merges authored YAML with mikefarah/yq v4 (tested with the devenv version,
4.54.1). Missing yq fails configuration when tests are enabled; with tests disabled the C++
library can build, but packaging still requires yq. Generated YAML lives in the build tree.
Running `bash data/build.sh` manually retains the source-tree `data/core-config` default;
`bash data/build.sh /absolute/output/path` selects an explicit output directory.

The console suite checks command registration, quoted paths and names, completion and history,
typed errors, numeric bounds, configuration reloads and the load/generate/tick/trade-planning
workflow. Run it without a terminal using `ctest --preset clang-debug -R '^console$'`.

`MANIFEST_BUILD_TUI=OFF` omits the terminal, console tooling and FTXUI.
`BUILD_TESTING=OFF` or `MANIFEST_BUILD_TESTS=OFF`
omits the suites. Manifest tests and the TUI default to off when a host consumes the project.
`MANIFEST_WERROR=ON` makes project warnings fatal. `MANIFEST_CCACHE=OFF` and
`JETTISON_CCACHE=OFF` disable compiler caches. C++ module scanning is disabled because this
project uses headers, keeping compile commands directly usable by clangd and clang-tidy.

The engine is added with `EXCLUDE_FROM_ALL`: configuring loads its complete SDK, but building
Manifest compiles only the engine modules and dependencies it links, plus its packer. Renderer
smoke tests remain Jettison's gate. Do not configure Jettison's unsupported install/export path.

Clang-tidy is opt-in locally (`./scripts/ci.sh --tidy`), since it is expensive on the
development laptop. CI enables it.

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

## Verification evidence

A gate is recorded, not implied. After running it, add a short entry to
[session handoff](session-handoff.md) with the commit, the presets and suites that passed, the
three state hashes (generation, 100-tick, raw price) and the log path. Gate logs are ignored build
artifacts under `build/`, never committed fixtures. "Configure succeeded", mounted content
equality and documentation edits are not verification. Record which stages actually ran: a partial
run is not a gate.

The newest local evidence is the
[T1/T9 verification record](session-handoff.md#t1t9-local-verification--2026-11-16), including
the tested tree, full command, presets, suites, replay hashes and log path. Both tidy findings
identified in the [historical CI analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16)
are fixed. This does not establish engine integration or gameplay acceptance.

The CI [workflow](../.github/workflows/ci.yml) runs the full gate with `--tidy` on every push
and uploads `build/ci.log`. Report CI from the exact commit's run and fetched logs, separately
from local evidence. Tidy still runs between Debug and Release under `set -euo pipefail`,
so a future tidy failure can suppress Release and ASan/UBSan evidence; T10 remains open.
The same handoff also addresses duplicate gates on identical branches. Preserve named-stage
reporting, and prefer a combined local gate over repeated pushes.

Preset builds live in `build/clang-debug/`, `build/clang-release/` and `build/clang-asan/`. A
top-level `build/` containing `CivSim*` artifacts is the ignored pre-rename configure tree; remove
it rather than reading it as a current build. `.vscode/` is ignored, so the tracked
`.vscode/*.example` files are the reference for a fresh clone; they are currently stale and are
repaired by handoff T7.

Proposed checks, not present (handoff T8): local Markdown link and heading-anchor integrity across
`docs/`, `README.md` and `AGENTS.md` — an ad-hoc scan at the 2026-11-16 assessment covered 31
documents and roughly 300 local targets, found two stale heading anchors in `docs/coding-standards.md`
pointing at Jettison headings, and those links now use `../../jettison/docs/…`; the
checker lives in the analysis appendix until an implementer commits it to `scripts/` and CI. Also
proposed: a `devenv.nix`, `devenv.yaml` and `devenv.lock` parity check against the pinned engine
revision, and the CMake dependency-closure
boundary checks in the [boundary assessment](engine-game-boundary-assessment.md#enforcement-to-implement-later).
