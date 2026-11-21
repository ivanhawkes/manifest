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
1/60 simulation second, the step Jettison's docking and physics are tested at, since W05. Let `s` denote the clock step before `SimWorld::Step` advances
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
| 5 | each step | engine `dock` | after `physics`; before `lanes` (Jettison `bc5dc1a`) | pads, docking, transfers |
| 6 | each step | engine `lanes`, then `cruise` | Jettison's own edges | lane and cruise state |
| 7 | each step | `manifest.travel` (W05) | after `dock` and `cruise` | travel intents, `Ship::dockedAt` |
| 7a | each step | `manifest.lifeSupport` (W06) | after `manifest.travel` | connections, onboard oxygen and cartridges, a job's draw |
| 7b | each day's last step | `manifest.pricing` (W18) | after `manifest.lifeSupport` | an AI-run venue's `Pricing` level and `Fees` |
| 7c | each hour's last step | `manifest.berths` (W10, W18) | after `manifest.pricing` | berth fees, arrears, a job's charges |
| 7d | each day's last step | `manifest.seizure` (W18) | after `manifest.berths`, before `manifest.observe` | a held ship's cargo sold through its owner's commands, arrears, a job's lot |
| 8 | each step | `manifest.observe` (W07) | after `manifest.travel` | players' market reports |

Jettison's movement systems are registered without its `behaviour` system (no behaviour trees
until W10), without `gates` (the fixture is one system) and without the sample's traders. Jettison
orders `lanes` after `dock` when both are attached (`bc5dc1a`, the one engine gap W02 named), and
the hosted registration's `Scheduler::Ambiguities` holds no pair that writes what the other touches;
the `travel` suite pins the order and the empty ambiguities.

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

W03 and W15 register 0, 1, 3 and, with W05, 4 to 7; W04 adds 2, W06 7a and W07 adds 8, each into
the edges above. A task that needs another place in the step changes this table first.

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
| Load, unload, refill, replace, provisioning, the upgrade (W08) | as Stage 3, W06 and W08 define each | refused with no business write |

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

## Delivered: W05, 2026-11-20

A new `travel` module, below `company`, puts the hosted world's ships in space. What it chose where
the contract above left room:

- **Content** ([D-088](decision-ledger.md#d-088)). Each scenario system is a `Celestial.StarSystem`
  record named by its ID (`data/systems/`): a Sun-like star, the planets on their orbits and each
  venue a `Station` body of its ID orbiting its planet 42,000 km out, a highport's radius 763 m; the
  venue names its layout, the body none. `position` left the scenario. Each hull has an
  `Ai.CruiseDrive` of its record name (`data/docks/drives/`). Content refuses a system with no record
  or one that does not resolve, a planet or venue its system has no body of, a station not orbiting
  its venue's planet or naming a layout, and a hull without a drive and the reverse. The content set
  carries the pad classes, dock hulls, drives, layouts and the scenario's resolved systems, so a world
  is composed from it alone.
- **Composition.** `Travel::Register` attaches physics, the spatial index, steering, the docks, the
  lanes and cruising, defines the content's docks and systems in them, and registers
  `Manifest.Travel.Travel` and `manifest.travel`; `OpenStations` builds each venue's station on its
  body's rails in the initial state. The host's step is 1/60 s, and the market's systems run before
  `spatial`.
- **Ships.** Provisioning parks the starter on the smallest free pad its dock hull fits, the lowest
  numbered of those (a skiff pad), as a dynamic body of its hull with its drive, refusing `NoPadFree`
  before anything is written. Its engine controller is a player's, slot n of its `Company<n>`
  (`ControllerOf`), never reused. `travel` sends every engine command as that controller. It weighs
  its hull's authored mass (W08).
- **The journey.** `World::Journey` on the ship (saved): the venue, the leg (`Departing`,
  `Clearing`, `Cruising`, `Arriving`, then `Arrived` or `Failed`), why it failed, and the step it
  started. `Travel` refuses `UnknownTarget`, `NotAuthorised`, `Travelling` while one is under way
  and `AlreadyThere` at the ship's own dock, and writes only the journey. Each step `manifest.travel`
  takes each journey in entity order one transition on from the engine's saved state: `ReleasePad`,
  clearing `Ship::dockedAt` once the pad lets go (`DepartureRefused` if not); the flight out of the
  station's reach (Jettison's `Space::ClearOf`); `Cruise` (`CruiseRefused`); on arrival
  `RequestPad` (`PadRefused`), a queue waited out in place; and `Ship::dockedAt` set as the docks
  clamp it (`PadLost` if the pad or place goes). A ship already near its destination asks for a pad
  at once. Nothing it sends is recorded, so a replay sends it again from the same state.
- **Engine provisions**, both in Jettison with their own suites' evidence: `Space::ClearOf`, the
  flight out of a station's reach that the sample's traders flew in their own code; and a cruise or
  a lane letting a steered ship's controls go with its destination, without which a ship queued on
  arrival flew on at steering's last demand.

Evidence, in the `travel` suite, on the hosted trading scenario: the starter parked docked on pad 13,
a skiff pad, commanded by `Company1`'s controller; Ore to Brine through every leg in about 28,000
steps, its cargo kept, a load refused `NotDocked` in transit and `WrongVenue` after, the unload
accepted at Brine and a second journey there refused `AlreadyThere`; each refusal writing no
journey; a departure refused while crates cross the deck, the ship kept docked; at a station with all
twenty pads held, a ship waiting still in its queue, unable to unload, then docking when one frees;
a world saved mid-cruise arriving at the same step with the same hash as the one saved; the compact
single-seat given a skiff pad and the long-haul trader a destroyer pad; the same journey from the pack
and the directory, hash for hash; and the step's order with no ambiguities. The `company` suite's
pinned hash moved, ships now bodies on pads.

Left: the journey in a player's snapshot and the console's and TUI's travel command (W15, whose
console steps at most 1,000 at a time, under a minute); a journey across systems, which waits for
gates; the handling services' charges; and long-haul timing, with W15's two-planet trade.

## Delivered: W17, 2026-11-20

The Manifest mode's cruise is limited by acceleration ([D-090](decision-ledger.md#d-090)), on
Jettison's `Space::CruiseProfile` (pin `54fb871`), which a drive may give in place of its power law:

- **Content.** A game mode's optional `cruise` (`Manifest.CruiseModel`) gives a `manned` and an
  `unmanned` profile, each checked as Jettison checks one. The Manifest mode's are 2 g for at most
  4 hours, 2 at each end, and 1 g between, and 12 g throughout; Astralis gives none, so its drives
  keep D-088's power law. As the content set is built, each hull's drive takes the mode's profile:
  unmanned for a hull whose `occupants` is 0, which content now accepts, and manned otherwise, the
  long-haul trader's open occupants included (the user's choice). Travel is unchanged: it gives a ship
  its hull's drive from the set.
- **The cruise.** Jettison times it by the profile's closed form and places the ship along its course
  as the profile covers it, in whole steps no step of which passes the boost.

Evidence: in the `content` suite, every hull's drive in the Manifest mode at the manned profile, a
crewless hull at 12 g, Astralis's drives as authored with no profile, a trip between two highports
across one planet's 42,000 km orbit at 2 g throughout, and a bad profile refused naming the mode; in
the `travel` suite, the starter from Ore to Brine, 396 million km, in 4.49 days, the least whole steps
its profile covers, at 2 g ten seconds out, and along its course 2 g to two hours, 1 g after and 2 g
slowing in the last two; and a world saved mid-cruise on the profile flying on step for step with the
one saved. The W05 journeys run on the power law, the fixture clearing the ship's profile, so each
takes minutes; the `company` suite's pinned hash moved for the profile on its ship's saved drive, and
holds with the mode's cruise taken out.

Left: a local trip in play waits for venues round one planet; long-haul play in the mode needs
stargates or moongates (L16), and the TUI and console show a journey's days (W15).

## Delivered: W16, 2026-11-20

Onboarding by [D-089](decision-ledger.md#d-089), with no engine edit:

- **Content.** A scenario names its `starterVenue`, one of its venues, beside its `starterHull`, both
  now required: the venue must be public and operational, have life support, and have a pad, not for
  display, that the starter's dock hull fits (D-047). Its operator is one of the scenario's
  organisations, none a player's, so it is neutral. `playerHome` and `ContentSet::homeVenues` are
  gone, and any region may be ungoverned (`governorId` empty), as the fixtures' former homes are,
  left for founding by acquisition later.
- **Provisioning.** `Manifest.Company.Provision` makes the principal, the company and its credits,
  and parks the starter on a free pad of the starter venue; it makes no polity and governs no region.
  `NoHomeRegion` is retired: every player starts at the one venue until its pads that fit the
  starter are taken, then `NoPadFree`. A saved `Region` no longer holds `playerHome`.

Evidence: in the `content` suite, the starter venue refused naming no venue, and for each lack (not
public, not operational, no life support, no pad the starter fits), an empty `starterHull` refused,
and an ungoverned region accepted and a governor naming nothing refused; in the `company` suite, a
player provisioned at Ore Highport with no organisation issued and the region ungoverned, players
provisioned there until `NoPadFree`, which spends no ID, and three players in one step at the
governance highport. The `company` suite's pinned hash moved for the save holding no polity and no
`playerHome`.

Left: founding a homeworld and its polity, under the acquisition rules, is its own later task; the
planning documents' home-region allocation (scope, roadmap's region supply, D-076, and the regional
plan's onboarding) is W13's to restate as founding.

## Delivered: W15, 2026-11-20

The vertical slice, played through the console on the trading fixture, on pin `54fb871` with no
engine edit:

- **Console.** `trade buy|sell [principal] <venue> <item> <totes> <price>` and `cancel [principal]
  <order>` send W03's `Place` and `Cancel`, and `travel [principal] <ship> <venue>` W05's `Travel`,
  each through admission as `cargo` does, or to the server when connected, as the token's actor. The
  price is exact decimal text per the item's unit. `book <venue> [item]` lists a venue's resting
  orders, asks then bids, best first, as the host sees them: a development view, refused when
  connected, until W07 gives a player market views (the user's choice, 2026-11-20).
- **Stepping** (the user's choice, 2026-11-20). `step` takes a count or a time (`90s`, `30m`, `6h`,
  `4d`), at most 60 days, and stops at the step after which a journey under way has ended, naming
  the ship, how it ended and its days. On the manned profile Ore to Brine is 23,298,304 steps, 4.49
  days, which a release build steps in 9 minutes 35 seconds (about 25 µs a step); a server steps it
  in real time, the idle the mode suits (D-090).
- **Snapshot.** A ship's view carries its latest journey, and a company's its resting orders, oldest
  first, `Order<n>` with the order as the world holds it: the player's own, which a cancel needs. The
  protocol is version 2 for the snapshot's shape. The console and the TUI show a journey's leg, venue
  and days out, a failed one's reason, and each order's side, venue, totes left and limit.

Evidence, in the `console` suite, on the trading fixture with the mode's cruise taken out so the
cruise is minutes: Ada buys 2 totes of furs at Ore's ask (25,000 to 24,691 credits), loads them,
travels, is seen cruising with them aboard and refused an unload in transit, steps `1d` and stops
docked at Brine, unloads and sells at Brine's bid (29,590 credits, no furs left), then a bid under
the market rests in her view and in the book until her cancel; every bad step, price and venue is an
error that steps nothing; and connected, a trade sent without a principal rests in the server's
snapshot of her. Played on the manned profile in a release build, the same commands reach the same
credits after 4.49 days. The `company`, `query` and `remote` suites hold.

Left: market views for a player (W07); the long step, which blocks the console while it runs and
costs minutes of a release build ([L26](delivery-order.md#later-priorities-and-re-entry-conditions)); and the slice's whole fixture, with production, life support, observations and the upgrade
(W09).

## Delivered: W04, 2026-11-20

The regional economy, `manifest.economy`, on pin `54fb871` with no engine edit. What it chose where
the plans left room ([D-092](decision-ledger.md#d-092), the user's choices):

- **Content.** A market row names its `regionId`, one of the venue's regions: that region's employer
  makes its `production`, which needs one, and its households want its `demand` as an optional good
  when no need names the good; a need's good's demand only prices it. A region has `employerFunds`,
  and `balance.yaml` a `production` section: batches of about a day (`batchSeconds`, 86,400) whose
  gross wage bill is `wageShare`, 0.3, of the output's value at base price. Every venue serving a
  region trades every need's goods, at the least (the user's rule): the trading fixture gains the six
  essentials, Ore growing rice and oats and Brine wheat and barley and making the medical supplies,
  each drawing its own water; its consumer goods (Ore's salt, glassware and soap, Brine's furs) are
  its households' optional goods. The governance fixture's Cassiter dock gains the three grains it
  lacked, and each Forge row a region's employer. `wageTaxRate` must be one credit in a whole number,
  so a carry withholds exactly.
- **State.** Each region has an `Employer` with its `EmployerAccount` and its `Production` (a batch
  under way for each row it makes, and the withholding's carry), and a `Household` with a `Stock` and
  a `Pantry` (what is left of opened totes, each need's unmet total, the last severity and the short
  intervals); the households' money stays the region's `HouseholdAccount`. A save is refused for a
  batch past its duration or paid past its wages, or a pantry out of range.
- **Production**, each step, region by region in the scenario's order. A batch is whole totes, about
  `batchSeconds` of the row's rate and at least one, its duration what the rate takes; its wages are
  reserved as it starts, and an employer that cannot fund them starts none. It works at the region's
  rate, `(0.5 + health / 200) × (0.5 + happiness / 200)`, from the condition the step before left. A
  finished batch's totes go to the employer's holding at its venue and the rest of its wages are paid,
  and the next starts with the work left over; one with no room in the warehouse waits, finished.
- **The turn**, on its last step: the wages each batch has earned, rounded down, paid less what was;
  each payment split by the employer's carry, a credit withheld for every ten of gross, to the
  region's treasury and the rest to its households. Then each employer cancels last turn's offers and
  offers all it holds at its broker's bid. Then each region's households, at the first venue serving
  it, take each need in the tuning's order: its fresh requirement (the rate per 1,000 people per day,
  for the population and the 60 s), bought in whole totes at the brokers' asks, the cheapest of its
  goods first, within their funds, any remainder cancelled at once so no household bid rests; then
  consumed exactly from opened totes and whole ones opened in turn; a shortfall added to the need's
  unmet total, with no catch-up after. Every need met, each optional good is bought and consumed
  likewise. The severity moves health and happiness ([the rates](economy-and-development-plan.md#population-health-happiness-and-production-selected)),
  which the work rate reads from the next step; then the brokers requote.
- **Finite brokers and demand.** A broker still bids only what its funds cover (W03), and its stock now
  moves with employers' sales and households' purchases as well as players' trades; no account is
  refilled. Nothing moves goods between venues: the hosted world never calls `Simulation::AdvanceTick`.

Evidence, in the `trade` suite's `EconomyTests.cpp`: the work-rate table (100%, 75%, 56.25%, 25%) and
each condition's monotonic effect; severity's 25% two-need case, a third with medical supplies
missing, a surplus that cannot make up for it, a need with nothing required left out, and the weights
setting importance; 100/100 to 94/88 in an hour short of everything and to 97/94 in an hour supplied,
a partial shortage in proportion and both bounds; the 100/90/10 split, ten one-credit payments
withholding one, and cumulative wages rounded down to the whole bill at the end. On the hosted fixture,
one employer's batch of medical supplies reserving its 469 credits, paying 46 at the first turn (4
withheld, 6 carried) and 469 in all at 600 s (46 withheld, 9 carried) and selling its tote to the
broker, the next batch starting on the next step; at half the rate the same batch taking 1,200 s for
the same wages; an employer short of its wages starting none; Ore's households buying a tote of
medical supplies, of water and of one grain only, the cheapest, consuming the interval's need of
each, buying their optional salt, and resting no bid; with Ore's medical supplies gone, a severity of a
third, the interval's 125/1,440 kg recorded, health and happiness down by 6 and 12 points an hour
times a third, no salt bought, Brine untouched, and the next interval adding a fresh minute's
shortfall only; and a save part-way through a batch and a turn stepping on hash for hash, a batch
paid past its wages refused. In the `content` suite, the refusals of a production row with no region,
a region its venue does not serve, a venue serving a region with no market for an essential, the
production tuning out of range and a withholding rate that is not one in a whole number. The W03
`trade` cases run with the economy taken out, as W15's ran with the cruise; the `company` suite's
pinned hash moved for the save's new entities and goods.

Left: a batch interrupted or cancelled, which waits for facilities that stop (the Bronze recipe's
batch with inputs is W12's); work and residence in different regions; a player's view of a region's
condition and shortages; the economy's workload measured at scale, and 1,440 intervals' totals run
rather than reasoned; and the governance fixture's oxygen, cartridge and tin demand, a ship's and a
workshop's, which W06 and W12 give their consumers.

## Delivered: W06, 2026-11-20

Station life support, onboard use and resupply, `manifest.lifeSupport`, on pin `54fb871` with no
engine edit. What it chose where the plans left room ([D-094](decision-ledger.md#d-094), the user's
choices):

- **State.** Each ship has a saved `LifeSupport`: the oxygen left in each tank, in its hull's order
  (D-050's per-tank record), the occupant-days left in each installed cartridge, and the venue it is
  connected to. A ship's `Cargo` holds the totes opened for it beside its lots, at most one an item,
  each still taking its tote of the hold. A save is refused for a reserve below 0 or not a number, a
  tank or cartridge its hull does not have or holding more than it does, a connection to no venue, or
  a hold over its hull's, opened totes counted.
- **Connections**, each step after `manifest.travel`, ship by ship in entity order: a ship no longer
  docked where it is connected lets go, then a docked ship is connected while its venue has one of its
  `lifeSupportCapacity` free, so a ship docked at a full venue waits on its own supplies, and one
  freed by a departure is taken in the same step. A connected ship uses none of its own (D-046).
- **Onboard use.** An unconnected ship draws its hull's `occupants` times the step over a day, at
  `oxygenKilogramsPerOccupantDay`, from its tanks in order, and the same occupant-days from its
  cartridges in order, never more than is left: an empty tank or cartridge stays at 0, and exhaustion
  has no consequence yet. A hull with no occupants uses none.
- **Provisioning** gives the starter its tanks full and its cartridge fresh, once, connected at the
  starter venue, and is refused `NoServiceFree` when the venue has no connection free (D-047). No
  docking, connection, retry or load restores either.
- **Resupply**, the players' `Refill` and `Replace` of a ship, wherever it is: the goods are the
  hold's, the tuning's `oxygenItemId` and `cartridgeItemId`, bought through the book and loaded.
  Refill fills the tanks in order from the opened tote first, then whole totes opened one at a time,
  142.5 kg each; what the last has left stays opened. Replace installs a fresh cartridge in place of
  the one with the least left, thrown away with it, from an opened tote's whole pieces first. Each is
  refused, writing nothing, for an unknown ship, another company's, tanks already full or a hull with
  none to fill or replace (`CapacityExceeded`), and nothing aboard to give (`InsufficientStock`).
- **Snapshot and console.** A ship's view carries its life support and opened totes, so the protocol
  is version 3. The console's `observe` shows a ship's oxygen, cartridges and connection, and
  `service refill|replace [principal] <ship>` sends the commands.

Evidence, in the `company` suite: the starter granted 10, 10 and 10 kg and a 30-occupant-day cartridge,
connected at Ore, a retried provisioning restoring nothing, and Ore's four connections refusing a fifth
player `NoServiceFree` while a venue with connections to spare refuses at `NoPadFree`; ten connected
steps using nothing, then an hour of 1 s steps undocked using 1/24 kg from the first tank alone and
1/24 of an occupant-day, and a tank's last drop and an exhausted cartridge never below 0; a ship docked
at a full venue drawing its own until a departure frees a connection, taken in that step with nothing
drawn; Refill filling 2, 10 and 0 kg from a tote and leaving 124.5 kg opened, then drawing on the opened
tote first, undocked too, the opened tote no tote to unload and still a tote of the hold, and Replace
installing a fresh cartridge from a tote of two, then the second, then refused; each refusal writing
nothing; and the saves refused. In the `travel` suite, a journey from Ore to Brine letting go of Ore's
connection as it leaves its pad, drawing a kilogram and an occupant-day a day until it docks, and
connected at Brine in that step, after which it draws nothing; and the step's order with
`manifest.lifeSupport` last. In the `content` suite, the tuning's items refused naming nothing or in
another unit; in the `query` and `console` suites the view and the command. The `company` suite's
pinned hash moved for the save's new component.

Left: the station's own oxygen and its resupply, which a connection does not draw on, and service
outages, which wait for stations that stop; exhaustion's consequences; a tank exchanged rather than
refilled (D-050's exchange procedure), and spent cartridges kept; a berth fee, which includes the
connection when travel is charged for; and the trading fixture's venues selling oxygen and cartridges,
which only the governance fixture's do, for W09's whole fixture.

## Delivered: W07, 2026-11-20

Live and stale market observations, `manifest.observe`, on pin `54fb871` with no engine edit. What it
chose where the plans left room ([D-095](decision-ledger.md#d-095), the user's choices):

- **State.** Each venue has a `Market` entity, by its ID, whose saved `Sales` hold the totes each
  matching pass filled, by step and item, for at least the last sales window, and each item's last sale,
  its step and price, however long ago. Each principal's saved `Reports` hold their reports, by venue:
  how it was come by (`Docked` or `Starting`), the step it was observed at, the step its sales window
  began and, in the market's order, each good's best resting bid and ask with the totes at that price,
  the totes sold over the window and the last sale. A venue with no report is unknown. A save is
  refused for sales or reports out of order, a report of a venue the world does not hold, or a price
  and its totes that disagree.
- **Sales.** Every fill counts once, buyer and seller together, a broker's, an employer's and a
  household's as a company's, recorded after the pass commits; a cancelled or unfilled order adds
  none. The window is `market.salesWindowSeconds`, 3,600 s, a whole number of steps: the fills of the
  steps from `observed` less the window up to `observed`, shorter for a market younger than that.
- **Observation**, after `manifest.travel`: each ship docked at a venue (`Ship::dockedAt`) covers it
  for its company's principal, and each covered venue's report is taken once and given to each of them,
  observed at the step the clock reaches as the step ends, from that step's completed book; an order
  that no longer matches after it is left out. A report no ship covers stays as it was, so the last
  ship to leave freezes it at the step before it left, its window and sales with it, and another of the
  fleet still docked keeps it live. Coverage follows the ship's company at the boundary, a client's
  connection no part of it.
- **Starting reports** (D-095). Provisioning gives the new principal a `Starting` report of every other
  venue of the starter venue's system and every venue of a system directly connected to it, observed
  at the clock's step as the command runs; the starter venue is live from that step's end. A
  scenario's system names its direct connections, `connections: [{systemId}]`, each way, refused
  for a system the scenario lacks, itself or one named twice. Only provisioning seeds them, once.
- **Snapshot and console.** A player's snapshot carries their reports as `markets`, each live when
  docked and observed at the snapshot's own step, its age otherwise the steps since; the
  Administrator's carries none (D-095), so the protocol is version 4. The console's `observe` lists
  them, live, or stale with their age and a starting report marked, in simulation minutes, and the
  slice now buys at the ask Ada's live report of Ore shows and sells at the bid her live report of
  Brine shows. `book` stays the host's development view.

Evidence, in the `trade` suite: a new player's report of Ore live as the world holds it, and Brine's
the starting report of its spawn step; a trade at Brine leaving that report as it was while Ore stays
live; docked at Brine, the report live with the trade counted once and its last sale, and Ore frozen at
the step the ship left, a later trade there unseen; a second ship of the fleet keeping a station live,
and the last one's departure step's trade unseen by its player and seen by another player docked
there; a window's sales, by good, dropping each fill as the window passes it and keeping the last
sale, and a frozen report keeping its own; the starting venues over a four-system graph, a
neighbour's neighbour left out; and a save carrying the reports and sales on, refused for a report of
no venue. In the `query` suite, a player's markets their own alone, live only at the snapshot's step,
and none for the host; in the `content` suite, the connections and the window refused; in the
`console` suite, the slice played from the reports; in the `travel` suite, the step's order with
`manifest.observe` last. The `company` suite's pinned hash moved for the markets and reports a save
now holds.

Left: trader and script facts, which the hosted world does not have until W10's standing policies and
NPC jobs and the scripts after them, which are to read these reports and nothing else; the
development sandbox's `prices` and `plan`, which stay privileged and apart from the hosted world;
ship transfers, which do not exist yet; and each covered venue's report rebuilt every step from the
book, to profile with the long step ([L26](delivery-order.md#later-priorities-and-re-entry-conditions)).

## Delivered: W08, 2026-11-20

Company-funded cargo upgrades, `Manifest.Company.Upgrade`, on pin `54fb871` with no engine edit. What
it chose where the plans left room ([D-096](decision-ledger.md#d-096), the user's choices):

- **The offer.** `Manifest.Offer`, a versioned record of its own under `definitions/offers`, authored
  as records under `data/offers/` and in the content fingerprint: an `offerId`, the Skill node it
  sells (`skillId`), its `target` (`Ship`, the one target so far), its `cost` in whole credits a tier
  and the highest tier it sells (`maxTier`, the skill's `maxTiers` at most). Content refuses a skill
  that is not a ship system, a cost below a credit and a tier out of range. The trading fixture's
  `OfferCargoHolds` sells `SkillShipCargoHolds` tier 1 at 27,000 credits, beyond a new company's
  25,000 and within the 29,590 W15's furs run ends at. `resourceCostPerTier` is not read.
- **The ship's systems.** A ship's saved `Systems` hold the tiers its company bought for it, by skill
  ID, apart from any civilisation's technology; a ship that has bought none has none. Its hold is its
  hull's `CargoVolume` with each system's effect at its tier, computed as the effects engine computes
  a hull's stats (`ShipEffectEngine::Stats`, which `Recompute` now calls), never a cached capacity:
  the starter's 160 totes and `CargoHolds`' 2,400 a tier make 2,560. `Load`, the save's check, the
  snapshot and the console all read it through `Identities::Hold`. What a ship may carry is one
  rule, `Fits`, that the purchase and the save's check both hold to: each system a Skill node of the
  game mode, of a ship effect, at most its `maxTiers`, of an effect and an exclusive group no other
  system has, in a slot of the hull, its prerequisites fitted. A save is refused for systems out of order, below tier 1, that a ship cannot
  carry, or a ship over its hold.
- **The command.** `Upgrade` names the ship, the offer and the tier bought, one above the ship's, so a
  retried request is answered from the ledger and a new one for a tier the ship has is refused
  `AlreadyFitted`, neither paying. It checks everything before it writes: the ship, its owner, the
  offer, that the ship is docked (at any venue), the tier, a slot on the hull for the skill's effect
  and each prerequisite fitted to the ship, no other system of the effect (`NotFittable`), a hold
  it would give that holds what is aboard (`CapacityExceeded`) and the cost in the company's account (`InsufficientFunds`); then it pays the
  cost, which leaves the economy, and fits the tier together. The starter has a slot for `CargoHolds`.
- **Mass.** A hull's `mass`, in kilograms, is authored: the starter's 20 t, the others open, each
  above 0 as the float a body keeps, and its dock hull's box's inertia at most 10^18 kg m², far below
  what Jolt's float arithmetic overflows at. A scenario's starter hull must have one, and a ship is spawned at it; fitting adds none.
- **Snapshot and console.** A player's ships show their hold and systems, protocol 5; the console's
  `fit <ship> <offer> <tier>` sends the command, and `observe` shows each ship's hold and systems.

Evidence, in the `company` suite: a new company refused for its funds with nothing changed; with the
slice's 29,590, the tier bought for 27,000, the hold 2,560 and the system shown; the request retried
answered as it ran, a new request for the tier refused `AlreadyFitted`, and both leaving the credits
and the hold as they were; 2,560 totes loading where 2,561 do not; every refusal (ship, owner, offer,
tier, a second tier, undocked, a hull with no slot, an unmet prerequisite, a second skill of the
effect or of the group, a hold past the largest)
paying and fitting nothing; a save carrying the systems on, refused for each broken rule; and an upgrade saved pending, run after
the load, retried there and replayed from the start, paying once with the hashes equal. In the
`content` suite, the offer's and the mass's refusals; in the `data` suite, the starter's slot and
mass; in the `query` and `console` suites, the hold in the view and `fit`. The `company` suite's
pinned hash moved for the `Systems` component a save now registers, and only for that: with it
unregistered the old hash holds.

Left: the upgrade in the slice's whole fixture, earned by trading, with production, life support and
observations (W09); the long-haul trader's tiers and every other offer, which no scenario sells yet;
a hull's systems refitted or taken off, which no command does; and what a hull's systems add to its
mass.

## Delivered: L26, 2026-11-20

The console's long step, on pin `9e0274f` with one engine provision:

- **Beside the frontend.** `step` takes its run a slice at a time, at most 50 ms, so a short step
  reports at once as before and a long one answers with where it goes and carries on from the
  frontend's `Poll`, which the TUI calls again at once while a run is under way; the console reads
  its input between slices, and a request queued then runs at the run's next step. `stop` ends the
  run at the step it reached, with the same report (`Stopped: hosted world at step N.`). The TUI's
  pane shows the run's progress. `step` is refused while connected, since the server steps its own
  world and no client steps it, an administrator included, and while a run is under way; `host`
  and `connect` are refused while one is.
- **Where a step's time went.** Profiled (`perf`, a release build with frame pointers) over three
  hours of Ore to Brine's cruise on the manned profile: 26 µs a step, about 45% of it Jettison
  evaluating orbits, since physics placed each pad, deck and buggy on a station's rails by the
  station's whole orbit chain; Manifest's systems together were under 5%, the trade's 1.7%. The
  engine now places a rails body once a step for everything on it, to the same bits
  ([Jettison's history](../../jettison/docs/history.md#neutral-sdk-and-manifest-integration)): 13 µs
  a step, Ore to Brine about 4 min 50 s. What is left is the engine's movement, spread over Jolt's
  update of each frame's world, the docks, the spatial index and the cruise.
- **The budget.** `step-budget`, a ctest of its own run serially, times a cruising step in an
  optimised build without sanitisers, the best of five batches of 20,000, and fails above half as
  much again as the 13.2 µs measured on Lythir; it does not time on CI, whose machines are not
  Lythir's, nor in a debug or sanitised build.

Evidence, in the `console` suite: sixty days asked for answer at once with the run under way; the
world steps between polls; observe and a queued load run mid-run, the load's outcome in the report
`stop` prints; `step`, `host` and `connect` refused while stepping and `stop` with nothing
running; and connected as the administrator, `step` refused. The vertical slice's `step 1d` runs
through the same slices. Every pinned hash held across the pin.

Left: an admin's control of a server's time (a pause or a scale), which no message carries; and
the step's remaining cost, the engine's movement, which a fast-forward over quiet steps would cut
rather than any rate of Manifest's own.

## Delivered: W09, 2026-11-20

The slice's whole fixture, the `fixture` suite (`source/test/FixtureTests.cpp`), on pin `9e0274f`
with no engine edit. One command stream on the trading scenario, through admission and the player's
query alone after the bootstrap (Ada provisioned by the host, her ship's cruise on the drive's power
law as the `travel` suite's is, so the journey is minutes):

- **The loop.** Ore live and Brine her starting report; furs, a tote of oxygen and one of cartridges
  bought at the asks of her live report of Ore (25,000 to 24,691 credits for the furs), the goods the
  company's at Ore and the hold unchanged; a load past what is there refused, then all three loaded; a
  bid under Brine's market left resting, its escrow held; the ship sent to Brine, then a load, an
  unload and the upgrade each refused `NotDocked` with the business unchanged, and Ore's report
  frozen at the step before the ship left its pad. Cruising, the tanks refilled from the hold in
  flight, the rest of the tote kept opened. Docked at Brine with its cargo, Brine live and Ore stale;
  an unload at Ore refused `WrongVenue`; a fresh cartridge from the hold; the furs unloaded and sold
  at the bid of her live report, the proceeds hers and no sell resting; the resting bid cancelled, its
  escrow back; and `OfferCargoHolds` tier 1 bought for 27,000 credits, the hold 2,560 totes, a retry
  answered as it ran and a new request refused `AlreadyFitted`, neither paying.
- **The ledger**, checked every step: the world's credits (every account, buy escrow and batch wages
  not yet paid) change only by the upgrade's 27,000 and the starting credits provisioning makes; each
  item's whole totes (every holding, reserved ones among them, every hold's lots and opened totes)
  change only by the batches delivered in the step, and fall only in a step the households' pantries
  take what they open. The suite sees both happen, under an overlay of its own
  (`source/test/overlays/quick-batches`) whose batches are 300 s, so they are delivered within the
  loop; dropping the production allowance fails it.
- **Continuation and replay.** Saved cruising with the bid resting, a loaded world goes on to the same
  outcomes, step for step, and the same hash; the recorded commands replayed from the bootstrap reach
  it too; and the final hash is pinned, the same in Debug, Release, sanitised and the TUI off.
- **Directory and pack.** The two give the same content manifest, the same outcomes and hashes at
  every command and the same final world.
- **Content.** The trading fixture's Ore makes oxygen and cartridges and Brine wants them, as the
  governance fixture's Cassiter and Forge do, so a ship's resupply is bought where it starts (W06's
  left item). Ore's broker quotes sixteen goods; W15's credits are unchanged.

The plan's other cases are each held where their mechanic is: partial fills, cancellation, the
expiry boundary, same-price FIFO, two buys competing for one account and `BookFull` in the `trade`
suite; another company's ship and order, invalid quantities and the warehouse and hold limits in the
`company` and `trade` suites; a full station in the `travel` suite, which now also has a cruise the
engine refuses, the ship left where it is with its cargo; and the starting grant over a connection
graph, frozen reports and different players' views in the `trade` suite's W07 cases.

Left: trader and script facts read from the reports alone, which W10's standing policies and NPC jobs
bring; durable recovery, which is Stage 5's; and the manned profile's 4.49-day journey in the fixture,
which a release build steps in minutes (W15) and the suite does not.

## Delivered: L27, 2026-11-20

A fast-forward over quiet steps, on pin `892b7b3` with its engine provision
([Jettison's architecture](../../jettison/docs/architecture.md#quiet-steps)):

- **The engine's.** `SimWorld::FastForward` passes over the steps before the soonest any system has
  to run in, to the same bits as stepping them, each system saying when (`Sim::Quiet`); a system that
  says nothing runs every step. Physics, the docks, steering, cruising and lanes, which Travel attaches,
  have theirs; a kinematic body with nothing dynamic in its world is put where it is, asleep.
- **Manifest's.** Each of its six systems says when it next has to run, changing no rule: expiry at an
  order's lapse; the economy at its turn's last step, or sooner a batch finishing, one that can start
  or a finished one with room, its skip adding each batch's work once a passed step, the same addition
  `Produce` makes; the brokers at the turn's last step; observation the next step while any fleet is
  docked and never otherwise; travel the step its ship's cruise arrives in, never while it waits on the
  docks, and the next for any other leg; life support the next while a connection would be made or
  lost, its skip drawing each passed step's air and cartridges as `Breathe` does.
- **Used.** The console's `step` and the server's loop fast-forward to the step they were stepping
  to, the console still stopping at a journey's end.

Evidence. The `fixture` suite runs its cruise fast-forwarded beside one stepped, the bid resting,
batches finishing and the crew breathing: the same bytes at each of 4,031 stops, 19,042 cruising steps
in 17 calls and the arrival's 4,014 one at a time. `step-budget` now times Ore to Brine on the manned
profile through the console: 2.37 s, the best of three in a release build, where stepping took 4 min
50 s, and it fails above half as much again. The `company` and `fixture` pins moved with the
engine's bodies at rest, not with Manifest's quiets, which leave both the same: the fixture's whole
loop, every command's step and result, the ledger, the holdings, each batch's progress and the tanks
to the bit, and the ship's place, is the same on the old pin and the new.

Left: observation reports each docked fleet's venues afresh every step, so no step is passed over
while a fleet is docked, which W10's fleets will be; [L28](#delivered-l28-2026-11-21) lets them be.

## Delivered: W10, 2026-11-21

Standing trade policies, autopilot and NPC jobs, on [D-097](decision-ledger.md#d-097) (the user's,
2026-11-21), on pin `0f973b2` with one engine provision, from Manifest `feature-base` at `b1bf8e8`
and Jettison's at `28e8272`:

- **The executor.** `manifest.policy`, a native system after `manifest.brokers` and before the
  movement, so a journey it starts is driven that step and a buy meets the turn's requote. It reads
  its owner's reports as the last completed step's observation left them, so the live report a ship
  trades on at arrival is the one observed as it docked.
- **The command path.** `Command::Requests::Act` runs a registered command's handler for an actor
  as a request's would run, with no admission, sequence, retention or recording: the world's own
  consequence, sent again by a replay, as `SimWorld::Apply`'s are. `Running()` is 0 under it, so an
  order it places records request 0, as a broker's does. Every buy, load, journey, unload and sale
  of a job is `Trade::Place`, `Trade::Cancel`, `Company::Load`, `Company::Unload` and
  `Travel::Travel` through it, meeting a player's checks.
- **A policy.** `World::Policy` on a ship, set by `Manifest.Policy.Set` (`shipId`, `enabled`,
  `targetProfit`, `lossAllowance`, whole credits, each at least 0, both 0 by default) from the ship's
  owner. A trade starts only when its estimate is above `targetProfit`; a sale's floor is the lot's
  cost plus `targetProfit`, less what is left of the allowance. An edit is read at the job's next
  decision, held cargo included; allowance spent stays spent.
- **A job.** `World::Job` on the ship: `Choosing`, `ToSource`, `Buying`, `ToDestination`,
  `Selling` or `Blocked`; its item, source and destination; its lot's totes, its cost (what the
  company's account paid for the fills, fees included) and the allowance it has spent; and for a
  blocked job its reason, the step it first blocked at and its retry step.
- **Choosing.** From the owner's reports alone: for every known source and destination venue, apart,
  and each good with an ask at the one and a bid at the other, the totes are the least of the ask's
  and the bid's totes, the hold's free totes and what the account covers at the ask's `PerTote`; the
  estimate is the bid's proceeds net of its fee less the ask's cost and fee, by the market price
  rules, less the route's charges. **Charges** are real from W10 at nominal values (the user's,
  2026-11-21). They are each venue's own, `World::Fees` on the venue, which its operator sets
  ([D-098](decision-ledger.md#d-098)); W10 gives every venue `balance.yaml`'s `charges` defaults
  in the initial state and adds no command to change them. A venue's fees are posted, so an
  estimate reads them live rather than from reports, and a change applies to the next charge, not
  one paid. Transport is a docking fee, 10 credits by default, paid to the
  destination venue's account when a `Travel` starts (refused `InsufficientFunds` without it, so it
  never overdraws); and holding is a berth fee, 2 credits by default each simulation hour a ship stays docked,
  paid to its venue's account, what the account cannot pay carried as the ship's `Arrears` to that venue,
  taken first at the next hour and all of it before the ship leaves; arrears compound daily at the
  venue's rate, 10% a year by default, never above the usury limit of 35% a year (D-098). The estimate's charges are one function's, `Policy::Charges`: the
  destination's docking fee, and the source's if the ship must fly there first. A lot keeps the
  charges attributed to it as they fall once it is bought (`charges`, whole credits: its journeys'
  docking fees and the berth fees of the hours it is aboard), and its sale floor recovers them with
  its cost; the source's docking fee is the estimate's alone, and a failed journey's fee is not
  refunded (W18's). Their real amounts and bases,
  warehouse storage, tolls and the life support used in transit are
  [W18](delivery-order.md#priority-5-stage-4-trading-automation-and-regional-development)'s. The
  highest estimate wins, then a source the ship is docked at, then the scenario's order of source,
  destination and item. An unknown venue gives nothing, and none above the target blocks the job.
- **Buying**, docked at the source: the estimate again on the live ask, skipped if no longer above
  the target; then a buy at the live ask for the totes, its rest cancelled in the same run (an
  immediate-or-cancel made of the two commands), the lot's cost the account's fall, and everything
  bought loaded. **Selling**, docked at the destination: the lot unloaded and offered at the lowest
  limit whose fills, however the book splits them in up to `kMaxFills`, net the floor's share of the
  totes sold (each fill's nearest-credit gross and rounded-up fee may cost 1.5 credits against one
  price; `Policy::SellLimit`); the rest cancelled, a shortfall below the sale's share of the whole
  floor spending at most the allowance left, and what is unsold loaded back.
  Partial sales share the lot's cost proportionally, rounded up, the last taking the rest.
- **Blocked.** It keeps its cargo and reconsiders at the next economic turn's last step (every 60
  simulation seconds, after the requote), no sooner, since a turn is a minute: no wake on an owner's
  docking. Its reason (`NoTrade`, `JourneyFailed`, or `Refused` for a journey it could not pay
  for or goods it could not move), the step it first blocked at (kept across retries that change
  nothing), the lot's charges so far and the chosen trade's `estimate` are the player's to see
  through the query's ship view and on reconnect.
- **NPC jobs.** A scenario's `traders` name NPC principals and their companies, provisioned in the
  initial state, after the first quotes, through `Company::Provision` at the starter venue
  (`Policy::ProvisionTraders`), each ship given an enabled policy, its target and allowance 0, and
  their principals marked the world's own (`World::Principal`'s `npc`), as whom the server admits
  no client whatever the token file holds. The content check leaves the starter a pad and a
  life-support connection beyond the traders' for a player. They read their own reports, seeded as
  a player's are. Autopilot is the job's travel; no route mode.
- **Its quiet.** The current step while an enabled job has a decision due, its retry step while
  blocked, never otherwise: travel's quiet wakes the world for an arrival.
- **Jettison.** What W10 uses was public: commands sent from a system, the quiet and the docks. The
  zero-client run found one gap, a ship that came round Ore fast and sideways and circled the point
  above its pad until the pad lapsed; Jettison's docks now bring a ship to rest out along its pad's
  normal and fly it straight down (pin `0f973b2`, with its consumer test).
- **Evidence.** The `policy` suite: a route planned from a stale starting report, its estimate and
  report age shown; a live ask past the target skipped and replanned from reports alone; cargo kept
  when no bid clears the floor; zero allowance never selling below it; an authorised allowance shared
  across partial fills, retries and a save, never multiplied; an edit applied at the next decision; a
  change to an unobserved market changing no decision; and a blocked job's docked wait to its retry,
  3,600 steps, passed over in 4 calls to the bytes stepping leaves, since [L28](#delivered-l28-2026-11-21).
  The `fixture` suite: an NPC and a disconnected player's ship each completing trades on a server with
  no client, every arrival docked, saved mid-job, loaded and replayed to one hash in every gate build
  (the NPC added by the `one-trader` overlay), 90,000 steps in 42,483 calls. A job blocked in space
  after a failed arrival is flown every step.
- **Slices**, each a commit with the gate green: (1) `Requests::Act`, `World::Policy`, `World::Job`
  and `Manifest.Policy.Set` (`c399fa1`); (2) the nominal charges, with their pins moved and why
  (`800e26b`); (3) the executor, with the `policy` suite (`69fc0d0`); (4) NPC traders in the scenario
  and their provisioning (`02ccc42`); (5) the `fixture` suite's zero-client run (`a1be7ef`); the
  docks' pin (`aec90b7`); then L28 (`f72c906`), on a docked fleet stopping the fast-forward.
- **Left.** Scouting an unknown market, a standing route and Wasm policies; the charges' real amounts and the
  ones not yet charged (W18). A ship flown in to a pad stops out along its normal first, Jettison's stopgap for one that
  circled its pad until the pad lapsed, as Ada's did at Ore in the zero-client run; the quality
  solution is Jettison's ([roadmap](../../jettison/docs/roadmap.md#then-robustness-tools-and-what-a-mod-can-reach)).

## Delivered: L28, 2026-11-21

A docked fleet's wait passed over, on pin `0f973b2` with no engine edit. A live report is a function
of its venue's book and sales, the fleets docked there and the step observed: the window's edge and
an order's lapse move with the step, the rest only in a step that runs, since each system that writes
them wakes the world for it. So observation's quiet never asks for a step, and its skip captures each
docked fleet's venue once at the last step passed over, the report that step's own observation would
have made. No other rule changes; the examined alternative, naming the step a sales window's edge or
an expiry changes a report, wakes the world for what a capture at the clock already gives.

Evidence. The `fixture` suite's fast-forwarded cruise, compared byte for byte with one stepped at
every stop, goes on for an hour docked at Brine: 6 calls, where observation's old quiet took 3,600.
The `policy` suite passes over a blocked job's docked wait to its retry, 3,600 steps, in 4.
Every pin is unchanged, the zero-client run's included. That run is still 42,483 calls for 90,000
steps: its docked waits, Nia's blocked one among them, each fall while the other ship is flown in a
station's frame, departing, clearing or coming in, which steering flies every step; only the cruises
are passed over.

## W18 plan

Transport and holding charges at their real values, on [D-100](decision-ledger.md#d-100) (the
user's, 2026-11-21), from Manifest `feature-base` at `dfde305` on pin `0f973b2`. No engine edit
is expected: a journey's cruise time is Jettison's `Space::CruiseSeconds`.

- **Fees by pad class.** A ship's fee class is its dock hull's own pad class (`Content::FeeClass`),
  the smallest it lands on, whichever pad it is given. `World::Fees` holds a docking fee and an hourly berth fee for
  each pad class, a berth fee for each occupant hour while the ship is connected to the venue's life
  support, and the arrears rate. `balance.yaml`'s `charges` authors the defaults, which rise with
  pad area from the skiff's 100 to dock and 20 an hour: corvette 1,750 and 350, frigate 5,625 and
  1,125, destroyer 18,000 and 3,600, cruiser 54,000 and 10,800, and 2 an hour per occupant.
  `Travel::Travel` takes the docking fee at the ship's class, and `manifest.berths` takes the hour's
  class fee plus the occupants' share. A failed journey's docking fee stays paid, as in W10.
- **The operator's command.** `Manifest.Company.SetFees` (`venueId` and the whole `Fees`) is accepted
  from the principal directing the venue's operator organisation (`Organisation::principalId`). It is
  refused `NotAuthorised` from anyone else or for an AI-run operator, `RateTooHigh` above
  `charges.maxArrearsRateBps`, and `InvalidQuantity` for a negative fee or a pad class missing or
  unknown. It takes effect from the next charge: a docking fee already paid and an hour already
  charged stay as they were.
- **The AI operator's rule.** `manifest.pricing` runs on each day's last step, before the berth fees,
  for every venue whose operator no principal directs. Occupancy is the share of the station's
  non-display pads not free. Above 3/4 the venue's saved level (`World::Pricing`) rises by 1,000
  basis points, below 1/4 it falls by 1,000, and it stays within 5,000 to 20,000, starting at 10,000.
  Every fee is then the default at that level, rounded to the nearest credit, a half up. The arrears
  rate stays at its default. The thresholds, step and bounds are `charges.pricing` in
  `balance.yaml`.
- **Life support used in transit.** The company keeps the last unit price it paid for each item
  (`World::Paid`, set as its buy fills settle). While a job's lot is aboard and the ship draws on its
  own life support, `manifest.lifeSupport` adds what it draws to the job: oxygen in kilograms and
  cartridge occupant-days. When the ship docks at its journey's end, the policy converts the draw
  at those prices, rounded up, into the lot's `charges`, once, as the job decides on the journey
  ended: what a ship docked off the venue's life support draws after that is converted at its next
  journey's end, not rounded up each step. An item the company never bought, such as its provisioned
  supplies, costs 0.
- **Estimates and floors.** `Policy::Charges` adds to the docking fees, at the ship's class, the life
  support of each leg: the occupants times `CruiseSeconds` over the distance between the two stations
  now, at the same prices. The berth fees are not estimated, since a job waits docked only when it is
  blocked. Every charge that falls on a lot is in its floor, as in W10.
- **Arrears' consequences.** `World::Arrears` records the step it began. The ship stays held, as in
  W10. On the last step of each day after `charges.seizureGraceDays` (7), `manifest.seizure`, in the
  policy module, sells the ship's cargo through its owner's commands (`Company::Unload` and
  `Trade::Place`). It goes lot by lot in hold order, opened life-support totes excepted, a tote at a
  time against the venue's live bids, until the proceeds cover what is owed or the bids run out. The
  unsold rest is cancelled and loaded back, and the proceeds pay the arrears at once. Seized totes of
  a job's lot leave it as a sale's do (D-097's shares), and a job left with no lot chooses again.

Slices, each one commit, the gate run after slices 3 and 5 (the relaxed rule, 2026-11-21), the saved
layout's moved pins proven bytes, not state:

1. Fees by pad class and the occupants' berth share (`content`, `company`, `policy` suites).
2. `Manifest.Company.SetFees`, with a changed fee charged from the next charge, not one paid.
3. `manifest.pricing`, a full and an empty station stepped over days, fast-forwarded to the same
   bytes.
4. Transit life support: `World::Paid`, the job's draw, the estimate and the floor recovering it.
5. Seizure after the grace, covered by the bids and run out of bids, fast-forwarded to the same
   bytes.
6. Close-out: the delivered section, D-100, the delivery order, history and the handoff.

## Handoff to stage 5

Carry forward the exact scheduler order, command/result schema, content manifest,
owner/venue ledger layout, allocation counters, escrow representation and travel states.
Stage 5 adds durable acknowledgement, journal/snapshot recovery, backup/restore and
distribution relocation. No stage 4 success response should claim durability yet.
