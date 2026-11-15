# Project analysis — 2026-11-16

Assessed at Manifest `feature-base` `f63a3a4` (`Ran out of tokens. Partial work being committed.`),
working tree clean, `main` and `feature-base` identical. Sibling engine checkout
`../jettison` on `feature-base` at `0564f19`; Manifest's pin is `f850920`.

This is read-only planning documentation. No implementation code, tests, fixtures, assets,
build scripts, `data/` YAML, `devenv.*` or `.vscode` files were changed here, and no build,
CTest run, clang-format gate or clang-tidy run was performed. Every finding below is recorded
so an implementer can act on it; the [models and agent workflow](agent-workflow.md) policy
assigns that execution.

## Verdict

The model layer is real, tested and deterministic. The engine integration is not. Manifest
today is a host-side C++ library plus a development TUI and a mountable content pack: it does
not bind state to a Jettison simulation world, load content through the engine VFS, register
scripts, network, or save. Stage 1 of the
[next development stages](development-stages.md#stage-1-public-engine-host-and-standalone-bootstrap)
has not started, and the roadmap already carries multi-month product pillars beyond it.

Documentation is now the repository's largest and fastest-moving artifact: 28 files and 8,719
lines against 17,398 lines of C++. Since the last verified gate, the only non-documentation
changes are `AGENTS.md`, `.gitignore` and `devenv.nix`; the seven commits after that merge touch
documentation only.
The main risks are therefore not code quality in `source/game/`, but an unverified build
baseline, a CI gate that is red on clang-tidy alone while suppressing its own Release and
sanitiser stages ([CI failure analysis](#ci-failure-analysis--2026-11-16)), duplicated planning
prose with no traceability to code, balance values that exist only in prose, and product scope
arriving faster than any of it can be built.

## State at assessment

| Layer | Contents | Assessment |
| --- | --- | --- |
| C++ model | 17,398 lines, 8 modules (`types`, `config`, `universe`, `progression`, `sim`, `ecs`, `effects`, `market`) plus `tools/tui` | Substantive: `market/src/Marketplace.cpp` 2,423 lines, `sim/src/Simulation.cpp` 802, `effects/src/ShipEffectEngine.cpp` 881 |
| Tests | 11 doctest suites, 111 `TEST_CASE` entries | `market/tests/MarketplaceTests.cpp` alone is 2,288 lines; cross-module, data and mod-pack suites exist in `source/test/` |
| Content | 89 items, 85 skills, ages, categories, 6,268 lines of universe name pools; `full` mode 176 references, `simple` 100 | Merged by `data/build.sh` with duplicate-ID checks, dangling-reference checks and atomic output |
| Build | Pinned SDK via `FetchContent`, engine-owned dependency targets, `manifest_add_module`, `manifest_mod` and `manifest_pack` | Sound; CI mirrors the devenv shell |
| Docs | 28 files, 8,719 lines, of which `economy-and-development-plan.md` 1,186 and `session-handoff.md` 1,092 | Forward-looking design dominates; see findings 2 and 3 |
| Engine integration | 4 touchpoints: `jettison/core/Assert.h`, `jettison/math/Random.h`, `jettison/devtools/Console.h`, `jettison/vfs/FileSystem.h` (test only) | Not integrated; consistent with the self-declared roadmap state |

## Verified build evidence

The newest recorded gate is the decimal-prices handoff in
[session handoff](session-handoff.md): formatting and all 11 suites passed in Debug, Release and
ASan/UBSan at `5d7f54e` (`Market prices: use decimal values while preserving floating
quantities`, 2026-11-15), with generation hash `10355632731109243635`, 100-tick hash
`12410383246888723003` and raw-price hash `15444164946577136117`.

`git diff 5d7f54e..HEAD` outside `docs/` and `README.md` is `AGENTS.md`, `.gitignore` (one
comment line) and `devenv.nix` (`+lazygit`). No C++, CMake, preset, `data/` or `mods/` file
changed after the verified gate, so the source build is very likely intact. The gate is
nevertheless **not verified at HEAD**, because the locked Nix shell changed. There is also no
`build/clang-debug/`, `build/clang-release/` or `build/clang-asan/` tree and no gate log in this
checkout: the top-level `build/` holds pre-rename `CivSimLibrary` artifacts from 2026-11-08 and
is ignored by `.gitignore`. CI cannot fill this gap: see the next section.

## CI failure analysis — 2026-11-16

Investigated read-only with `gh run list` and `gh run view --log-failed` (`gh` 2.103.2,
authenticated). No clang-tidy was run locally, as `AGENTS.md` requires.

**CI has been continuously red since 2026-11-14T17:05Z.** The newest green run is 2026-11-10T01:27Z
(`Added Context7 integration for Pi`, 42735907592). Eighteen runs have failed since, with three
cancellations from rapid pushes; thirteen are consecutive failures on the two live branches, running
from 2026-11-15T01:47:30Z (`Docs: sequence the next five development stages for Claude`, 43664731273)
to 2026-11-15T18:03:27Z (`Docs: record the CI failure analysis and agent-facing gate status`,
43744555932 on `ee83ea5`). The last nine commits in the streak touch documentation and `AGENTS.md`
only; the two before them (`dbeb191`, `2716380`) changed `.gitignore` and `devenv.nix`. CI's most
recent build signal is therefore the Debug presets on `ee83ea5`. Two further runs listed at
2026-11-16T04:21Z (43806887285, 43806869597, both `failure`) were for a head commit GitHub no longer
serves, and their run records now return 404; the counts above come from the run list at assessment
time and exclude them.

Every recent failure has the same shape, and the build is not the problem:

| Stage in `scripts/ci.sh` | Result in CI |
| --- | --- |
| `scripts/format-check.sh` | pass |
| Debug configure and build | pass |
| `ctest --preset clang-debug` | pass: `100% tests passed out of 11` |
| clang-tidy (`--tidy`, CI only) | **fail**: `1 warning treated as error`, process exit 1 |
| Release build and CTest | never reached |
| ASan/UBSan build and CTest | never reached |

Run 43744555932 reproduces that table on `ee83ea5`, a documentation-only push: `100% tests passed
out of 11` in Debug, then 16 `Decimal.h:99` reports and one `Marketplace.cpp:1688` report, and no
`clang-release` or `clang-asan` output anywhere in the log. Its wall clock was about 17 minutes, so
plan for 5–17 minutes per gate depending on runner cache state. The run's `build/ci.log` artifact is
the citable evidence, and it is not retained in this repository.

Two clang-tidy findings account for the streak, identical in each log:

| # | Location | Check | Evidence |
| --- | --- | --- | --- |
| 1 | `source/game/types/include/manifest/types/Decimal.h:99`, `raw += digit * place;` | `bugprone-implicit-widening-of-multiplication-result` | 16 occurrences per run: the multiply is performed in `std::int64_t` and the result is then widened to `__int128_t`, and `HeaderFilterRegex: 'source/.*'` reports it once per TU that includes the header. The range (`digit` ≤ 9, `place` ≤ 10⁶) is well inside `int64`, so behaviour is correct, and the sibling branch at `Decimal.h:93` already writes `static_cast<__int128_t>(digit) * kScale`, so the header is internally inconsistent. |
| 2 | `source/game/market/src/Marketplace.cpp:1688`, `errorMessage = "MarketplaceService: " + errorMessage;` | `performance-inefficient-string-concatenation` | One occurrence per run; first seen as `Marketplace.cpp:1576`, before the module reorganisation moved the line. |

The streak begins with `81f72a6` (2026-11-15 00:13 +10, `Build: align Manifest with Jettison's SDK
and module layout`), which added `scripts/tidy.py`, added `--tidy` to `scripts/ci.sh` and added
`--tidy` to the CI gate. Run 43619179521 (2026-11-14T17:05Z) is the first with tidy and fails on
finding 2 alone with `100% tests passed out of 10`. `5d7f54e` then introduced `Decimal.h` and
finding 1, present from the 2026-11-14T18:15Z run onward.

Local gates never saw these findings: the documented local gate omits `--tidy`, and the last
recorded local gate at `5d7f54e` ran without it. CI is also stricter than the repository's own tidy
configuration, because `scripts/tidy.py` passes `--warnings-as-errors=*` while `.clang-tidy` sets
`WarningsAsErrors: ''`. The rule stated in `.clang-tidy` is that "a check earns its place by being
clean over the project's own sources, or by pointing at something that gets fixed"; these two
checks are neither, so the tree currently violates its own configured invariant.

**The consequence is larger than the two warnings.** `scripts/ci.sh` runs tidy between the Debug
and Release stages under `set -euo pipefail`, so every run in the streak aborts before Release and
before ASan/UBSan: CI has produced no Release, ASan or UBSan evidence since 2026-11-14. Do not
treat a CI run as gate evidence for those presets, and never describe CI as passing or failing
"the build" without naming the stage that ran. The only route to Release and sanitiser evidence
today is a local gate run (T1 with T9).

Two smaller CI observations: `main` and `feature-base` are identical, so every push pays for two
full gates, and rapid pushes cancelled three runs. The failures on 2026-11-10 and 2026-11-12
(durations near 50 seconds) are a different, resolved era: CMake configure errors in the legacy
monolithic `CMakeLists.txt` (`civsim_add_test`, `add_library` and `add_executable` with missing
sources) from the acknowledged broken build, fixed by `81f72a6`.

## Findings

1. **Build status is unverified at HEAD, and stale local artifacts obscure it.** Evidence is in
   the section above. The `devenv.nix` delta changes the locked environment, so "it built at
   `5d7f54e`" is not "it builds now". Task T1.
2. **Planning volume and duplication outrun implementation.** Selected decisions are repeated
   near-verbatim across documents: `30 kg oxygen` appears in 8 docs, `0.5 + health / 200` in 6,
   `500 kg per 1,000` in 6, `90 kg copper` in 4, `72 server-running hours` in 3. Seventeen of the
   28 documents carry 106 `remain open`/`not implemented` markers. Nothing maps a decision to a
   module, stage, test or status, so consistency depends on re-reading prose. Tasks T2 and T3.
3. **Balance values live only in prose.** Consumption rates, cadences, fees, basis points, the
   condition multiplier, concession duration and tender bond percentages are stated in markdown
   paragraphs. The authored YAML under `data/` covers items, skills, ages and game-mode
   selections; it carries none of these tuning inputs, so no suite validates them and balance
   iteration means editing documents. Task T4.
4. **Progress is gated on engine work, and the pin lags.** The
   [boundary assessment](engine-game-boundary-assessment.md) requires Jettison-side extraction
   (sample gameplay out of the app target, celestial assumptions out of physics/scene/sim
   snapshot schemas, a CPU-only consumer that configures without ImGui/Vulkan, docking admission
   and queue seams, a smaller-berth capability, atomic topology publication) before Manifest can
   bind state, movement and recovery. The engine checkout is at `0564f19`; the pin is `f850920`.
   Task T5 covers pin and environment parity.
5. **Product scope is expanding faster than the delivery pipeline.** Since 2026-11-15 the
   roadmap added a dedicated networked idle server with remote TUI `connect`, multi-region
   governance on one planet, corporate political and media influence, public procurement and
   bribery, stargate FTL bootstrap, life-support consumables and market information fog. The
   roadmap itself states none of it is implemented. The first playable loop needs stages 1–4,
   not these pillars. Task T6 records the scope funnel rule.
6. **Committed developer files are stale.** `.vscode/launch.json.example` (tracked) targets
   `build/ManifestCoreTests`, which no longer exists: presets place binaries under
   `build/<preset>/` and test executables are per module (`ConfigTests`, `EcsTests`,
   `MarketplaceTests`, …). `README.md` states that `devenv.nix`, `devenv.yaml` and
   `devenv.lock` match the pinned checkout: `devenv.yaml` and `devenv.lock` match `f850920`
   exactly, `devenv.nix` adds `lazygit` relative to both `f850920` and engine HEAD, which
   conflicts with the identity rule in `AGENTS.md`. The README wording is corrected here; the
   environment file and the VS Code examples are implementer work (T5, T7).
7. **Minor module asymmetry.** The `effects` module declares no `TESTS` in
   `source/game/effects/CMakeLists.txt`; it is covered by `source/test/EffectsIntegrationTests.cpp`
   (1,507 lines, 8 cases), which is documented in [architecture](architecture.md). No action
   beyond noting that per-module failure isolation for effects is coarser than for the other
   modules. All 54 `namespace Manifest::Core` occurrences in `source/` survive the module
   reorganisation, which the README documents as an intentional compatibility alias.
8. **Two stale heading anchors in `docs/coding-standards.md`, now fixed.** An ad-hoc link and
   anchor check over 31 documents and roughly 300 local link targets found no missing files and
   exactly two broken fragments: `architecture.md#scripts` and `build-and-gates.md#integration-rules-the-set-follows`
   at `docs/coding-standards.md:113` and `:136`. The surrounding text cites engine material
   (`celestial`, `spatial`, `SimClock`, Jolt, WAMR, `dependency_probe`), and both headings exist in
   Jettison's documents, so the links lacked the sibling prefix the
   [boundary assessment](engine-game-boundary-assessment.md) uses. From `docs/` the correct relative
   path is `../../jettison/docs/…`, which resolves against the sibling checkout `AGENTS.md`
   documents, and both links now use it. Nothing enforces link integrity in CI, so this class of
   drift is unchecked: handoff T8.
9. **CI is red on clang-tidy alone, and that red hides a coverage hole.** Thirteen consecutive
   failures on the live branches, with the format gate, Debug build and all 11 suites passing in
   every run, including the documentation-only push at `ee83ea5`.
   `Decimal.h:99` and `Marketplace.cpp:1688` fail the CI-only tidy stage, and `scripts/ci.sh`
   aborts before Release and before ASan/UBSan, so CI has reported no sanitiser evidence since
   2026-11-14. Tasks T9 and T10; full evidence, timeline and run IDs in
   [CI failure analysis](#ci-failure-analysis--2026-11-16).

## Recommendations, in priority order

**P0 — re-establish a verified baseline (T1).** Run the gate at HEAD and record the outcome,
the log path and the three state hashes in `session-handoff.md`. This is the cheapest action that
removes the "not sure of build status" ambiguity introduced by the merge commit `2716380`, and it
is the precondition for Stage 1.

**P0 — repository hygiene (T5, T7).** Remove the ignored pre-rename `build/` tree; reconcile
`devenv.nix` with the pinned SDK, documenting an explicit exception if `lazygit` is to stay;
repair `.vscode/launch.json.example` and `.vscode/tasks.json.example` (the tracked task file
selects `ALL_BUILD`, an MSBuild generator target, while the presets use Ninja).

**P0 — make CI meaningful again (T9, T10).** Apply the two tidy fixes and move the tidy stage after
the Release and ASan/UBSan stages. The current CI red streak is not a build failure: Debug builds
and all 11 suites pass in every run, and the tidy abort is what suppresses Release and sanitiser
evidence. Fixing the findings restores green; reordering the stages keeps the signal honest.

**P1 — execute Stage 1, not further planning.** Stage 1 has written exit criteria: Manifest
starts, mounts its package and shuts down through public APIs; the Jettison sample still works; a
clean checkout builds without the sibling repository; startup-failure cleanup is tested. The CPU-only
consumer that configures without presentation dependencies is a Stage 1 acceptance criterion per
the boundary assessment, and it is also the foundation of the required dedicated server.

**P1 — traceability (T2, T3).** Record selected decisions once, in
[the decision ledger](decision-ledger.md), with stable IDs, canonical sections, status, owner and
stage. Long-form plans link by ID. The ledger is the antidote to finding 2 and the index a fresh
session needs.

**P1 — move tuning inputs into authored data (T4).** Add the economy cadence, household,
condition, concession, fee and bond values as authored YAML with schema validation, so prose
records intent and data carries numbers, and the data suite can reject malformed tuning sets.

**P2 — enforcement checks (T8).** The boundary assessment's "enforcement to implement later"
list plus cheap documentation checks: local Markdown link and heading-anchor integrity (the ad-hoc
check at this assessment scanned 31 documents and roughly 300 targets, found the two stale anchors in
finding 8, and is otherwise clean), `devenv.*` parity against the pin, dependency-closure checks
rejecting core-to-game edges, and a gate-log naming convention. The checker in the appendix is
offered as a starting point; committing and wiring it into CI is implementer work.

## Handoffs to the implementer

Each task is bounded, names its acceptance evidence, and uses two compile jobs with local
clang-tidy left opt-in.

| ID | Task | Acceptance |
| --- | --- | --- |
| T1 | Run `./scripts/ci.sh --werror --asan --log build/gate-analysis.log` at HEAD; record the result, log path and state hashes in `session-handoff.md`; if it fails, fix and re-run | Gate passes in Debug, Release and ASan/UBSan; hashes recorded and compared with the `5d7f54e` values above |
| T2 | Extend the existing [decision ledger](decision-ledger.md) as decisions are selected, and reduce the duplicated statements in `scope.md`, `roadmap.md`, `development-stages.md`, `architecture.md` and `session-handoff.md` to one canonical section per decision plus a ledger link | Each selected decision has exactly one canonical statement; links resolve; `git diff --check` clean |
| T3 | Apply the [ledger rules](decision-ledger.md#rules-for-maintaining-this-ledger) and the [planning discipline](agent-workflow.md#planning-discipline-after-the-2026-11-16-analysis) to future planning updates | New decisions arrive with an ID, status, owner and stage; no new prose duplicates |
| T4 | Author the economy tuning inputs as YAML under `data/`, wire them through the existing merge, loader and data suite, and point the prose at the file | Data suite validates the values; no balance number exists only in prose |
| T5 | Reconcile `devenv.nix` with pinned Jettison (`f850920`) by removing `lazygit` or recording an explicit documented exception, add a CI parity check for all three devenv files, and keep the README statement aligned with the final rule | Parity check green; README, `AGENTS.md` and the files agree |
| T6 | Enforce the scope funnel in planning: each new mechanic names its stage, the public SDK API it consumes and the demonstrated gap, and no new product pillar is scheduled before the Stage 4 acceptance criteria pass | Roadmap entries carry stage/API/gaps triplets; pillars are explicitly deferred with a re-entry condition |
| T7 | Repair the tracked VS Code examples: preset-relative binary paths, real test target names, Ninja task target | A fresh clone's examples match `CMakePresets.json` and the generated binaries |
| T8 | Add the cheap checks in the appendix to this document to CI: local Markdown link and heading-anchor integrity, `devenv.*` parity against the pinned engine revision, and the CMake dependency-closure boundary checks from the [boundary assessment](engine-game-boundary-assessment.md#enforcement-to-implement-later) | Checks run in CI and fail on violations; documented in [build and gates](build-and-gates.md) |
| T9 | Fix the two clang-tidy findings that keep CI red: an explicit widening cast at `source/game/types/include/manifest/types/Decimal.h:99`, matching the `static_cast<__int128_t>` form already used at `Decimal.h:93`, and the `insert`/`append` form the check suggests at `source/game/market/src/Marketplace.cpp:1688`. Both preserve behaviour. Run `./scripts/ci.sh --werror --asan --tidy --log build/gate-tidy.log`, then push and confirm CI green | Local gate passes including tidy; the CI run on the fix commit is green; both findings are absent from `ci.log`, which now reaches Release and ASan/UBSan |
| T10 | Repair the CI gate shape: run the tidy stage after Release and ASan/UBSan so a tidy warning cannot mask sanitiser evidence; run one gate per push rather than twice on identical `main` and `feature-base`; consider a bounded tidy baseline in place of `--warnings-as-errors=*`, and reporting header findings once rather than per TU | A failing tidy stage no longer prevents Release and ASan/UBSan results; one push costs one full gate |

T1, T5, T7, T9 and T10 are execution. T2, T3, T4, T6 and T8 also change content the planning
documents propose; the implementer applies them and runs the gates. T9 is the shortest path to a
green CI and is best combined with T1 in one gate run.

## Method and limits

Inspected: the full document tree and its internal links and heading anchors; `git log`, `git diff`
and merge structure; `source/` layout, sizes, tests, namespaces and every Jettison include;
`CMakeLists.txt`, `CMakePresets.json`, `cmake/ManifestModule.cmake`,
`source/external/CMakeLists.txt`; `data/build.sh` and authored YAML; `scripts/ci.sh`,
`format-check.sh`, `tidy.py`; `.github/workflows/ci.yml`; `AGENTS.md`, `README.md`, `.gitignore`,
`.vscode`, `.codex`; the pinned versus checked-out engine `devenv.*` files; and the GitHub Actions
run list and failure logs for 2026-11-07 to 2026-11-15 through the authenticated `gh` CLI. Not
performed: any build, test, format, tidy, benchmark or engine run; no line-by-line audit of
renderer, physics, ECS or marketplace algorithms; no C++ memory analysis. Quantities are `wc -l`/
`grep` counts over the working tree, excluding `.git`, `build`, `.devenv` and `.cache`. The link and anchor checker was run
from `/tmp`, not added to `scripts/`, because tooling is implementer work under the planning-only
policy; it is recorded in the appendix for handoff T8.

## Appendix: documentation link and anchor checker

Proposed content for `scripts/doc-links.py`, to be created, wired into `scripts/ci.sh` and CI, and
verified by the implementer (T8). It reports missing files and heading anchors across `README.md`,
`AGENTS.md` and `docs/*.md`, and exits non-zero on any problem. Anchors follow GitHub's rule:
lowercased, punctuation removed, spaces to hyphens, duplicates suffixed.

```python
#!/usr/bin/env python3
"""Check local Markdown links and heading anchors in docs/, README.md and AGENTS.md."""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = [root / "README.md", root / "AGENTS.md", *sorted((root / "docs").glob("*.md"))]
link = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")


def anchors(path):
    text = path.read_text(encoding="utf-8")
    found, seen = set(), set()
    for line in text.splitlines():
        if not line.startswith("#"):
            continue
        title = line.lstrip("#").strip().lower()
        slug = re.sub(r"[^a-z0-9 -]", "", title).replace(" ", "-")
        while slug in seen:
            slug = f"{slug}-1"
        seen.add(slug)
        found.add(slug)
    found.update(m.group(1).lower() for m in re.finditer(r'<a[^>]*name="([^"]+)"', text))
    found.update(m.group(1).lower() for m in re.finditer(r"\{#([a-z0-9-]+)\}", text))
    return found


cache, targets, missing = {}, 0, []
for path in files:
    for match in link.finditer(path.read_text(encoding="utf-8")):
        href = match.group(1)
        if href.startswith(("http://", "https://", "mailto:")):
            continue
        target, _, fragment = href.partition("#")
        targets += 1
        resolved = (path.parent / target).resolve() if target else path
        if not resolved.exists():
            missing.append(f"{path.relative_to(root)}: missing file {href}")
            continue
        if fragment:
            cache.setdefault(resolved, anchors(resolved))
            if fragment.lower() not in cache[resolved]:
                missing.append(f"{path.relative_to(root)}: missing anchor {href}")

print(f"Checked {len(files)} documents and {targets} local link targets")
for problem in missing:
    print(problem)
sys.exit(1 if missing else 0)
```

Related: [roadmap](roadmap.md), [decision ledger](decision-ledger.md),
[development stages](development-stages.md),
[boundary assessment](engine-game-boundary-assessment.md),
[session handoff](session-handoff.md).
