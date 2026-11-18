# Next development stages

Approved by the user on 2026-11-15. Implementation follows the
[planning-only model policy](agent-workflow.md#planning-only-policy). These stages
refine the [standalone mod plan](standalone-mod-plan.md) and
[gameplay and persistent server plan](gameplay-and-server-plan.md) into the next delivery
sequence, originally a local persistent trading game before networking. The subsequent
amendments below supersede that networking deferral and add planetary/corporate goals.
They are planned work,
not completed capabilities. Each stage should leave a working, testable result and be
split into small commits.

This is the near-term execution order; stage numbers here are distinct from the broader
standalone plan's workstreams. Preserve the product decisions and host/script contract
in the linked documents. Policy scripts and graphical presentation follow the proven
native trading loop.

The [prioritised delivery order](delivery-order.md) breaks these stages and their
maintenance/amendment tasks into a dependency-ordered implementation queue. Use it
to choose the next task; the detailed stage plans retain their acceptance contracts.

Status: see the [delivery order](delivery-order.md#status-2026-11-18).
The [project analysis](project-analysis-2026-11-16.md) retains the historical diagnosis.
The amendments below are read together with the
[decision ledger](decision-ledger.md), which carries each selected decision once with its status,
owner and stage; the roadmap's delivery sequence governs ordering, and a new pillar does not
displace Stage 1.

## Subsequent product goals and delivery amendments

The [planetary development and frontier plan](planetary-development-plan.md) records the
current direction: a headless authoritative idle world; required remote TUI `connect` and
`disconnect` gameplay; a GUI sharing the server interface; NPCs, autopilot and physical
auto-trading operating without clients. Manual TUI spaceport trading remains a stretch goal.

The [regional correction](regional-economy-and-governance-plan.md) requires multiple governed
economic regions on one physical planet in the first governance scenario. Players start
with development/public-budget control of one region alongside a separate trading company.
AI governs others. Region-scoped population, household/public accounts, condition, local
projects and employer-region tax carry replace planet-scoped state. AI acquisitions cannot
take player-governed regions, but otherwise eligible AI neighbours remain purchasable
without a sibling governor's veto. Public money and private owners remain separate.
Concessions cover eligible resources in one named frontier region, with independent neighbour
eligibility/timers and government control separate. Port construction has one initial opportunity
per eligible region, without a neighbouring port blocking it. A port may serve several regions
on that planet under owner terms, preserving bookings, physical delivery and separate
economies/rights/construction opportunities. Allocate starting regions only with an operational
starter-compatible public station and physical delivery connection. Also require available
working station life support and spawn the new player connected. The starter receives
30 kg oxygen and one fresh installed 30-occupant-day cartridge once, in dedicated service
space outside its trade hold. Trade supplies/loose spares use cargo volume; storage
dimensions, station provisioning and later outages remain open. Guarantee continuing basic
docking/trading/regional delivery access against corporate revocation, retaining charges,
finite capacity/goods/funds and private-port policies. Detailed transport and starter
fees/capacity/funds remain open.
One controlled district does not confer whole-planet sovereignty. The world must
provide eligible home-region capacity for its group and later joiners, plus an AI economy.
Optional later expansion adds less developed or uninhabited systems at the existing map edge.

There is no win condition. Companies accumulate wealth and control, seeking power over trade,
economies, governments and media. Server lifespan and pacing over days/months are undecided;
no timed victory or automatic reset is established.

First development decisions are projects, investment budgets and public procurement.
Public contracts require competitive corporate bids. Plan a simple government corruption
trait and abstract bribery after the clean tender/fulfilment lifecycle works; detailed
official/political modelling is deferred. The governor's own company must bid too.
Corporate leverage starts with infrastructure, essential supplies and access contracts.
Company-built stations may set fees/shipping priorities and refuse rivals access. Their
construction, ownership and policy are game rules; reusable docking admission/queue hooks
must be provisioned in Jettison. Government financing and media influence follow later.
A corporation may build a very expensive orbital spaceport for its own, an eligible AI or an
eligible unclaimed region. There is one initial construction opportunity per eligible region;
neighbouring ports do not block it, and other players' governed regions are protected against
unsolicited builds. It opens resource exploitation and colonies
with company-controlled puppet governments. Prove this on a static map before requiring live
frontier generation; construction rights and planet-wide control are distinct contracts.
Later new-system access requires the [stargate expedition](stargate-expansion-plan.md): a
government/corporation funds an automated unmanned FTL delivery, with distance-based duration
tuned through speed/pacing to hours/days of server-running wall time. An operational gate opens
the connection; its owner controls access and fees, including refusing rivals. This is separate
from planetary ports, control and exploitation grants and does not delay the first static-map loop.
The selected [site reservation rules](planetary-development-plan.md#company-built-stations-and-control-of-shipping)
give the first eligible funded project construction priority, with non-refundable mobilisation,
reserved construction funds and a fixed completion deadline. Abandonment/expiry reopens the
site; unspent funds and unused goods retain company ownership. Exact amounts, deadline timing
and completed-work salvage remain unresolved; resource/government grants are separate.

Further refinements: [physical economy/development](economy-and-development-plan.md) requires
instant credit settlement, full commodity transport through warehouses, finite principal
brokers, spreads plus separate handling charges, and production-capped supply/scarcity pricing.
Planet tech requires local/imported materials and built production facilities. Generate
higher-tech/depleted inner worlds and richer, less developed outer worlds. Frontier resource
exclusivity lasts real elapsed server-running time, pausing during shutdown; rival ports and
exploitation become permissible afterwards even under puppet governments. Starting homes stay
protected. These refinements supersede the permanent no-second-port interpretation on frontier
concessions. The host records timing inputs/expiry for replay; deterministic systems read no clock.
The selected exclusivity grant starts when the first eligible frontier port becomes operational
and its exploitation concession activates. Disclose its duration before construction; the
initial balance-testing term is 72 server-running hours. Idle facilities and disconnected
owners do not pause it; ownership transfers, rebuilding and government changes cannot renew
or extend it. The concession does not itself transfer government control.

The first selected [tech example](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
is Bronze metallurgy in the separate governance fixture. Basic smelting knowledge, a seeded
operational local workshop and delivered public project inputs of 90 kg copper plus 10 kg tin
gate timed work. Use local copper and physically imported tin. Consume project inputs once;
workshop interruption pauses work. Completion unlocks local production batches of 9 kg copper
plus 1 kg tin into 10 kg bronze. Knowledge survives workshop loss; production still needs an
operational workshop. Quantities are starting test values; work duration, throughput and
profitability remain unvalidated. Preserve the separate eight-good trading fixture.

The first household basket has three equally weighted needs: staple food satisfied only
by rice, wheat, oats or barley; drinking water; basic medical supplies. The four tradable food
grains are alternatives within one need and distinct from planting seed; bread remains
tradable outside this basket. Food grains trade in kilograms; 1 kg of any grain meets 1 kg
of food demand, including mixtures. Keep separate stocks/prices/storage and debit actual
grains once without creating a food commodity. Population counts people in aggregate;
consumption rates are per 1,000 people per simulation day, scaled by population and elapsed
simulation time with fractional quantities retained. Any supported migration must explicitly
convert legacy abstract world units. Initial food demand is 500 kg per 1,000 people per
simulation day across the qualifying grains. Drinking-water demand is 3,000 litres per
1,000 people per simulation day, separate from agricultural/industrial water demand.
Preserve physical volume through conversion to selected inventory units. Remaining item
authoring values and population sizes need selection/
implementation through the stage 2 content path.

Basic Medical Supplies is a distinct tradable `Kilogram` commodity for routine medical
consumables. Preserve specialised medicines as separate items without automatic substitution.
Initial basic-supplies consumption is 5 kg per 1,000 people per simulation day, prorated
across updates. Remaining authored values still need selection; the economy plan records
the content implementer handoff.

Further [economy rules](economy-and-development-plan.md) select aggregate households,
pre-funded recipe payroll, progress-based wages, tax/rounding, regular payroll/purchases
every 60 simulation seconds and health/happiness losses that both reduce production.
Condition updates follow household purchases on that cadence: greater essential shortages
cause greater losses, full essentials allow gradual recovery and happiness declines/recovers
faster than health. Both remain within 0–100, initially 100 in the first balance-testing
scenario, without resets on joining/acquisition/governor changes. Initial test rates per
simulation hour are health −6/happiness −12 at complete shortage, partial losses proportional
to severity, and health +3/happiness +6 recovery at full essentials. Severity averages required
essential needs' unmet fractions using authored importance weights, initially equal. Food
counts once across qualifying grains. Surplus cannot
offset another missing essential. Condition slows production work rate while completed batches
retain declared inputs, outputs and gross wages. Slower work slows proportional wage accrual;
changes apply to subsequent steps without reversing progress or earned/paid wages. Initial
multiplier is `(0.5 + health / 200) × (0.5 + happiness / 200)`, 25%–100% of otherwise eligible
work rate; missing materials/funds or inactive facilities can still stop work entirely.
Production uses prior-step condition and settles completion. At each due economic boundary,
regular payroll/tax precedes household purchases/consumption, then condition updates for the
next fixed step. Full movement/market integration ordering and balance need validation.
The [market-information plan](market-information-plan.md) requires station-specific live
data from docked fleet members, dated remembered reports, and one-time spawn reports for
every station market in directly connected systems. Apply that knowledge boundary to
queries, auto-traders and scripts before offering/ranking options; fleet coverage supplies
the intended information advantage. These remain planning requirements without runtime proof.

The [small-ship plan](small-trader-and-docking-plan.md) adds a single-occupant long-journey
class with a truck-like tiny cabin, 4 m³ (four IBC units) of cargo and a matching small berth.
Its primary purpose remains unspecified and trade is secondary. The distinct lone-trader
starter has a 20-IBC (20 m³) hold; onboarding must not confuse the two classes.
Plan definitions in stage 2, ownership/capacity in stage 3, physical berth integration in
stage 4 and recovery in stage 5. The smaller berth is a pad class Manifest authors
([D-044](decision-ledger.md)); Jettison provisioned it in W01. Dimensions,
costs, endurance, starter upgrades and the starter's berth fit remain open.

The [contract/booking handoff](procurement-and-station-contracts-plan.md) proposes the first
supply tender and accepted-booking lifecycle. Partial delivery payments and grandfathered
booking terms are selected, as is a supplier-funded performance bond held until close with
proportional forfeiture for undelivered goods ([D-042](decision-ledger.md#d-042); its rate
is proposed, not accepted). Detailed numeric
settlement and other failure defaults remain proposals. No code or gameplay-validation
evidence is supplied by these additional plans.

Keep the five stage numbers below as foundational work packages. Before committing state
schemas in stages 2–3, account for distinct planet, region, government, company, control,
household and public-budget records, without duplicating population under bloc membership.
The proposed extended order is host/content/minimal authority, a real network/TUI slice,
idle physical trading and regional development on a shared planet, recovery, then control acquisitions,
corporate influence and frontier settlement. GUI work uses the proven shared interface.
Full frontier generation or political simulation must not block the first governed idle loop.

Stage 4 must include enabled physical auto-trader jobs and NPC operation, rather than only
a manually driven demonstration. Stage 5 must cover saved policies, development jobs and
control rights as those land. Keep the two-venue test as a small fixture; add a governance
fixture with a home for each player and AI neighbours. Original stage-plan scope statements
must be read with these amendments; none of this work has been implemented or validated.

Detailed planning handoffs for the selected implementer (implementation and validation remain pending):

- [Stage 1: public host and bootstrap](stage-1-host-plan.md).
- [Stage 2: Jettison definitions and content](stage-2-content-plan.md).
- [Stage 3: ownership and authoritative commands](stage-3-authority-plan.md).
- [Stage 4: local trading loop](stage-4-trading-plan.md).
- [Stage 5: recovery and relocatable release](stage-5-recovery-plan.md).

Apply the subsequent [engine/game boundary assessment](engine-game-boundary-assessment.md)
across these stages. Jettison must support multiple game types without a required
space-game model; Manifest owns production gameplay and content. Engine-local gameplay
fixtures and optional samples remain permitted for automated testing. The assessment
adds sample isolation, neutral consumer/build checks and module-boundary work to the
original delivery sequence.

## Stage 1: Public engine host and standalone bootstrap

Establish how Manifest starts and owns a game through Jettison.

**In progress** ([status](delivery-order.md#status-2026-11-18)). Use the
[detailed handoff](stage-1-host-plan.md) for the lifecycle contract, delivery slices
and acceptance matrix. Complete its maintenance prerequisites before implementation.

- In Jettison, expose the smallest public lifecycle/composition interface needed for
  game registration, content loading, world creation and shutdown.
- Separate sample gameplay registration from reusable host services.
- In Manifest, add a thin executable and host adapter using public SDK targets.
- Retain pinned CMake consumption and the explicit local checkout override.
- Keep the simulation host independent of rendering and terminal presentation.

Primary areas: Jettison's app lifecycle and public headers, SDK consumer check and
CMake helpers; Manifest's dependency setup and new host/executable targets.

**Exit criterion:** Manifest starts, mounts its package and shuts down cleanly through
public APIs; the Jettison sample still works; a clean checkout builds without the sibling
repository. Test startup failure cleanup as well as successful startup.

Keep this stage focused on lifecycle and mounting. Full definition integration belongs
in stage 2. No sample economy should become active implicitly.

## Stage 2: Validated content and a small scenario

Depends on stage 1. Make packaged content usable by the actual game.

- Adapt configuration and universe loading to the VFS and definition registry.
- Define schema versions, layering rules and contextual validation errors.
- Add a command-line content validator sharing the host's loading rules.
- Author the initial scenario: one system, two civilisations, two venues and eight goods.
- Preserve existing authored IDs and the full content set.
- Establish a content fingerprint for later save compatibility.

Primary areas: Manifest config/universe modules, authored scenario definitions, package
staging and content tests; reusable engine definition APIs only where a gap is demonstrated.

**Exit criterion:** directory and pack inputs produce equivalent validated definitions;
malformed references and incompatible schemas fail clearly; the scenario loads from an
unrelated working directory. This is definition loading, not yet the complete trading loop.

## Stage 3: Company ownership and authoritative commands

Depends on stages 1–2. Establish the state model before connecting the full trading loop.

- Represent companies separately from civilisations.
- Define ownership of treasuries, ships, venue inventory, cargo and orders.
- Register authoritative state in one Jettison simulation world.
- Define command identity, request IDs, execution ordering and structured results.
- Implement ownership, docking, capacity and resource validation.
- Extract command/query services from terminal presentation; retain the existing
  development harness where useful.

Primary areas: Manifest host adapter, ECS, progression and market ownership boundaries,
plus console command services and their test targets. Do not expose trusted development
methods directly as a future remote protocol.

Design save representation and retry semantics here, even though durable recovery arrives
in stage 5. Preserve the established numeric policy and authored identity references.

**Exit criterion:** commands run at fixed-step boundaries; unauthorised or invalid
operations leave state unchanged; company and civilisation funds remain distinct;
command tests run with the TUI disabled.

## Stage 4: Complete the local trading loop

Depends on stage 3. Connect existing algorithms to the authoritative model.

- Integrate production, consumption and marketplace settlement.
- Ensure automatic route trading cannot also transfer the same inventory.
- Implement buy → venue inventory → load → travel → dock → unload → sell.
- Reuse suitable Jettison movement and docking facilities.
- Add one company upgrade, preferably cargo capacity.
- Provide structured queries for balances, inventories, orders and command results.

Primary areas: Manifest simulation, market, effects, progression and host integration,
using public Jettison movement/docking APIs.

Specify economic cadence and system ordering explicitly. Physical transport must have
authoritative location and arrival state; it cannot be a presentation-only animation.

**Exit criterion:** the scenario completes through commands, with partial fills,
cancellation, expiry and rejected actions covered. Goods cannot teleport between venues,
reservations cannot be spent twice, and identical command streams produce identical
state hashes. Verify directory/pack equivalence and cross-build determinism.

## Stage 5: Persistence, recovery and a relocatable release

Depends on stage 4. Turn the working scenario into a world that survives restarts.

- Save all authoritative state, including identity counters, reservations and in-flight travel.
- Add atomic snapshots and journal recovery using Jettison facilities where suitable.
- Persist accepted command ordering before acknowledging durable success.
- Preserve retry detection across restart.
- Validate game/schema/content compatibility before resuming.
- Package a runnable distribution with separate writable user data.
- Resume at the saved simulation step without downtime catch-up.

Primary areas: host persistence/command lifecycle, engine save integration, compatibility
metadata, distribution staging and recovery tests. Apply the detailed acknowledgement,
snapshot-boundary and content-freeze contracts in the gameplay/server plan.

**Exit criterion:** interrupted saves preserve a usable recovery point; durably acknowledged
commands survive a process crash with intact storage; retries cannot duplicate execution;
recovered state matches uninterrupted execution; the distribution runs outside both
checkouts. Backups and restore tests address storage loss separately.

## Validation and delivery

For every stage, the selected implementer records acceptance evidence, updates the roadmap
and runs the relevant suites and required repository gates. Follow
[D-051](decision-ledger.md#d-051) for machine-specific concurrency; keep local
clang-tidy opt-in. Commit Jettison changes before updating Manifest's immutable SDK pin,
and keep the development environment identical to the selected SDK.

Preserve the standalone plan's CMake consumer matrix, public-API boundaries, directory/pack
checks and relocation checks. Run save/replay and recovery tests as those facilities land.
Do not mark a stage complete based only on configure success or mounted content equality.

Networking is now a required boundary to prove after minimal authority, with filtered
snapshots, reconnect behaviour, bounded command queues and an initial four-client test.
Stage 5 still gates durable production operation. Larger benchmark targets, total planet
capacity and days/months of operation remain unmeasured; client count does not determine
the number of registered players or worlds advancing while they are absent.

## Handoff

The original planning baseline was `feature-scaffold` at `f8014e5`. For the current
Stage 1 baseline, prerequisites and SDK capability map, use its
[final planning handoff](stage-1-host-plan.md#baseline-and-prerequisites) and the newest
[session handoff](session-handoff.md). No Stage 1 implementation or gates have been
performed by these planning updates. Recheck both repositories before implementation.

Start with Stage 1's maintenance prerequisites, then its delivery slices. At each
completed roadmap task, follow the session handoff guidance
in `agent-workflow.md`: recommend continuing, compacting or starting a fresh session,
and leave concrete state and validation evidence when handing off.
