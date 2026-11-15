# Stage 4: complete the local trading loop

Subsequent requirements: the [idle/planetary plan](planetary-development-plan.md) requires
physical auto-trader jobs, autopilot and NPC operation with zero clients. Preserve the
manual command fixture below, then prove saved standing policies and unattended jobs.
Regional development adds a separate public-budget project loop; manual TUI trading
is a stretch goal, and the TUI is to become a remote gameplay client.

Subsequent economy decisions in the [economy plan](economy-and-development-plan.md) select
aggregate household payroll/purchases every 60 simulation seconds, progress-based batch
wages and tracked health/happiness losses that both reduce production. Those decisions
amend the original governed-economy proposals below. The selected economic sequence is
fixed-step production using prior-step condition and completion settlement, then due regular
payroll/tax, household purchases/consumption and condition update. Updated condition applies
from the next fixed step; the full movement/market scheduler graph remains design work.
The [market-information plan](market-information-plan.md) also supersedes
global real-time trader knowledge: fleet docking supplies station-specific live data;
other estimates use dated observations, including one-time starting reports.

The [regional correction](regional-economy-and-governance-plan.md) also replaces planet-wide
household/government state with separate economies on a shared planet. The first governance
fixture needs multiple regions, independently controlled public/household accounts, local
condition/production and project effects. The selected Bronze project targets one region;
its quantities and batch recipe remain. Payroll/tax settlement resolves region explicitly,
with tax carry per employer/region. Shared stations do not pool regional funds or teleport
goods, and regional authority grants no extra live market information.

Planning handoff for Claude, prepared 2026-11-15. Depends on the public host,
[validated content](stage-2-content-plan.md) and
[authoritative command model](stage-3-authority-plan.md). This refines stage 4 of
[development stages](development-stages.md). No implementation, builds or gates were
performed. Recheck APIs against the SDK selected after stages 1–3.

Apply the [engine/game boundary assessment](engine-game-boundary-assessment.md).
The current cruise/docking APIs below are integration evidence, not an endorsement of
their existing module boundaries. Split reusable physics/AI/scene capabilities from
optional space adapters before hosted movement integration; Manifest owns travel pacing,
docking permissions and cargo policy. Preserve explicit engine test gameplay.

Inspection baseline: Manifest `feature-base` at `2716380`, Jettison `feature-base`
at `826c4bec125b6631a6ba8ee822c90463316c3511`. Only planning files were uncommitted.

## Outcome and existing integration hazards

Deliver a local command-driven scenario that buys at venue A, loads an owned ship,
physically travels to venue B, docks, unloads, sells and buys a cargo upgrade. The
scenario uses one authoritative world, the two civilisations/two venues/eight goods
from stage 2, and stage 3's ownership, inventory and command contracts.

- `Simulation::AdvanceTick` produces/consumes goods and then directly transfers stock
  and credits through `PlanTrade`. Do not call it from the hosted world. Extract/reuse
  production, consumption and pricing rules without the automatic inter-venue transfer.
- `MarketplaceService` already has Decimal price levels, reservations, maker-price
  settlement, partial fills, cancellation and expiry. Its account lookup is currently
  civilisation/global-stock based; adapt that boundary to stage 3's owner/venue records.
- `ExecuteOrder` may return fills after a later pair cannot settle. A returned error
  therefore cannot be interpreted as proof that the whole order left state unchanged.
  Hosted result semantics and transaction planning must make this explicit.
- Jettison cruise is physical authoritative state, with deterministic departure/arrival
  steps and positions. A docked ship cannot cruise, and a ship within station reach is
  refused. Destination arrival is outside the station, not docked at a pad.
- `Dock::Carry` counts integer crates and drives deck transfers; Manifest goods have
  floating quantities and different units. It is not a drop-in inventory representation.
- `Sim::ControllerKind::None` bypasses ordinary engine ownership checks, and an entity
  without `ControlledBy` accepts any sender. Manifest must never expose that bypass as
  a player-supplied command field.

## Fixed-step schedule

Keep Jettison's command-before-systems contract. Let `s` denote the clock step before
`SimWorld::Step` advances it, and `N` the scenario's positive integer economic cadence.
The selected economic cadence is 60 simulation seconds, represented by positive integer `N`
steps under the scenario's fixed timestep. Production work advances every fixed step;
regular payroll, household purchases/consumption and condition update execute once when
`(s + 1) % N == 0`. Validate cadence arithmetic and persist its configuration identity and
position. Author recipe quantities per batch and demand against elapsed simulation time,
rather than silently treating a production batch as one economic turn.
Do not use wall time, frame count or rounded elapsed seconds to trigger the economy.

The host skeleton below remains proposed, with the production/economic subsequence selected:

1. Apply admitted commands in stage 3's global sequence order. Successful order commands
   may reserve and match immediately; cargo commands observe the current docked state.
2. Expire orders with `s >= expiresStep`, refunding remaining reservations in stable order.
3. Advance production each fixed step using regional condition from the previous completed
   step, settling completed batches. On due economic turns, pay accrued regular payroll/tax,
   then household purchases/consumption (essentials before permitted optional goods), then
   update regional health/happiness. The new multiplier applies from the next fixed step.
   Goods and funds retain explicit regional/owner/venue identities and physical access.
   Pricing and native market-order placement must be integrated without reversing this sequence.
4. Advance movement/docking through the engine scheduler's required dependencies.
   Reconcile Manifest travel intents after the relevant engine systems have run.
5. Refresh authoritative quotes, then update permitted fleet observations and publish
   actor-filtered live/stale views at the completed step boundary. Newly docked observers
   receive that step's report; the last departing ship retains the previous report. Follow
   the selected [observation timing](market-information-plan.md#observation-timing-selected)
   and simulation-time age rules; do not publish unseen current market rows in an actor's
   snapshot.

Declare dependencies explicitly; do not rely on registration accident. If movement's
existing graph requires independent systems to interleave differently, preserve these
observable boundaries and document the resolved graph before implementation.
Test a batch completing exactly on an economic boundary: completion payroll settles once,
regular payroll subtracts amounts already paid, households use actual available funds/goods,
and resulting condition changes cannot rewrite that step's production or earned wages.
On intervening steps, production continues without repeating household/condition updates.
Save/replay/retry must retain cadence position and avoid duplicate economic responses.

Matching excludes an order as soon as `s >= expiresStep`, even before the expiry sweep.
Its remaining escrow becomes spendable after that sweep, so commands earlier in the
step cannot spend an automatic expiry refund. An authorised cancellation in the command
phase may retire/refund it first; the sweep then does nothing. A ship docking during
the systems phase becomes eligible for cargo commands at the next command boundary.
New production likewise becomes available to external commands on the following step.

Order placement and fill timestamps use server steps. Convert the configured economic
order lifetime to checked step counts; do not silently interpret the existing
`orderLifeTicks` number as physics steps. Lifetime zero retains its existing no-expiry
meaning. Define and test the exact expiry boundary.

## Production, demand and native liquidity

Production credits only its civilisation's local inventory. Consumption debits only
available civilisation-owned stock, never a company's purchases or sell escrow. Preserve
shortage accounting and price rules, with checked finite arithmetic. Record intentional
production/consumption sources and sinks for invariant tests.

Create a small deterministic native market policy for the two civilisations: bounded
sell orders from local surplus and bounded buy orders from demand and available funds.
Use stable venue/item ordering and the same order/reservation path as companies. Specify
replacement/cancellation cadence so each economic turn cannot accumulate unbounded
orders. Prices use the existing native price calculations and explicit Decimal rounding.
The policy is native for this milestone; Wasm policy integration remains later work.

The policy may trade at a venue but cannot move inventory between venues. Cross-venue
company transport is the only initial delivery path. `PlanTrade` may remain a privileged
development estimate; its global current-world rankings are not a hosted player or NPC
trader view. Build actor route estimates and policy options from permitted observations;
stale prices/volumes are estimates, not guaranteed arrival terms. No route estimate may
settle goods or credits in the hosted loop.
Follow the market-information plan's selected auto-trader rules: stale reports do not expire
automatically; show estimate freshness, recheck live information after docking and enforce
configured limits at execution. Skip out-of-limit trades and reconsider permitted routes.
Retain aboard cargo if no acceptable sale is found; selling below the minimum requires an
authorised fallback policy. Unknown markets can be scouting targets without fabricated
profit estimates. Standing policies continue while the player is disconnected.
Default unattended trades require positive estimated profit after purchase/transport/handling
costs, a purchase-price cap and a sale threshold recovering purchase and attributable trading
costs; players may request higher profit. Do not relax limits automatically. Blocked jobs
retain cargo, reconsider every 60 simulation seconds or on new permitted fleet observations,
and report the first blockage and holding costs. Any authorised loss allowance applies to
the entire cargo across partial sales, with zero allowance by default. Resolve cost allocation,
rounding and shared allowance accounting before implementation; holding costs can still
reduce profit. See the market-information plan for the selected policy and required checks.
If native demand exhausts its finite treasury, it stops buying. Do not add a hidden mint
to keep the scenario profitable; any future subsidy must be explicit authored policy.

## Order and settlement contract

Initially expose bounded limit orders and cancellation. Add immediate-or-cancel only if
the acceptance scenario needs it, with explicit limit and remainder semantics. Do not
publish an unbounded market-buy operation based on a Decimal maximum sentinel.

Reserve buy funds and sell goods against the correct account and venue. All hosted
venues enforce stock and funds; the development `requireStock=false`/`requireFunds=false`
options are not player-accessible. Validate item selection, owner, quantity, price,
venue and resource bounds before creating an order or spending an ID.

Match best price first and FIFO within a price using server placement sequence; store
that sequence in authoritative state. The existing development tie-break by tick and
string order ID remains a development behaviour, not a reason to let `Order-10` jump
ahead of `Order-2` in the hosted book. The maker sets the fill price.

Preserve [market price rules](market-prices.md): exact Decimal comparisons, double
quantity/product conversion, ceil buy reservations, nearest whole-credit fill gross
and ceil fees. Do not introduce microcredit wallets, cumulative rounding, a silent fee
cap or changed authored prices as part of ownership integration.

Preflight settlement using a transaction plan that aggregates all changes by account,
venue/item and order before writing. Include buyer goods, seller proceeds, both fees,
venue income, escrow, final refunds, counters and records. Aliased accounts, including
self-trades or a venue owner also trading there, must not bypass overflow checks.
Keep venue income as one credited balance; a `venueTake` statistic must not mint a
second copy of fee income.

For a command-triggered matching pass, plan its complete bounded set of fills and
remaining order before committing. Expected validation or arithmetic failure rejects
the command without business-state changes. Ordinary lack of liquidity is a successful
partial fill with an active remainder, not a rejected action. Report filled quantity,
remaining quantity and resulting order identity separately. A deterministic work bound
must be a documented limit/continuation rule, not a wall-clock timeout.

Periodic native matching and expiry use the same settlement/refund rules. Preflight
combined refunds before a batch writes, preserving the existing batch atomicity work.
Cancellation checks actor ownership; request deduplication prevents duplicate refunds.
Closed-order results needed for retries belong to the request/result state, not the
64-entry presentation fill log. Keep that display history bounded.

Freeze venue fee policy initially. If a later upgrade changes fees, define how existing
orders remain fully funded before enabling it. Do not change fees underneath reserved
orders and assume their original escrow still guarantees settlement.

## Cargo capacity and physical travel

The subsequent [small-ship plan](small-trader-and-docking-plan.md) requires a compact
single-occupant long-journey hull with four IBC units (4 m³) of usable cargo space and a
matching small berth. Its primary role remains unspecified, with trade secondary. The
separate lone-trader starter has a 20-IBC (20 m³) hold. Preserve distinct capacities in
onboarding and load validation, and the compact class's limit across effects.
The pinned SDK's fixed smallest pad does not provide the required smaller berth; provision
the reusable engine capability before consuming it here. Dimensions, endurance, starter
upgrades and each hull's berth fit remain open; neither class is implemented by this amendment.

Selected starter life-support storage is separate from its 20 m³ trade hold: operating
oxygen and the installed cartridge occupy dedicated service space. Trade supplies and
loose spares use normal cargo capacity. Track actual stock locations; transfer into
service storage debits cargo/source stock once and must obey authored destination limits.
The starter's operating oxygen maximum is 30 kg; it begins full. Cargo oxygen remains
separate until transferred and cannot increase this limit. Preserve stock, ownership
and remaining service capacity through retry/recovery. Other hull limits, physical tank
volume, compartment dimensions, item fields and transfer/replacement procedures
remain open; separate storage does not grant unlimited reserves or duplicate inventory.

Implement one shared quantity-to-volume conversion using the distinctions in
[model units](model.md). Proposed rules derived from that contract:

| Unit | Occupied cubic metres for quantity `q` |
| --- | --- |
| Kilogram, Gram | `q / (unitsPerCubicMetre * packFactor)`; the authored count is solid material. |
| Litre, Kilolitre | `q / unitsPerCubicMetre`; authored bulk volume already includes the heap's voids. |
| Unit, Sheet, SquareMetre | `q / unitsPerCubicMetre`; the authored count is already packed. |

Do not apply `packFactor` twice to volume/piece goods. Verify copper, a bulk mineral
and glassware against the documented examples, then cover every supported unit. Sum
occupied volume in stable item order and reject invalid denominators/nonfinite totals.
The initial capacity constraint is `CargoVolume`; do not invent an unauthored payload
mass limit. If dynamic cargo mass is integrated with physics, derive and test it
separately without changing the volume contract.

Keep stage 3 load/unload as atomic transfers at a confirmed docked venue. This milestone
does not simulate deck loading time. Do not convert fractional goods to integer crates
or make `Dock::Carry` a second inventory owner. Deck transfer animation can follow with
an explicit conversion and pending-transfer accounting contract.

A travel command creates an owned, saved intent, with states such as departing,
cruising, approaching, waiting-for-pad, docked or failed. The host resolves stable IDs
to engine entities and derives the engine controller from authoritative ownership.
Every company ship receives `ControlledBy`; raw engine movement commands are internal.

Coordinate `ReleasePad`, departure clearance, `Cruise` and `RequestPad` through existing
public engine APIs. Cruise attachment currently requires physics and lane services;
docking also requires steering. Register the actual dependencies without activating
sample traders. Define the scenario star system, hulls, station layouts and physics
shapes through public APIs; use a consistent venue-to-station/body mapping.

Reconcile transitions using saved engine state and structured outcomes. Transient
presentation events must not be the only record of an arrival or failure. Internal
derived engine commands must not be recorded as duplicate external intentions during
replay. Persist enough travel intent to avoid repeating a departure or pad request
after restore. Correlate a travel operation separately from the final result of the
command that started it: a queued pad request is not an arrival.

Reject a second travel intent while one is active. Handle no compatible pad, an occupied
pad, cruise refusal and departure blockage explicitly. A queued ship retains its cargo
and cannot unload. Successful destination docking is the only point at which destination
inventory transfers become possible. Never solve a refusal by setting a ship's location
or docking flag directly. Bound scenario tests by simulation steps so a stalled journey
fails with its current state instead of hanging.

## First company purchase

Use `SkillShipCargoHolds`/`CargoHolds` as the initial upgrade, targeting an owned ship.
The existing effect adds 300 cubic metres per tier; reuse its stat recomputation rather
than incrementing a cached capacity repeatedly. Company-owned progression records the
purchase, scoped to its target ship, separately from civilisation technology.

Add an explicit versioned company-offer definition with skill ID, whole-credit cost,
target scope and applicable tier limits. Load it using the stage 2 Jettison pipeline
and include it in the content fingerprint. Do not reinterpret the existing
`resourceCostPerTier: 2` as two credits or debit the civilisation's resource pool.
Choose the scenario offer cost so a demonstrated completed trade can fund it; record
the chosen value and measured trade outcome as fixture evidence.

Validate ownership, docking if fitting requires it, slots, prerequisites, maximum tier
and available company credits before committing payment and progression together.
Retrying the request must neither charge twice nor add capacity twice. Keep any broader
technology propagation unchanged until its hosted ownership rule is specified.

## Acceptance scenario and delivery

Implement in reviewable slices: owned account settlement; local production/demand and
cadence; physical travel integration; company purchase; complete command-driven scenario.
Each slice preserves existing relevant checks before the next expands the integration.

The final test must use the public command/query boundary, not direct component edits
after bootstrap:

1. Load the validated scenario and provision a company with one docked ship and a
   bounded treasury. Native production/policy supplies genuine local orders.
2. Buy at A. Verify goods enter company inventory at A, while ship cargo is unchanged.
3. Load some goods. Verify capacity and source balance, then request travel to B.
4. Advance fixed steps through real engine departure/cruise/docking. Reject load/unload
   during transit and at the wrong venue. Prove cargo stayed with the ship.
5. Unload at B, sell against funded demand and verify seller proceeds, buyer inventory,
   fees and cleared reservations. No step may teleport stock between venue balances.
6. Purchase the cargo upgrade with company funds. Verify its actual capacity increase
   and that a duplicate request leaves balance and capacity unchanged.

Also cover partial fills, cancellation, exact expiry boundary, same-price FIFO, multiple
commands competing for funds, another company's order/ship, overflows, invalid quantities,
full pads and refused cruise. For rejected actions compare business state; clocks and
recorded command outcomes may legitimately change as described in stage 3.

Add the market-information plan's observation checks: A live/B stale at bootstrap, every
market in directly connected systems seeded once, frozen remote quotes/sales after changes,
refresh on docking and freeze after the last observing ship leaves, different player views,
and no hidden-current-state influence on trader/script options. Reconnect cannot reseed or
refresh starting reports. Retain the small manual fixture and use separate cases for
multi-station/multi-fleet coverage and remembered-state recovery.

Track total goods across available venue inventory, cargo and escrow, adjusted only by
explicit production/consumption. Track credits across available balances, buy escrow and
fee recipients; the upgrade is an explicit credit sink unless its offer names a recipient.
Do not assert conservation across intentional sources/sinks without accounting for them.

Run the identical command stream from directory and pack content. Compare canonical
initial content, per-step outcomes and authoritative hashes. Repeat across Debug,
Release and sanitised presets. Exercise in-memory save/load continuation during an
open order and a journey; durable crash recovery remains stage 5.

The implementer runs relevant market/effects/host/command/engine movement suites and
required repository gates with two compile/test jobs and no local clang-tidy. Document
remaining graphics configuration dependencies honestly. A successful automated local
loop establishes gameplay integration; it does not establish multiplayer, recovery,
graphical presentation or public hosting readiness.

## Handoff to stage 5

Carry forward the exact scheduler order, command/result schema, content manifest,
owner/venue ledger layout, allocation counters, escrow representation and travel states.
Stage 5 adds durable acknowledgement, journal/snapshot recovery, backup/restore and
distribution relocation. No stage 4 success response should claim durability yet.
