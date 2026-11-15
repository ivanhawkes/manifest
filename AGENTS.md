# Manifest

Manifest is a Jettison mod project. Read `docs/coding-standards.md`,
`docs/architecture.md` and `docs/build-and-gates.md` before changing it.
For current verification state, open handoffs and the CI failure analysis, read
`docs/project-analysis-2026-11-16.md`; for selected decisions and their status, read
`docs/decision-ledger.md`.

- Jettison's `feature-base` branch is the SDK reference; `main` may lag behind it.
  The sibling checkout is usually `../jettison`. The reproducible SDK commit is in
  `source/external/CMakeLists.txt`; `JETTISON_SOURCE_DIR` is an explicit local override.
- Keep `devenv.nix`, `devenv.yaml` and `devenv.lock` identical to the selected engine SDK.
  Reuse its dependency targets rather than fetching second copies or changing ABI defines.
- Follow the engine's naming, formatting, warnings and simulation rules. Keep authored
  YAML keys and IDs stable when renaming C++ identifiers.
- Build and test with the presets. `./scripts/ci.sh --werror --asan` is the local gate.
- Avoid running clang-tidy locally: the development machine is an underpowered laptop.
  It is opt-in with `--tidy`, enabled in CI. Default local builds to two compile jobs.
  The open handoff T9 authorises one local `--tidy` gate run to clear the two findings below.
- A mounted content package is not yet playable gameplay. Document the host/script boundary
  and the remaining fixed-point/VFS integration accurately; see `docs/roadmap.md`.

## Current verification and CI status (2026-11-16)

Read this before building, validating or reporting a gate result. Details, run IDs and the
handoff table are in `docs/project-analysis-2026-11-16.md`.

- **CI has failed on twelve consecutive runs of the live branches, and the build is not the cause.**
  Every run passes the format gate, the Debug build and all 11 CTest suites, then fails in the
  CI-only clang-tidy stage on exactly two findings: `source/game/types/include/manifest/types/Decimal.h:99`
  (`bugprone-implicit-widening-of-multiplication-result`, reported 16 times because
  `HeaderFilterRegex: 'source/.*'` scores every TU that includes the header) and
  `source/game/market/src/Marketplace.cpp:1688` (`performance-inefficient-string-concatenation`).
  `scripts/tidy.py` passes `--warnings-as-errors=*`, stricter than `.clang-tidy`'s
  `WarningsAsErrors: ''`. Both fixes are one-line and behaviour-preserving (handoff T9).
- **CI has produced no Release, ASan or UBSan evidence since 2026-11-14.** `scripts/ci.sh` runs
  tidy between the Debug and Release stages under `set -euo pipefail`, so each failing run aborts
  before those stages. Never describe CI as passing or failing "the build": name the stage that
  actually ran, and do not use a CI run as gate evidence for Release or the sanitiser presets.
- **HEAD is unverified locally.** The last recorded full gate (Debug, Release, ASan/UBSan, 11
  suites, state hashes) is at `5d7f54e`; the only non-document changes since are `AGENTS.md`,
  `.gitignore` and `devenv.nix` (`+lazygit`). No preset build tree exists locally and the
  top-level `build/` is the ignored pre-rename `CivSimLibrary` tree.
- **Next implementer action:** one gate run covering the baseline and tidy together,
  `./scripts/ci.sh --werror --asan --tidy --log build/gate-tidy.log`, with the two tidy fixes
  (handoffs T1 and T9), then record the outcome, log path and hashes in `docs/session-handoff.md`.
  Prefer `--skip-release` only for an intermediate pass; the recorded baseline needs all presets.
- Planning agents must not reason "CI is green, therefore validated", and must not call the current
  failures a build regression: they are tidy-only.

## Models and delegation

Before integration work, read `docs/engine-game-boundary-assessment.md`, the current
development-stage/gameplay plans and the selected SDK's `docs/game-integration.md` when
available. Manifest owns production rules and content; Jettison supplies genre-neutral
capabilities. Map required mechanics to public SDK APIs and record demonstrated gaps.
The implementer provisions reusable missing capabilities in Jettison, commits them and
updates the SDK pin; game rules, balance and authored values remain in Manifest. Use
Jettison YAML/archive/registry mechanisms rather than parallel custom loaders. Do not
include private engine app headers or depend on its sample economy. Jettison may retain
small gameplay examples for automated testing in separate sample/test targets and assets.

**OpenAI models are for planning only. They must not implement project work.** This applies
to the primary agent and all sub-agents, regardless of model, effort or role name, and
supersedes earlier recommendations assigning implementation to OpenAI models.

- Planning may include read-only repository investigation, architecture, task breakdowns,
  acceptance criteria, validation plans and handoffs to an implementer.
- Do not use OpenAI models to write or modify implementation code, tests, fixtures, assets,
  build scripts or configuration, or to apply fixes, refactors or mechanical migrations.
- Explicit requests to write planning or workflow documentation permit those documentation
  edits only; they do not authorize implementation.
- Do not assign implementation to the existing OpenAI-backed roles under `.codex/agents/`.
  Their presence and configured defaults do not override this policy. Implementation belongs
  to the user or a non-OpenAI implementer selected by the user; no replacement is assumed.
- Follow `docs/agent-workflow.md`. Delegate only bounded, read-only planning investigations
  when they reduce primary context or expensive-model work. Use fresh context where supported,
  supply relevant paths and a specific question, and request concise findings with references.
- Use at most two sub-agents concurrently. Sub-agents must not delegate further unless the
  primary explicitly assigns further delegation. Do not promise lower total tokens.
- OpenAI planning agents describe the required checks in the handoff; implementation and
  build/gate execution belong to the implementer. The two-job local compile limit and the
  restriction on local clang-tidy still apply to implementation work.

## Session context after roadmap tasks

After completing each roadmap task, recommend one of: continue this session, run `/compact`,
or start a fresh session with `/clear` or `/new`. Give a brief reason based on the next task's
dependence on the current discussion and the amount of stale investigation/build output.
Do not compact or clear automatically. Before recommending a fresh session, leave a concise
handoff with the current commit/branch, uncommitted work, decisions, validation and next step.
Distinguish conversation context from the managed KV/prompt cache; do not claim to measure,
flush or compact that cache, invent context-usage figures, or guarantee token/cost savings.
