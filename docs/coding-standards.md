# Coding standards

Manifest follows Jettison's [coding standards](../../jettison/docs/coding-standards.md) at the
SDK pin (`MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`). This file holds only
how they apply here and where Manifest differs. It does not copy them: a copy drifts. Read them
from the sibling checkout at the pin, or from `build/<preset>/_deps/jettison-src`.

## Manifest application

Engine paths and `JETTISON_` names refer to the SDK. Manifest uses:

- `<manifest/<module>/Name.h>` for its headers;
- `Manifest::Core` and the other `Manifest::` targets;
- `MANIFEST_` project macros and options;
- `manifest_warnings`, which links the engine's warning target privately.

The TUI is development tooling, not simulation code; its session transcript is UI output.

Source categories mirror Jettison:

- game modules under `source/game/`;
- tools under `source/tools/`;
- integration tests under `source/test/`.

Each game module owns a CMakeLists.txt and its public `include/manifest/<module>/`, private
`src/`, and unit `tests/`.

The development shell and the SDK pin move together ([D-006](decision-ledger.md),
[D-054](decision-ledger.md#d-054)). Do not add a second version of an engine dependency.

Clang-tidy runs in CI and is opt-in locally ([D-007](decision-ledger.md)), and build
concurrency is set per machine ([D-051](decision-ledger.md#d-051)).

## Differences from the engine's rules

- **Numbers:** [decimal market prices](market-prices.md) ([D-001](decision-ledger.md)). Only
  per-unit prices use a decimal type. Quantities, population, effect stats, physics and other
  continuous values keep floats/doubles. Whole-credit ledgers stay integers. The host adapter
  makes explicit any conversion an engine API needs. Do not expose the current development
  classes wholesale as a save or script ABI. Preserve authored IDs and YAML keys.
- **Determinism:** the engine's list of deterministic modules names engine modules. In Manifest,
  the simulation-side game modules are held to the same rules, under the contract of
  [D-002](decision-ledger.md).
- **Declaration spacing:** the engine enforces it with `scripts/space-declarations.py`. Manifest
  has no such check, so the rule applies here only as house style.
