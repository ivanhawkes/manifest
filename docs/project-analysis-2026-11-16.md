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
baseline, duplicated planning prose with no traceability to code, balance values that exist only
in prose, and product scope arriving faster than any of it can be built.

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
is ignored by `.gitignore`. CI (`ci.yml`, full gate with `--tidy`) is the only current evidence,
and its log is not captured in this document.

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

## Recommendations, in priority order

**P0 — re-establish a verified baseline (T1).** Run the gate at HEAD and record the outcome,
the log path and the three state hashes in `session-handoff.md`. This is the cheapest action that
removes the "not sure of build status" ambiguity introduced by the merge commit `2716380`, and it
is the precondition for Stage 1.

**P0 — repository hygiene (T5, T7).** Remove the ignored pre-rename `build/` tree; reconcile
`devenv.nix` with the pinned SDK, documenting an explicit exception if `lazygit` is to stay;
repair `.vscode/launch.json.example` and `.vscode/tasks.json.example` (the tracked task file
selects `ALL_BUILD`, an MSBuild generator target, while the presets use Ninja).

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

T1, T5, T7 and T8 are execution. T2, T3, T4, T6 and T8 also change content the planning documents
propose; the implementer applies them and runs the gates.

## Method and limits

Inspected: the full document tree and its internal links and heading anchors; `git log`, `git diff`
and merge structure; `source/` layout, sizes, tests, namespaces and every Jettison include;
`CMakeLists.txt`, `CMakePresets.json`, `cmake/ManifestModule.cmake`,
`source/external/CMakeLists.txt`; `data/build.sh` and authored YAML; `scripts/ci.sh`,
`format-check.sh`, `tidy.py`; `.github/workflows/ci.yml`; `AGENTS.md`, `README.md`, `.gitignore`,
`.vscode`, `.codex`; and the pinned versus checked-out engine `devenv.*` files. Not performed: any
build, test, format, tidy, benchmark or engine run; no line-by-line audit of renderer, physics, ECS
or marketplace algorithms; no C++ memory analysis. Quantities are `wc -l`/`grep` counts over the
working tree, excluding `.git`, `build`, `.devenv` and `.cache`. The link and anchor checker was run
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
