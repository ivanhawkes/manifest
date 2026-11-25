# Coding standards

Manifest follows Jettison's [coding standards](../../jettison/docs/coding-standards.md) at the SDK
[pin](glossary.md#pin) (`MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`). This
file holds only how they apply here and where Manifest differs. It does not copy them, since a copy
drifts. Read them in the sibling checkout at the pin, or in `build/<preset>/_deps/jettison-src`.

## Manifest application

Engine paths and `JETTISON_` names refer to the SDK. Manifest uses:

- `<manifest/<module>/Name.h>` for its headers;
- `Manifest::Core` and the other `Manifest::` targets;
- `MANIFEST_` project macros and options;
- `manifest_warnings`, which links the engine's warning target privately.

Source categories mirror Jettison's:

- game modules under `source/game/`;
- applications under `source/apps/`;
- tools under `source/tools/` (the development console and the TUI);
- integration tests under `source/test/`.

Each game module owns a `CMakeLists.txt` and its public `include/manifest/<module>/`, with a
private `src/` and unit `tests/` where it has them: `config` is headers only, and the
`effects` suite lives in `source/test/`.

The TUI is not simulation code. It is a remote client shipped in the [bundle](glossary.md#bundle)
([D-012](decision-ledger.md#d-012), [D-114](decision-ledger.md#d-114)), and its session
transcript is UI output.

The development shell and the SDK pin move together ([D-006](decision-ledger.md#d-006),
[D-054](decision-ledger.md#d-054)). Do not add a second version of an engine dependency.

clang-tidy is opt-in locally (`--tidy`) and runs at the gate before a push, not in CI
([D-007](decision-ledger.md#d-007), [D-116](decision-ledger.md#d-116)). Build concurrency is set
per machine ([D-051](decision-ledger.md#d-051)). Both are in [build and gates](build-and-gates.md).

## Differences from the engine's rules

- **Numbers:** only per-unit market prices use a decimal type ([prices](pricing.md),
  [D-001](decision-ledger.md#d-001)). Quantities, population, effect stats, physics and other
  continuous values stay floats or doubles; whole-credit ledgers stay integers. The host adapter
  makes explicit any conversion an engine API needs. Do not expose the development classes
  wholesale as a save or script ABI. Keep authored IDs and YAML keys stable when renaming C++.
- **Determinism:** the engine's list of deterministic modules names engine modules. In Manifest,
  the game modules on the simulation's side are held to the same
  [rules](../../jettison/docs/coding-standards.md#simulation-code), under the contract of
  [D-002](decision-ledger.md#d-002).
- **Declaration spacing:** Jettison checks its
  [rule](../../jettison/docs/coding-standards.md#declaration-spacing) with
  `scripts/space-declarations.py`. Manifest has no such script, so here the rule is house style.
