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
- Avoid running clang-tidy routinely on the underpowered laptop.
  It is opt-in with `--tidy`, enabled in CI. Build concurrency is machine-specific;
  follow [D-051](docs/decision-ledger.md#d-051) and
  [the concurrency policy](docs/build-and-gates.md#machine-specific-concurrency).
  T9's authorised combined local `--tidy` gate has completed; see the verification record below.
- A mounted content package is not yet playable gameplay. Document the host/script boundary
  and the remaining fixed-point/VFS integration accurately; see `docs/roadmap.md`.

## Agent Directions

Answer concisely.

## Environment & Tooling Instructions (NixOS + Niri + devenv.nix)

You are operating inside a restricted `devenv.nix` shell environment on **NixOS** running the **Niri** Wayland compositor.

### 1. NixOS / devenv.nix Constraints

- **Do not** attempt to install global packages via `apt`, `brew`, `pip`, `npm -g`, `nix-env`, or `sudo`. All compilers, build tools, and language servers are declaratively managed by `devenv.nix`.
- **Do not** hardcode FHS paths like `/usr/bin/...` or `/bin/bash`; rely strictly on `$PATH` provided by the active `devenv` shell.
- If a required CLI tool, LSP binary, or C++ library is missing from `$PATH`, stop and report the exact package name needed so I can add it to `devenv.nix`.

### 2. Mandatory `compile_commands.json` Generation

Before running any C++ code analysis, refactoring, or LSP queries:

1. Check if `compile_commands.json` exists in the repository root or active CMake build directory (`build/compile_commands.json`).
2. If missing, inspect `CMakeLists.txt` (and `CMakePresets.json` if present) and ensure `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` is enabled during CMake configuration (e.g., `cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` or `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` in `CMakeLists.txt`).
3. Symlink or copy `build/compile_commands.json` to the project root (`ln -sf build/compile_commands.json compile_commands.json`) if it is not already accessible there so LSPs and static analyzers detect it automatically.

### 3. Language Services (LSP) & Code Intelligence

You are running in a terminal TUI environment (Claude Code, Codex CLI, or Pi). Enable and prioritize native LSP / code-intelligence tools over brute-force text `grep` across our 100K-line codebase whenever those capabilities or extensions are available in your harness:

- **C++:** Use `clangd` (backed by `compile_commands.json`) for symbol definitions, references, type hierarchy, and diagnostics.
- **YAML:** Use `yaml-language-server` for schema validation and key/structure checks on engine configuration and entity definitions.
- **CMake:** Use `cmake-language-server` / `neocmakelsp` for target, variable, and dependency resolution.
- If your current TUI agent lacks direct MCP/LSP tool bindings in this session, run `clangd --check=<file>` or use `compile_commands.json` flags directly via bash before falling back to targeted `rg` searches on `.hpp` headers.

## Coding standards

Read [coding standards](docs/coding-standards.md) before any work that changes code, scripts,
build files or data files, and review against it. It is the single source of truth for naming,
comments, ownership, errors, logging, determinism, dependencies, testing and commits; this file
does not restate it.


### Enforcement Protocol (Claude & Codex)
- **Before writing**: Express intent through C++20/23 types (`std::span`, `concepts`, strong typedefs, `[[nodiscard]]`) and explicit YAML key naming before resorting to a comment.
- **During refactoring**: Delete or update any adjacent comment whose underlying invariant changed.
- **During review**: Flag and strip any newly generated comment that restates syntax or schema structure.

## Current verification and CI status (2026-11-16)

Read the current [T1/T9 verification record](docs/session-handoff.md#t1t9-local-verification--2026-11-16)
before building, validating or reporting a gate result. The project analysis retains the
historical CI diagnosis and the remaining handoffs.

- **The two tidy findings are fixed, and the combined local gate passed.** Presets, suite
  results, SDK revision, replay hashes and the log path are recorded in the handoff. This
  is model-library verification; Stage 1 engine-hosted gameplay is still unimplemented.
- **Local gate results do not establish CI results.** Inspect the exact pushed commit's
  Actions run and logs before reporting its CI status. The pre-fix failure streak in the
  [analysis](docs/project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16) is historical.
- **T10 remains open.** Tidy still runs before Release and ASan/UBSan, so a future tidy
  failure can prevent those stages from running. Name the stages that actually ran;
  do not describe a tidy-only failure as a build regression.
- **Next implementer work:** the remaining T10, T5 and T7 handoffs, then Stage 1, following
  the analysis priorities. SDK/environment parity remains unresolved under T5; the local
  gate does not settle it. Avoid repeated gates without a new change or unresolved failure.
- **Tool output in this project has emitted identifiers that do not exist.** The 2026-11-16 analysis
  saw two CI run records (43806887285, 43806869597, "created 2026-11-16T04:21Z") and a push rejection
  (`9ea337b..7c0a307`, "Docs: record the running-world economy decisions") that resolve to nothing on
  GitHub: the records 404, `gh api` reports _No commit found for SHA_ for both SHAs, no forced push
  event exists, and `FETCH_HEAD` plus the remote commit list show linear history to `531295e`. Nothing
  was lost. Confirm any externally-reported run or commit identifier against a second source (`gh api`,
  `git ls-remote`, `FETCH_HEAD`, the remote's commit list) before reporting it or writing it into docs;
  see the data-quality note in
  [CI failure analysis](docs/project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16).
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
  build/gate execution belong to the implementer. The machine-specific concurrency policy
  and the restriction on local clang-tidy still apply to implementation work.

## Session context after roadmap tasks

After completing each roadmap task, recommend one of: continue this session, run `/compact`,
or start a fresh session with `/clear` or `/new`. Give a brief reason based on the next task's
dependence on the current discussion and the amount of stale investigation/build output.
Do not compact or clear automatically. Before recommending a fresh session, leave a concise
handoff with the current commit/branch, uncommitted work, decisions, validation and next step.
Distinguish conversation context from the managed KV/prompt cache; do not claim to measure,
flush or compact that cache, invent context-usage figures, or guarantee token/cost savings.
