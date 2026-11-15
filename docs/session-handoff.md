# Session handoff

## Current planning handoff — 2026-11-15

This section supersedes the historical resume instructions below. Stages 1–5 have
implementation plans, not completed implementations. OpenAI agents remain planning-only;
implementation and build/gate execution belong to Claude under the user's policy.

- Manifest is on `feature-base` at `2716380` (`Merging in WIP. Not sure of build status.`).
  The working tree was clean before this planning session; current build validity is unknown.
- Jettison is on `feature-base` at `826c4bec125b6631a6ba8ee822c90463316c3511`, initially clean.
  Manifest still pins `f85092027eda49c29001f7589608df1f0b8827fb`; the committed difference
  from that pin is only `.gitignore`. Jettison now has the uncommitted documentation below.
- Uncommitted Manifest work: five new `docs/stage-{1..5}-*-plan.md` files and
  `docs/engine-game-boundary-assessment.md`; updates to `AGENTS.md`, architecture,
  agent workflow, roadmap, development stages, standalone/host-script plans and this handoff.
- Uncommitted Jettison work: new `docs/game-integration.md`; updates to `CLAUDE.md`
  (`AGENTS.md` is its existing symlink), README/docs index, scope, architecture, roadmap,
  sample-game, coding standards and build/gates docs. Both repositories now instruct
  implementers to inspect Manifest requirements and provision reusable SDK gaps while
  retaining a separate small example for testing. No code, configuration or assets changed;
  no commits were made.
- Read [development stages](development-stages.md) for the index and execution order.
  Begin with [stage 1](stage-1-host-plan.md), including merged-baseline validation.
- User correction: retain `lazygit` as a development-machine tool for terminal change
  review. It is not a runtime requirement. Reconcile environment parity through the
  shared SDK development shell; do not simply remove it from Manifest.
- User direction: replace Manifest's custom YAML readers with Jettison's YAML archive,
  migration and definition mechanisms. Stage 2 specifies this migration; no parallel
  legacy runtime deserialiser is planned.
- User direction: Jettison must provide genre-neutral capabilities; Manifest owns its
  production gameplay and content. Engine-local gameplay for automated testing is allowed.
  The [boundary assessment](engine-game-boundary-assessment.md) finds partial separation
  and adds sample isolation, neutral consumer/build checks and optional space-module
  extraction to the stage plans. Do not remove useful engine gameplay tests or assume
  that moving files alone establishes separation.
- Preserve price-only Decimal, floating continuous quantities and whole-credit balances.
  Stages 3–4 specify company ownership, venue-local inventory and one authoritative world.
  Stage 5 proposes durable step batches and recovery; none of its guarantees is validated yet.
- Validation in this planning session: read-only source/API inspection, `git diff --check`
  in both repositories, and 157 local Markdown link-target checks with no missing targets.
  No builds, gates, benchmarks or crash/recovery tests ran. Detailed plans contain required
  implementer checks and explicitly identify proposed choices and remaining decisions.

Recommended next context: start a fresh implementation session with Claude using this
handoff and stage 1. The plans retain the decisions; the exploratory source excerpts
in the planning conversation are no longer needed for the first delivery slice.

## Historical handoff — superseded repository state

Prepared for `/clear` on 2026-11-15. Read `AGENTS.md`, `docs/coding-standards.md`,
`docs/architecture.md`, `docs/build-and-gates.md` and `docs/agent-workflow.md` before editing.

## Repository state

- Workspace: `/home/ivan/projects/manifest`.
- Branch: `feature-scaffold`.
- HEAD and `origin/feature-scaffold`: `5d7f54e84b854cbcc9ff43a3b133bc592c639bc6`,
  `Market prices: use decimal values while preserving floating quantities`.
  Commit and push succeeded before finalising this handoff.
- Previous baseline: `11de8f3`, shared-console TUI integration.
- GitHub reports the repository moved to `git@github.com:ivanhawkes/manifest.git`. The
  existing origin URL still names `civilisation-simulation.git`; its redirect accepted the push.
  The remote URL was not changed.
- Roadmap item 1 is committed as `09e01d8`; item 2 is committed and pushed as `5d7f54e`.
  All implementation and project documentation changes are committed. The only uncommitted
  file is this handoff, `docs/session-handoff.md`; keep it for resuming after `/clear`.
- SDK: Jettison `feature-base`, pinned at `f85092027eda49c29001f7589608df1f0b8827fb` in
  `source/external/CMakeLists.txt`. The sibling `/home/ivan/projects/jettison` matches the pin.
  Development environment files and SDK dependency definitions were not changed.

## Decisions that must survive the clear

The user's explicit correction was: **only market prices use decimal types; physics and other
continuous values use floats/doubles.** This supersedes the original all-integer economy plan.
Existing whole-credit balances, reservations, gross amounts and fees remain integers.
Quantities, population, production/demand, effect stats, ratios and freight/profit estimates
retain doubles. The abandoned broad fixed-point migration was reverted; do not reinstate it.

[Host/script boundary](host-script-boundary.md): authoritative economy, progression, effects
and settlement run in linked native game modules, eventually in one Jettison `SimWorld`.
Wasm supplies bounded policy inputs, starting with trade preference; initial event scripts
observe only. Reuse Jettison's VFS, definition registry, reflection, utility AI, command/replay,
save and script infrastructure. There is no native C++ mod ABI. Mounting content is not yet
playable integration.

[Decimal market prices](market-prices.md) records the implemented numeric contract:

- `Manifest::Core::Decimal`, in `source/game/types/include/manifest/types/Decimal.h`, stores
  signed 64-bit millionths of a credit per authored item unit. Integral construction means
  whole credits; `FromRaw` means microcredits; `_dec` literals parse decimal text exactly.
- Only base/market/local prices, route unit prices, order limits, venue minimum price,
  quote prices and fill unit prices use it. Authored IDs, YAML keys, units and prices are unchanged.
- `TryParse` rejects malformed, nonfinite, exponent, excessive nonzero precision and out-of-range
  text. `Generate` validates nonnegative base prices before replacing a world, with headroom
  for the maximum local multiplier. Configuration still retains authored price strings.
- Scarcity/local coefficients remain double. `TryFromDouble` explicitly rounds computed prices
  to the nearest microcredit, halfway away from zero. Price comparison and levels are exact.
- Mixed quantity/price settlement explicitly uses `price.ToDouble() * quantity`, preserving
  existing double-product rounding. Market buy reservations ceil whole credits, fill gross
  rounds nearest, and fees ceil. The separate development simulation still truncates route
  costs/revenue. No microcredit balances or cumulative per-order rounding were introduced.
- Checked cost/fee conversion and ledger preflight prevent overflow before writes. Cancellation
  and expiry batches preflight combined refunds and process stable order IDs. Representable
  high fee rates are preserved; there is no new silent fee cap.

## Committed implementation

New files: `docs/market-prices.md`, the Decimal header and implementation under
`source/game/types/`, and `source/game/types/tests/DecimalTests.cpp`.

Modified code: price fields in ECS components; simulation and market APIs/implementations/tests;
their CMake dependencies; the types module/test registration; data assertions and the shared
test helper; and TUI price display types. Effects implementation and physical representations
are unchanged. Documentation, including the roadmap and coding-standard override, is updated.
Use `git show --stat 5d7f54e` for the implementation file list.

Review findings about double-product rounding, uncapped fees and batch refund atomicity were
fixed and re-reviewed with no remaining material findings. New tests cover exact authored and
adjacent prices, invalid-price rollback, rounding, overflow, refunds and conservation.

## Validation

The complete local gate finished successfully:

```bash
devenv shell -- env CMAKE_BUILD_PARALLEL_LEVEL=2 CTEST_PARALLEL_LEVEL=2 \
  ./scripts/ci.sh --werror --asan --log build/gate-prices.log
```

Formatting and all **11 suites passed in Debug, Release and ASan/UBSan**. No local clang-tidy
was run. The log is an ignored build artifact, not a committed fixture. `git diff --check` and
local Markdown link checks also passed. Subsequent changes only marked completion in docs.

Original simulation state hashes remain unchanged: generation `10355632731109243635`,
100 ticks `12410383246888723003`. The new raw-price hash is `15444164946577136117` at both
points; current prices depend on unchanged production/demand rates. All presets agree.

Use the locked devenv shell for builds. The ordinary tool shell lacks compiler/Python commands;
`.devenv/profile/bin/python3` and `.devenv/profile/bin/clang-format` work for small local tasks.
Stale compiler paths initially forced a sanitizer cache refresh and dependency rebuild; that
is resolved. The final sanitizer cache has Debug, `-fsanitize=address,undefined`,
`-fno-sanitize-recover=all` and `MANIFEST_WERROR=ON`. Do not repeat the full gate merely to
reconstruct conversation context; rerun checks when new changes warrant them.

## Next work

Roadmap items 1 and 2 are complete under the corrected price-only scope. The next implementation
task is item 3: replace host-path configuration loading with Jettison VFS and typed definition
registry loads, including layered patches, schema versions, migrations and directory/pack tests.
Current loaders still take host paths. Fixed-step host state/commands/saves remain item 4;
scripts/assets and a playable engine-hosted scenario remain item 5.

Start item 3 from the committed, validated price patch. Delegate bounded inventories when useful,
keep design/integration with the primary, use at most two workers, schedule builds only from the primary with two
compile jobs total, and avoid local clang-tidy. No worker from the previous session is needed.

Suggested resume prompt:

> Continue Manifest on feature-scaffold at 5d7f54e (pushed). Read AGENTS.md and
> docs/session-handoff.md. Roadmap items 1 and 2 are complete and validated.
> Only market prices use Decimal; quantities, physics and other continuous values remain
> floats/doubles, with existing whole-credit ledgers unchanged. Continue roadmap item 3
> using Jettison's existing VFS/definition infrastructure.
> Avoid local clang-tidy and limit builds to two jobs.
