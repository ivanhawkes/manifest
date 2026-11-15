# Session handoff

## Stargate lifecycle and competing-access follow-up — 2026-11-15

Resumed planning on `feature-base` at `df12cbf`, pushed to origin, with a clean working tree.
The user selected a fixed pair of stargates per connection. Each endpoint serves its named
connection; another destination needs another pair. This does not select a per-system gate
cap or resolve later retargeting/transfer rules. The prior decisions below remain in force.

Current uncommitted changes are planning documentation in this handoff and the
[stargate plan](stargate-expansion-plan.md). The user also accepted manufacturing both
endpoints at an accessible origin, installing one and sending the other with self-contained
unmanned propulsion/power/deployment equipment. Activate only when both endpoints are
commissioned/ready; disclose flight arrival separately from trade opening. Initial ownership
is both endpoints held by the government/corporate sponsor, as public/company assets
respectively. Equipment costs, operating supply quantities and repair terms remain unresolved; no destination
workers or existing port are required for
the selected autonomous deployment model.

The user also accepted the launch equipment/funding model: authorise/reserve a capped project
budget, paying for delivered equipment and completed services during preparation. Launch only
after the origin gate is commissioned and the remote gate, carrier, FTL drive, power, controls,
deployment equipment and consumables are physically ready. Fund remaining contracted flight/
deployment services before departure; the launched mission needs no routine cash top-ups.
Pause preparation overruns for an explicit budget increase. Pre-launch cancellation releases
uncommitted funds while preserving equipment ownership, paid work and awarded supplier
obligations. Exact quantities and credit costs remain balance work; post-launch recall/
cancellation remains unresolved. Initial operating rules are selected below.

The user accepted two server-running hours for remote deployment/commissioning as an initial
balance-testing value quoted before launch. Work starts after arrival when equipment can
operate; shutdown/equipment interruption pauses progress, disconnection does not. Preserve
completed work through interruptions/restarts. An unavailable origin delays opening after
remote commissioning; both endpoints must be ready. The carrier is a one-way deployment
package whose useful power, controls and installation hardware become part of the remote
gate, with its cost included in the expedition budget. Initial upkeep is selected below.

The user accepted six server-running flight hours per light-year as the initial balance-testing
rate, freezing each expedition's quoted rate/duration at launch; later tuning affects future
launches. Examples: 2/4/8 light-years take 12/24/48 running hours in flight, plus two hours of
uninterrupted commissioning when otherwise ready. Preparation and interruptions add time.
Authored speed/pacing and unit conversions must realise the selected rate; no settings changed.

The user also selected one protected return crossing for the same ship after entry through
an operational gate, even if its owner later bans the company. This is ordinary gate traffic,
not the one-way delivery carrier. The user accepted the detailed permit model: quote and reserve
outward/return fees together before entry; charge each once at transit commitment. Honour
accepted terms after later price increases/bans; new permits use current policy. Bind the
return to the same ship/pair, without time expiry while it remains in the destination system.
The first departure closes the visit. Return through that pair consumes the right without
creating another; leaving by another route releases unused return funds. Ownership transfers,
detailed repair/compensation and quote validation/fee amounts remain unresolved. The right
does not grant unlimited re-entry or recursively create another return permit.

The user accepted initial per-crossing energy/supply costs, deferring recurring idle upkeep.
At admission the owner reserves actual physical supplies for the protected return until the
visit closes; the ship's fee reservation does not substitute for that stock. Shortages stop
new admissions while preserving return supplies. Genuine equipment outages visibly block
service, preserving rights/funds until repair. Owners may close to new business immediately,
but honour accepted passages and returns; dismantling waits for their discharge. With no
return expiry, retirement can wait indefinitely for a visiting ship. No forced departure,
random breakdown model or safe topology removal is established; detailed repair/compensation,
resource quantities, maintenance and salvage remain design work.

The user accepted independent rival gate pairs into already-connected systems, without
incumbent gate-owner permission. Several independently owned pairs may serve a system,
including parallel links between the same systems. A rival may deliver its endpoint by its
own funded unmanned FTL expedition under the same travel/commissioning rules; an incumbent
gate ban cannot block that flight. Each owner controls its own access/fees. Gate construction
does not bypass protected planets or active resource concessions. Site eligibility/placement,
capacity limits and possible delivery through existing gate networks remain unresolved.

Additional pinned-SDK evidence: public `Gates::DefineStarSystem` rejects redefinition and
`MakeGates` rejects repeat creation for a system. Further endpoints in an existing system
therefore need a demonstrated public lifecycle or minimal reusable Jettison provision, not
private app access or a duplicate loader. The capability map and required consumer/recovery
checks are updated; no engine change or execution evidence is claimed.

No implementation, SDK/environment changes, builds or gameplay validation occurred.
The main stargate lifecycle/access decisions are recorded, with exact materials/prices,
operating/repair recipes, site rules, transfer/recall, quote validation and SDK integration
still pending. Documentation checks: `git diff --check` and 18 local Markdown link/heading
targets across the two changed documents passed. Only this handoff and the stargate plan are
uncommitted; `df12cbf` remains the pushed baseline.

Recommended next planning topic: long-running demand/public-income circulation, including
funded consumer demand, producer income and public-budget replenishment. Those rules remain
unselected; no household model, tax or currency issuance is implied by the gate decisions.
Recommend `/compact` before that topic: the decisions are preserved here, while earlier gate
API excerpts and repeated documentation-check output are no longer needed for the next design.

## Contract, frontier, first-tech and stargate follow-up — 2026-11-15

Resumed from Manifest `feature-base` at `066b5cc`, with a clean working tree. This follow-up
updates supplier security, construction-site, exclusivity, first-tech and stargate decisions in the product-planning
handoff below; its other constraints and decisions remain in force. Current changes are
planning documentation only in this handoff, the contract, planetary-development and
economy/development plans, the development-stage index, roadmap, architecture, gameplay/server
plan and docs index, plus the new stargate-expansion plan.
No implementation, SDK/environment change or build/gate execution occurred.
Documentation checks: `git diff --check` and local Markdown link/heading-target checks passed.

The user accepted a supplier-funded performance bond, initially 10% of the winning offer
subject to balance testing. Publish its requirement with the tender; lock it from unreserved
company funds at award, considering the next qualified bidder if funding fails. Hold it until
contract close while continuing immediate payment for accepted partial deliveries. Complete
delivery returns it all. Abandonment or missed deadline forfeits the undelivered proportion
to the public treasury and returns the rest; accepted goods and payments remain settled.
NPCs and the governor's company use the same rules; disconnecting changes none of them.
See [performance security](procurement-and-station-contracts-plan.md#performance-security-selected).

Whole-credit bond rounding/minimums and issuer/service-failure handling remain design work;
the bond is not a guarantee of replacement-supplier affordability. No economic validation
has occurred.

The user also accepted the [construction-site reservation rules](planetary-development-plan.md#company-built-stations-and-control-of-shipping):
the first eligible company starting a funded project secures the site, paying meaningful
non-refundable mobilisation and reserving construction funds. A fixed completion deadline
cannot be renewed indefinitely through token deliveries. Abandonment/expiry reopens the site;
unspent funds and unused goods remain company property. Completed-work salvage, exact amounts,
funding scope, mobilisation beneficiary, deadline clock/duration and intervening control changes
remain unresolved. The reservation grants construction priority only; resource exclusivity and
government control need separate grants. No anti-squatting or recovery result has been tested.

The user accepted the [exclusivity trigger and provisional term](economy-and-development-plan.md#real-time-exclusive-exploitation):
start when the first eligible frontier spaceport becomes operational and its exploitation
concession activates. Publish the duration before construction; use 72 hours of actual
server-running time as the initial balance-testing value. Shutdown pauses it; idle facilities
and disconnected owners do not. Ownership transfers, rebuilding and government changes cannot
restart or extend it. At expiry rivals may exploit and build their own ports; incumbents keep
their infrastructure and may refuse new bookings at their own port. Protected homes keep
separate rules, and the concession does not transfer government control. Construction costs,
extraction capacity and pacing must validate the term; no such validation has occurred.

The user accepted [Bronze metallurgy](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
as the first material/facility tech example. Require basic smelting knowledge and an operational
local metalworking workshop, seeded explicitly for the separate governance fixture. Deliver
90 kg copper and 10 kg tin into public project inventory, with local copper and competitively
procured/physically imported tin from an explicit external supplier. Consume the project inputs
once, then advance fixed-step work; workshop interruption pauses progress. Completion unlocks
local production, with each fresh batch consuming 9 kg copper plus 1 kg tin into 10 kg bronze.
Knowledge survives workshop loss; production needs an operational workshop. Reuse existing
`SkillSmelting`, `ItemCopper`, `ItemTin` and `ItemBronze` IDs, preserving authored data and the
separate eight-good trading fixture. Work duration, throughput, profitability and detailed
facility/batch terms remain unresolved; the quantities are initial test values. No content,
fixture, code or tests changed, and no gameplay validation ran.

All four initial follow-up decisions are recorded. The user subsequently required government
or corporate funding of physical stargate delivery before opening a new system to trade and
development. Flights are fully automated/unmanned at a multiple of lightspeed, with meaningful
server-running wall time. The user chose distance-based duration, tuned through configurable
FTL speed/pacing to hours or days, and owner control of operational gate access/fees, including
refusing rivals. See the new [stargate handoff](stargate-expansion-plan.md).

Gate delivery/activation does not grant government control or start planetary resource
exclusivity. SDK inspection at the existing pin confirms that `PlotRoute`/`EnterLane` require
an existing lane, while `OnLane` uses simulation-step arrival; they do not establish the
required bootstrap expedition or server-running timing contract. Gate toll refusal/charge
callbacks exist; per-owner policy integration and atomic topology/recovery remain to prove.
Jettison stays unchanged at `0564f19`; the SDK pin and environment are unchanged. Only read-only
source investigation and documentation checks occurred, with no builds or gameplay validation.

Next stargate choices: endpoint pairing/deployment, launch cost/equipment, exact speed/pacing,
gate commitments/exit rights and rival access alternatives when a gate owner refuses service.
Target generation/reveal triggers, detailed construction/colonial/transfer contracts and
long-running demand/public-income circulation remain open. Continue this session while the
physical economy and access constraints are fresh; no context reset is needed yet.

## Current product-planning handoff — 2026-11-15

This section supersedes the earlier planning/repository state below. The planning baseline was
`feature-base` at `8f02d54c2935dc929052a4e753c66bcf9f777a5c`; Jettison is on `feature-base`
at `0564f19ed41bc7e0a278952153567d4a137a2f5b`. Both started clean. The SDK pin remains
`f85092027eda49c29001f7589608df1f0b8827fb`; no SDK/environment changes were made.
The user requested committing and pushing the Manifest documentation at session close;
the resulting planning commit is the commit containing this handoff. Jettison is unchanged.

Current work is Manifest planning documentation only: a new
[planetary development/corporate-power assessment](planetary-development-plan.md),
[supply/station contract handoff](procurement-and-station-contracts-plan.md),
[physical economy/development plan](economy-and-development-plan.md) and aligned
roadmap, gameplay/server, development-stage, architecture, scope, stage 3–5 and index/handoff
guidance. No implementation, assets, fixtures or configuration changed. No builds, gates,
benchmarks, economy simulations or recovery checks ran; build validity is not established.

Confirmed direction to preserve:

- Dedicated headless server; fixed-step world, NPCs, autopilot, enabled auto-trading and
  development progress with zero clients. Server downtime still pauses simulation initially.
- Existing TUI becomes a required remote gameplay client with `connect`/`disconnect`; GUI
  shares the command/query layer and uses selected public engine features. Manual TUI
  spaceport trading is a stretch goal. Administrative permissions remain separate.
- Each player initially directs one planet's development and public budget alongside a
  separate trading company. AI governs others; disconnecting does not relinquish control.
- Acquire more control through eligible AI-planet purchases or frontier settlement; protect
  other players' planets from that acquisition mechanic. Keep initial/late-join starter
  capacity separate from the acquisition pool and an AI economy.
- Endlessly accumulate corporate wealth, power and control, seeking a stranglehold over
  trade, economies, governments and media. There is no win condition or automatic victory
  reset. Days versus months is unresolved server lifespan/pacing, not a victory timer.
- First development controls: projects, investment budgets and public procurement. First
  corporate leverage: infrastructure, essential supplies and access contracts. Powerful
  companies can build/own stations, set fees and shipping priorities, and refuse rivals
  access. Government financing and media mechanics follow later.
- Public procurement requires competitive bids, including the governor's own company.
  Plan a simple government corruption trait and abstract bribery after clean tendering works;
  detailed officials/political simulation is deferred. This supersedes the earlier open
  question about directly awarding public contracts to one's own company.
- Pay suppliers for accepted partial deliveries as they arrive. Honour accepted station
  bookings under their agreed access/fee/priority terms; owner changes affect future requests.
  The new [contract/booking handoff](procurement-and-station-contracts-plan.md) proposes sealed
  bidding, a clean price-based award, cumulative total-contract payment and bounded visits.
  Those mechanics are proposals, not implementation or changes to marketplace settlement.
- Credits move instantly between authorised accounts; goods move physically from extraction/
  industry through haul, station warehouse, ship, destination warehouse and local delivery.
  Brokers trade their own finite funds/stock and earn spreads; warehouses charge handling
  separately at both ends. Production limits new output, stock limits offers, scarcity/demand
  determine prices; explicit production-based price bands were not selected.
- Planet tech advancement needs exploited/imported materials and actual production facilities.
  Generate higher-tech/resource-depleted inner worlds and richer, less developed outer worlds.
- Frontier resource rights are initially exclusive for elapsed real server-running time,
  pausing during shutdown. Afterwards rivals may exploit and build their own spaceports even
  under the incumbent's puppet government. Starting homes remain protected. This supersedes
  the permanent no-second-port rule on reopened concessions. The operational-port/concession
  trigger, provisional 72-hour term and construction-site reservation are selected in the
  follow-up above.
- Corporations may build a very expensive orbital spaceport at any planet without one that
  is not owned/controlled by another player. This enables resource exploitation and colonies
  with company-controlled puppet governments, supporting long-term corporate returns.
  Resource/colonial authority and control of a pre-existing AI government still need design.
- Later frontier systems extend the existing map with generally less developed and some
  uninhabited worlds. Terraforming, settlement, expansion triggers and capacity are unproven.
- Keep player/company/planet/civilisation/control identities and public/company finances
  explicit. Retain price-only Decimal, floating continuous quantities and whole-credit ledgers.
- Preserve genre-neutral engine provision, public SDK boundaries, pinned dependency/environment
  parity, existing YAML/archive/VFS/registry mechanisms and the OpenAI planning-only policy.

Proposed delivery amendment: retain the five foundation plans as work packages, but prove a
real TUI connection after minimal authority; then idle physical trading, governed development
and recovery. Multi-planet acquisitions, specific government/media influence and frontier
expansion follow. The tiny two-venue fixture remains useful; add a separate governance fixture.
The old optional administration-only TUI and permanent AI-only development assumptions are
superseded. The exact extended implementation schedule is still a planning proposal.

Inspection found no implemented networking/session layer in Jettison. System registration,
route plotting and saves exist, but do not establish atomic live topology expansion.
Physics system definitions are not saved/hashed; generated topology must be restored through
a versioned world-data contract before loading entities/transit. See the new capability map.
Docking lacks generic admission/owner-priority hooks; normal ship authority checks are not
station entry permission. Reusable policy seams belong in Jettison, corporate rules in Manifest.

Documentation validation for this update: `git diff --check` passed, and a read-only check of
14 changed Markdown documents found no missing targets among 114 local links and two heading
anchors, no trailing whitespace and no missing final newlines. Jettison remains unchanged.

The running-time host bridge must persist monotonic elapsed observations with authoritative
state/journal batches and record expiry events for deterministic replay. Do not use an
absolute UTC deadline that deducts offline time or read a clock from deterministic systems.
Specify checkpoint loss after a crash rather than promising exact crashed-process uptime.

Next planning cycle, updated by the follow-up above: supplier security and construction-site
reservation rules, exclusivity trigger/provisional term and first material/facility example
are selected; continue with long-running demand/public-income circulation. Station booking
protection, partial payment and post-exclusivity rival
ports are approved; detailed settlement, service windows, corruption scoring, colonial powers
and long-running demand/public-income circulation remain design work. Initial starter
protection/viability still needs explicit allocation policy; unassigned AI planets with no
ports are eligible for initial corporate construction under the rule.
Acquisition pricing, influence rules, abandonment policy, frontier triggers and server pacing
remain open. Implementation/build/gate work belongs to the selected implementer; no such work
is authorised for OpenAI agents. The user ended this planning session. Start a fresh session
next time using this handoff and the linked plans; decisions are preserved and earlier
repository excerpts are no longer needed. No implementation or gameplay validation is complete.

## Earlier foundation-planning handoff — superseded state

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
