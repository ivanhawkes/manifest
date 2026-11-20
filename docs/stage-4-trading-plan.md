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
from the next fixed step; W02 resolved the full movement/market graph ([the step](#the-step-in-order)).
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

Resolved in W02 (2026-11-20, [D-086](decision-ledger.md#d-086)). The hosted world steps at
1/60 simulation second, the step Jettison's docking and physics are tested at; `Start.cpp`'s
one-second shortcut goes with W05. Let `s` denote the clock step before `SimWorld::Step` advances
it. The economic cadence `N` is `economy.intervalSeconds` divided by the fixed step, 3,600 steps,
checked at start-up to be a positive whole number. Production work advances every fixed step;
regular payroll, household purchases/consumption, condition update and the brokers' requote run
once when `(s + 1) % N == 0`. The position is the saved clock's step modulo `N`, so no second
counter is saved; the content fingerprint carries `balance.yaml`, so a save under another cadence
is refused as other content. Author recipe quantities per batch and demand against elapsed
simulation time, rather than silently treating a production batch as one economic turn. Do not
use wall time, frame count or rounded elapsed seconds to trigger the economy.

### The step, in order

`SimWorld::Step` applies the queued commands, then runs the simulation phase's systems in the
scheduler's order. Every edge below is declared, `after` or `before`; none is left to
registration order, and a test pins `Scheduler().Order(TickPhase::Simulation)`.

| # | Runs | System | Declared order | Writes |
| --- | --- | --- | --- | --- |
| 0 | each step | the command phase (`Requests::Execute`) | before every system | per command, [below](#transaction-boundaries) |
| 1 | each step | `manifest.expiry` | before `manifest.economy` | expired orders, their escrow |
| 2 | each step; the turn on due steps | `manifest.economy` (W04) | before `spatial` | production, payroll, households, condition |
| 3 | due steps | `manifest.brokers` | after `manifest.economy`, before `spatial` | each broker's orders, fills |
| 4 | each step | engine `spatial`, `steering`, `flight`, `physics` | Jettison's own edges | transforms, velocities |
| 5 | each step | engine `dock` | after `physics`; before `lanes` once W05 provisions the edge | pads, docking, transfers |
| 6 | each step | engine `lanes`, then `cruise` | Jettison's own edges | lane and cruise state |
| 7 | each step | `manifest.travel` (W05) | after `dock` and `cruise` | travel intents, `Ship::dockedAt` |
| 8 | each step | `manifest.observe` (W07) | after `manifest.travel` | quotes, fleet observations |

Jettison's movement systems are registered without its `behaviour` system (no behaviour trees
until W10), without `gates` (the fixture is one system) and without the sample's traders. `dock`
and `lanes` both write `Transform` and `InFrame`, and Jettison orders neither before the other, so
registration order decides theirs today. Manifest cannot add an edge to a system Jettison
registers: W05 provisions it in Jettison (an ordering between the two, with an independent
consumer's test), and the hosted registration's `Scheduler::Ambiguities` then holds no pair that
writes what the other touches. This is the one engine gap W02 names.

- **Commands** apply in stage 3's global sequence order. An order command reserves and matches at
  once; a cargo command sees `Ship::dockedAt` as the previous step's `manifest.travel` left it; a
  travel command creates its intent and writes no engine state.
- **Expiry** retires every order with `s >= expiresStep` and refunds its remaining reservation,
  in placement order.
- **Economy** (W04) advances production each fixed step using regional condition from the
  previous completed step, settling completed batches. On due turns it pays accrued regular
  payroll/tax, then household purchases/consumption (essentials before permitted optional
  goods), then updates regional health/happiness; the new multiplier applies from the next fixed
  step. Goods and funds keep their regional/owner/venue identities and physical access.
- **Brokers** requote after the turn's economy, so their quotes see its stock and funds
  ([D-084](decision-ledger.md#d-084)).
- **Travel** reads the saved engine state the movement systems left (docking, cruising, a pad's
  occupant), advances each intent one transition, and sends the next engine command through
  `SimWorld::Apply`, which the engine's systems act on from the next step. It alone writes
  `Ship::dockedAt`, from an observed docking. Its engine commands are the state's consequence and
  are not recorded, so a replay sends them again rather than twice.
- **Observe** (W07) refreshes authoritative quotes, then updates permitted fleet observations and
  publishes actor-filtered live/stale views at the completed step boundary. Newly docked observers
  receive that step's report; the last departing ship retains the previous report. Follow the
  selected [observation timing](market-information-plan.md#observation-timing-selected) and
  simulation-time age rules; do not publish unseen current market rows in an actor's snapshot.

W03 and W15 register 0, 1, 3 and, with W05, 4 to 7; W04 adds 2 and W07 adds 8, each into the
edges above. A task that needs another place in the step changes this table first.

### Transaction boundaries

Jettison leaves a world part-way through a step when a command or system throws, and only a load
recovers it. Every expected failure is therefore a result decided before the first business write,
and a throw is an invariant breach.

| Unit | Plans before writing | On an expected failure |
| --- | --- | --- |
| One admitted command | its reservation, its whole matching pass (at most 64 fills), both sides' fees, venue income, refunds and counters, aggregated by account, venue/item and order, aliases included | refused with no business write; the clock and the ledger's outcome still change |
| The expiry sweep, one a step | every refund the step's expiries make, combined | none expected: the refunds are already escrowed |
| One broker's requote, a venue a turn | cancelling its resting orders, their refunds, its new orders and their matching pass | the broker posts what it can fund; a side it cannot fund posts nothing |
| One travel transition, a ship a step | the intent's next state and the engine command that leads to it | the intent records the refusal (no pad, cruise refused, departure blocked) and waits or fails, the ship's cargo kept |
| Load, unload, provisioning, the upgrade (W08) | as Stage 3 and W08 define each | refused with no business write |

A command's matching pass settles at most 64 fills. A side of one venue's book for one item holds
at most 64 resting orders, and an order past that is refused as `BookFull`, so a pass can always
settle every crossing maker and no remainder rests crossed. Ordinary lack of liquidity is a
successful partial fill with an active remainder.

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

Order placement and fill timestamps use server steps. A resting order lives one simulation day by
default ([D-085](decision-ledger.md#d-085)), authored in simulation seconds and converted to steps
with checked arithmetic; the existing `orderLifeTicks` is not read as steps. Zero keeps its
no-expiry meaning, and a consignment listing has none. Define and test the exact expiry boundary.

## Production, demand and native liquidity

Production credits only its civilisation's local inventory. Consumption debits only
available civilisation-owned stock, never a company's purchases or sell escrow. Preserve
shortage accounting and price rules, with checked finite arithmetic. Record intentional
production/consumption sources and sinks for invariant tests.

The native counterparty is a broker at each venue ([D-084](decision-ledger.md#d-084), W02): a
trading principal owned by the venue's operator, with its own account and holdings in totes
there, seeded from the scenario's venue stock and an authored treasury. On each due economic turn
it cancels its resting orders and posts one bid and one ask per item it trades, around the
existing native price with an authored spread and explicit Decimal rounding: the ask no more
than its unreserved holdings, the bid no more than its unreserved funds cover with fees. Its
orders take the same order/reservation path as a company's, in stable venue/item order, so a turn
cannot accumulate orders. The treasury and spread are fixture test values W03 authors in
`balance.yaml`, as C05 authored the fixtures'; until W04, a broker's stock changes only by trade.
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
single-occupant long-journey hull with 32 totes (4 m³) of usable cargo space and a
matching small berth. Its primary role remains unspecified, with trade secondary. The
separate lone-trader starter has a 160-tote (20 m³) hold. Preserve distinct capacities in
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

Reconciled with [D-078](decision-ledger.md#d-078) in W02 ([D-083](decision-ledger.md#d-083)) and
counted in totes since W03 ([D-087](decision-ledger.md#d-087)): goods are held, moved and traded in
whole totes, an eighth of a cubic metre each, one tote being an eighth of the item's
`unitsPerCubicMetre` in its unit, so the occupied volume of `n` totes is `n / 8` cubic metres for
every unit and no `packFactor` enters capacity. An order's quantity is a whole number of totes and
its limit stays a Decimal price per authored unit. A fill's gross is that price times
`unitsPerCubicMetre / 8` times the totes, converted to whole credits by the
[market price rules](market-prices.md) (nearest whole-credit gross, ceil fees, ceil buy
reservations); the double unit count is formed once, at that conversion. A broker's holdings are
totes like a company's.
The initial capacity constraint is `CargoVolume`; do not invent an unauthored payload
mass limit. If dynamic cargo mass is integrated with physics, derive and test it
separately without changing the volume contract.

Keep stage 3 load/unload as atomic transfers at a confirmed docked venue. This milestone
does not simulate deck loading time. Do not make `Dock::Carry`, which counts crates, a second
inventory owner. Deck transfer animation can follow with
an explicit conversion and pending-transfer accounting contract.

Selected in W02 ([D-086](decision-ledger.md#d-086)): one player command, `Travel(ship, venue)`,
starts a journey, and `manifest.travel` drives it through pad release, departure, cruise, pad
request and docking ([the step](#the-step-in-order)); the player sends no engine command. The
first slice charges nothing for travel, docking or handling: a fill's listing fee is the only
charge until a later task selects the economy plan's handling services. The starter docks at a
`skiff` pad ([D-058](decision-ledger.md#d-058)).

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
The existing effect adds 2,400 totes (300 cubic metres) per tier; reuse its stat recomputation rather
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

W03 restated the trading fixture's stocks in totes ([D-087](decision-ledger.md#d-087)). A
25,000-credit start ([D-077](decision-ledger.md#d-077)) buys totes of furs at Ore and of salt,
lime, glassware and soap at Brine, and now a tote of copper (about 14,800 credits) or of tin
(about 42,400 at base price, beyond the start); a tote of gold is about 290 million, a later good.
W15 trades furs Ore to Brine or a Brine good back.

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
required repository gates with each machine's concurrency (D-051), local clang-tidy opt-in. Document
remaining graphics configuration dependencies honestly. A successful automated local
loop establishes gameplay integration; it does not establish multiplayer, recovery,
graphical presentation or public hosting readiness.

## Delivered: W03, 2026-11-20

The `trade` module settles the hosted book on owner/venue accounts; Stage 3's goods are counted in
totes ([D-087](decision-ledger.md#d-087), the user's choice during W03), and content authors a
hull's `CargoVolume` and a venue's `stock` in totes as well. What it chose where the contract above
left room:

- **State.** `World::Order` (`Order<n>`, n its time priority) on its own entity names its owner, a
  `Company` or a venue's `Broker`, its venue, item, side, Decimal limit as raw microcredits, totes
  and remaining, a buy's escrow, the fee it was placed at, its `expiresStep` and the sequence of the
  request that placed it. A sell's remaining totes are its owner's `Holding::reserved`, which a
  load checks against the sell orders both ways. Each venue has a `Broker` (by the venue's ID) with
  a `Stock` seeded from its market `stock`, a `BrokerAccount` with `brokers.treasury`, and a
  `VenueAccount` for the fees, the one credited balance of venue income.
- **Commands.** `Manifest.Trade.Place` and `Manifest.Trade.Cancel`, a player's, refusing with the
  Stage 3 results plus `InvalidPrice` and `BookFull`. What filled and what rests are read from the
  order its request placed (`Order::request`); a whole fill rests nothing. A company may trade at any
  venue, with no ship there.
- **Escrow.** A buy reserves, for each tote, a tote's cost at its limit rounded up and the fee on
  that. A fill of n totes at a price at or under the limit costs at most n of these (its gross
  rounded to the nearest, its fee rounded up), so the escrow never runs short, however the book
  splits the order; what is left goes back when the order leaves. This is the market price rules'
  ceil reservation taken a tote at a time.
- **Room.** A buy is refused unless its owner's warehouse there holds it with every resting buy of
  theirs filled, and an unload counts those buys too, so no fill can pass `kMaxWarehouseTotes`.
- **Planning.** A command or system works on copies of the accounts, stocks and orders it touches,
  one copy for an account reached twice, and writes them only when every check has held; an order's
  ID is issued after. The expiry sweep and a broker's cancellations retire in placement order, and a
  refund they cannot pay throws, as an invariant breach.
- **Brokers.** The native price is the development model's (`Simulation::MarketPrice` and
  `LocalPrice`, extracted for this) over the scenario's venues as its economies. On the last step of
  each turn, `(s + 1) % N == 0`, and once in the initial state, each broker in scenario order
  cancels its orders and, good by good in its market's order, asks for all it holds at the price
  plus half the spread (rounded up) and bids at the price less half (rounded down) for what an equal
  share of its funds among the goods still to quote buys, within its warehouse's room. Its orders
  live a day like any, and a side it cannot fund or rest posts nothing. Fixture values:
  1,000,000 credits and a 1,000-basis-point spread.
- **Step.** `manifest.expiry` then `manifest.brokers` (after it); the brokers' edge after
  `manifest.economy` comes with W04, and before `spatial` with W05, since the pinned SDK's scheduler
  has no edge to an unregistered system.

Evidence, in the `trade` suite: a buy filling a resting sell at the maker's price with both fees to
the venue and the rest resting, then cancelled to the escrow left; reserved totes refused a load and
given back on a cancel; a better price, then the older order at one price, filling first; an order
matching on the step before its `expiresStep`, not on that step, and that step's sweep returning it;
every refusal leaving accounts, stocks and orders as they were; two buys in one step competing for one
account; 64 resting orders and `BookFull` past them, a whole fill still accepted; a 64-fill pass with
its remainder resting; credits (accounts and escrow) and totes conserved by a trade; the brokers'
first quotes and their requote at step 59 replacing every order, none piling up; the step's order; an
order life not a whole number of steps refused at start; and a save with an open order continuing as
its world, a holding reserving more than its sell orders, or an order of an unknown owner, refused.

The governance fixture's stocks are restated in totes too. Left: market views in the snapshot, and
the console's and TUI's trade commands (W07, W15); immediate-or-cancel, which the acceptance
scenario has not needed; and a broker's demand beyond its funds, a W04 question.

## Handoff to stage 5

Carry forward the exact scheduler order, command/result schema, content manifest,
owner/venue ledger layout, allocation counters, escrow representation and travel states.
Stage 5 adds durable acknowledgement, journal/snapshot recovery, backup/restore and
distribution relocation. No stage 4 success response should claim durability yet.
