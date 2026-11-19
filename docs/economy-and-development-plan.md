# Physical economy, brokers and planetary development

Product planning, 2026-11-15. Extends the [roadmap](roadmap.md),
[planetary/corporate goals](planetary-development-plan.md) and
[supply/booking contract handoff](procurement-and-station-contracts-plan.md).
The decisions below are requirements, not implemented capabilities. The first tech/production
recipe is selected below; schemas, other recipes, rates, fees and prices remain design work.
No code, content, SDK pin or
development environment was changed by this assessment; no builds or game simulations ran.

The subsequent [regional correction](regional-economy-and-governance-plan.md) supersedes
one-population/government-per-planet assumptions. Population, households, condition, public
budgets and local development belong to economic regions on a physical planet. The first
governance scenario must include multiple regions on one planet; each player initially
controls one region. Frontier resource concessions now name one region and cover its eligible
resources, leaving neighbours independently available under their own eligibility rules.
Port construction has one initial opportunity per eligible region, unaffected by neighbours'
ports, with unsolicited construction in other players' governed regions prohibited. Shared
ports may serve several regions on that planet under owner terms and protected bookings,
with physical delivery and separate economies/rights. Starting regions require continuing basic
public-port access; detailed transport, starter fees/capacity/funds and construction/control-change
rules remain open.

## Confirmed rules

- Add LPG (liquefied petroleum gas) as a distinct tradable commodity under the existing
  Fuels category, with inventory/trading measured in kilograms. Preserve fractional
  quantities; price, physical storage/packing and production/consumption uses remain open.
  The [LPG handoff](stage-2-content-plan.md#lpg-content-follow-up) records the implementer task.
- Authorised credits transfer instantly between entities, without distance or shipping delay.
  Whole-credit balances and native ownership/resource checks remain in force.
- Physical goods follow the full production, custody and transport chain. Mining output
  reaches station commodity warehouses before brokers/traders can trade and ship it onward
  to another planet or satellite with demand.
- Brokers trade their own inventory and funds and quote buying/selling prices. Merchant
  returns include broker spreads and separate warehouse handling charges at both ends of
  a planet-to-planet journey. The intended merchant cut is substantial; rates are undecided.
- Production/extraction capacity bounds new supply; only real available stock can trade.
  Scarcity and demand drive pricing. Production-based minimum/maximum price bands were not
  selected; existing numeric validation/authored price policy is preserved.
- Live market data requires a fleet member docked at that specific station. Retain dated
  last-known reports elsewhere. New players receive one-time spawn snapshots for every
  station market in directly connected systems. Fleet observation constrains UI, trader
  decisions and scripts; see the [market-information plan](market-information-plan.md).
- Consumer demand uses one aggregate household pool per inhabited region. Employers pay
  wages from available funds; the pool buys physically delivered consumer goods within its
  budget, prioritising essentials. Initial household/business funds are explicit generation
  state. GDP measures activity rather than automatically generating spendable credits.
  Batch regular wage payments and household purchases every 60 seconds of simulation time
  as an initial balance-testing setting; do not simulate individual citizens or families.
  Use an authored essentials basket scaled by population and elapsed simulation time.
  The first basket has three needs: staple food satisfied only by rice, wheat, oats or
  barley; drinking water; and basic medical supplies. Food is one need, not four separately
  required grains. Bread remains tradable but does not satisfy this staple-food requirement.
  Trade all four food grains in kilograms; 1 kg of any qualifying grain satisfies 1 kg of
  staple-food demand, including mixtures, while stocks/prices/storage remain distinct.
  Initial food demand is 500 kg per 1,000 people per simulation day, prorated across updates.
  Drinking-water demand is 3,000 litres per 1,000 people per simulation day; agricultural
  and industrial water are separate demands.
  Basic Medical Supplies is a distinct tradable kilogram-based commodity for routine
  medical consumables, required at 5 kg per 1,000 people per simulation day; specialised
  medicines retain their separate identities.
  Settle scheduled payroll before household purchases; optional purchases wait until that
  interval's essential needs are met. Carry unspent household credits forward. Each interval
  generates fresh consumption needs; record shortages without automatically adding missed
  consumption to later purchase targets. Numeric wages, remaining commodity definitions/quantities,
  detailed unmet-needs consequences and performance limits remain open.
- Track aggregate population health and happiness as authoritative simulation state per
  inhabited region. Shortages lower those values; both contribute to lower regional production
  output. Each is a continuous value from 0 to 100, where 100 is ideal and 0 the lowest
  modelled condition. Start both at 100 in the first balance-testing scenario; joining or
  acquiring an existing region preserves its current values. After household purchases on
  each 60-simulation-second economic update, unmet essentials lower both values, with larger
  shortages causing greater losses. Meeting all essential needs permits gradual recovery;
  happiness declines and recovers faster than health. Keep both within 0–100. Initial test
  rates per simulation hour are health −6 and happiness −12 at complete shortage, with
  partial-shortage losses proportional to severity; full essentials recover health +3 and
  happiness +6. Severity is the weighted average of each required essential need's unmet fraction,
  using authored importance weights, initially equal. Cap each need's fulfilment at 100%;
  surplus cannot compensate for another missing essential. Lower condition slows production
  work rate; completed batches retain their declared inputs, outputs and gross wage bill.
  The initial work-rate multiplier is `(0.5 + health / 200) × (0.5 + happiness / 200)`,
  bounded to 25%–100% of otherwise eligible work rate. Missing inputs/funds or an inactive
  facility can still stop work entirely; the condition floor does not bypass those gates.
- Withhold 10% of wages actually paid for the applicable regional public treasury, as an initial
  balance-testing rate. Split the employer's gross payment between households and the
  treasury at payroll settlement: 100 credits pays 90 to households and 10 to government.
  Carry the withholding remainder across payments for each employer and region: every ten
  cumulative gross credits paid produces one treasury credit. Batch completion/cancellation
  does not reset the remainder. Wage rounding follows the production-batch rule below;
  later rate-setting powers remain open.
- The first economy scenario uses declared starting credit balances without periodic currency
  creation or automatic account replenishment. Wages, taxes, purchases and fees transfer
  existing credits. Expose cash shortages and measure circulation before choosing any
  continuing currency source for later scenarios.
- Each production batch declares a gross wage cost. Reserve that payroll alongside required
  inputs before starting; insufficient funds block new batches while existing stock remains
  sellable. Reserved payroll cannot fund another project or purchase. Wages accrue in
  proportion to completed work and are paid from the reservation on economic updates.
  Interruptions pause work and accrual; cancellation pays earned but unpaid wages and releases
  unearned payroll. During production, round cumulative earned gross down to whole credits,
  retaining fractional work progress. Completion pays the full declared payroll; cancellation
  rounds cumulative earned gross up once, capped at the reserved total, less earlier payments.
  Each production recipe specifies an authored gross wage bill, shown before starting and
  fixed for that batch. Numeric wage amounts remain to be selected; regular payments use
  the selected 60-second simulation-time economic cadence.
- Regional tech advancement requires exploited resources, purchased/imported resources the
  region cannot gather, and dependencies on actual production facilities.
  The first selected example is Bronze metallurgy: an operational local workshop and basic
  smelting knowledge, 90 kg copper plus 10 kg tin for the project, followed by repeatable
  production batches of 9 kg copper plus 1 kg tin into 10 kg bronze. Quantities are initial
  test values; work duration, throughput and profitability remain to be balanced.
- The inner map starts with higher civilisation technology and fewer remaining natural
  resources due to historical exploitation. Outer worlds are generally less developed and
  resource-rich, encouraging a gold rush for control of remaining deposits and access.
- A corporation receives an initial exclusive exploitation/lockout period for eligible
  resources in one explicitly named frontier region, followed by opening to rival exploitation
  at a disadvantage. Measure the period in wall-clock units,
  counting elapsed real time only while the server runs. Shutdown time does not count.
  Rival spaceports become permissible after the period; the one-port restriction is an
  initial protection rather than a permanent cap. Frontier concessions reopen even under an
  incumbent's puppet government. Other players' governed regions are protected against
  unsolicited port construction, without turning frontier grants into permanent exclusion.
  Start the grant when the first eligible frontier spaceport becomes operational and its
  exploitation concession activates. Publish the duration before construction starts; use
  72 hours of actual server-running time as the initial balance-testing term. Idle facilities
  and disconnected owners do not pause it. Ownership transfers, rebuilding and government
  changes cannot restart or extend it. The concession does not itself transfer government
  control. Sibling regions remain independently eligible; activation or expiry in one region
  cannot start another's timer or change its resource rights. One initial port-construction
  opportunity per eligible region replaces the planet-wide restriction. A shared port can
  serve several regions on that planet under owner terms without pooling economies or
  transferring rights. Starting regions have guaranteed basic public-port access; detailed
  transport and starter fees/capacity/funds remain open. Extraction
  capacity, costs and server pacing need validation.

Proposed details below are distinguished from these rules. Interpret inner/outer relative
to the established region of the server map, not orbital distance from a star. Exact map
geometry, starting distributions, resource rights and initial-player placement remain open.

## Credits, title and custody

Instant credit transfer means settlement occurs at the validated authoritative command/step
boundary with no in-world travel period. It does not bypass authentication, retry detection,
reserved-funds checks, atomic debit/credit, overflow checks or durable acknowledgement rules.
Distance does not prevent paying a distant supplier, broker, warehouse or public authority.
Do not make currency another physical cargo type.

Only entities with a valid ledger/account can receive credits; entity existence alone does
not make a ship, raw resource node or scene decoration a treasury. The host resolves the
entity's authorised beneficiary/account. Public, company, broker and facility accounts may
have distinct owners; explicit transfers are possible without aliasing their balances.

Goods need separate facts: item/quantity, owner, physical location, custodian and reservation
or in-transit operation. A warehouse can hold commodities for many companies without owning
all of them. Buying from a broker changes title to goods already at that warehouse; the
buyer still needs to load a docked ship. Sending credits cannot relocate those goods.
Selling at the destination likewise requires goods physically available there.

For the first trading slice, the company carries its own goods in its ship. Third-party
freight/consignment can be added later using the same ownership/custody distinction. Do not
force a fake sale whenever a company moves its own goods through a facility.

## Physical chain and warehouse operations

```mermaid
flowchart LR
    R[Remaining planetary resources] --> M[Mining or extraction stock]
    M --> H[Surface and orbital haul]
    H --> A[Origin station warehouse]
    A --> S[Trader ship cargo]
    S --> B[Destination station warehouse]
    B --> L[Local delivery]
    L --> I[Industry, consumption or development]
```

Every arrow is a native capacity/time/ownership-validated operation with saved progress.
Individual surface vehicles/shuttles may be abstracted into deterministic transport jobs;
their source debit, in-transit goods, destination receipt, timing and capacity remain explicit.
No renderer, terminal refresh or global stockpile synchronisation completes a handoff.

Extracted output initially belongs at the operation's local store. Only delivered warehouse
stock becomes available for a broker to offer there. Industry similarly consumes inputs at
its actual location and places output locally before transport. Production and consumption
are intentional sources/sinks with accounting; transport cannot create or duplicate goods.

Warehouse operations distinguish receipt, custody/title transfer, reservation, load-out,
storage and collection. A combined unload-and-sell can be a bounded native transaction, but
must still debit ship cargo, record warehouse custody, transfer title to a funded broker
and settle the proper charges exactly once. Partial acceptance leaves excess with its owner.
Delivery to an orbital warehouse does not satisfy a surface factory's input requirement
until the agreed surface delivery has happened.

Reserve actual stock and transport/storage capacity at the appropriate commitment boundary.
The same goods cannot back two broker offers, a public project and a ship load. Quantities
remain floats/doubles under the existing policy; finite/range/canonical-quantity rules must
reject unavailable amounts without minting small quantities through epsilon allowances.

## Brokers and merchant income

A broker is a trading principal with its own finite treasury and commodity holdings at a
specific station warehouse. It buys at its bid and sells at its ask. The spread is embodied
in those prices, not a second debit labelled spread on top of the same transaction. Profit
depends on actual purchases, sales and costs; quoting a spread does not generate credits.

Displayed ask quantities cannot exceed unreserved local stock. Bids cannot exceed buying
capacity backed by unreserved funds, including applicable charges. Use the existing native
market reservation/matching machinery where suitable, with the hosted owner/location adapter;
do not invent unlimited broker liquidity or a parallel loader/economy. Initial ordinary
trader acceptance scenarios use real broker counterparties. Whether later direct player
order crossing is offered separately remains a design choice.

Supply, stock cover, outstanding funded demand and expected consumption/production can
inform quote policy. Broker prices remain Decimal per-unit values; quantity and dimensionless
scarcity inputs remain doubles. Update decisions on declared economic cadence, retaining
deterministic ordering. Existing authored prices and fee rules are not silently replaced by
this plan. Precise broker algorithms, margins and work bounds remain open. External views
follow the selected [fleet-observation rules](market-information-plan.md); a broker's access
to its own actual stock/book does not grant other traders global real-time information.

Warehouse handling charges are separate income for the facility/operator actually providing
the service. The proposed first trader charge points are origin load-out and destination
receipt; producer inbound handling and final local collection belong to their respective
legs/payers. Specify each service, payer, beneficiary, basis, commitment and receipt. An
ownership change in one warehouse is not an imaginary physical load-out/load-in pair.
Broker spread and distinct handling charges can coexist without charging one handoff twice.

Owner, broker, warehouse operator and government may be different entities even at one
station. When they share a company, preflight aliased accounts and consolidate transfers
correctly: an internal charge cannot mint company wealth. Existing market listing fees
must be reconciled explicitly with new handling/docking services rather than relabelled or
quietly doubled. No new numeric fee rate or authored ABI change is approved here.

Goods handling pays the selected per-service charge when that handoff is accepted. Reserve
funds/caps at booking where applicable, capture actual service once and return unused funds.
Credit movement itself has no distance/transport fee introduced by this requirement.
First public supply contracts also retain accepted-partial-delivery payment. Define whether
their offer includes handling and which contractual delivery point triggers public ownership
and payment; bidders must include their assigned costs in the price.

## Production caps, inventory and auto-trading

Production capacity limits new output over a declared simulation period, subject to real
inputs, resources and operating state. It does not require selling only this period's output:
warehouses can accumulate earlier production. Instantaneous trade availability is actual
unreserved stock. Physical export throughput is additionally limited by handling, storage,
ships, port access and transport capacity, not a hidden production-based sales quota.

Deposits have initial/remaining resource state distinct from extraction rate. High resource
abundance is not infinite extraction per step; high industrial capacity does not create
missing raw inputs. Exact depletion/renewability and recipes require definitions, including
unit conversions, sources/sinks and production facility requirements.

For an auto-trader, expected route proceeds are destination bid receipts minus origin ask
cost, warehouse handling at both ends, docking/storage/transport and any other separately
contracted costs. Origin ask and destination bid already incorporate broker spreads; do not
subtract those spreads again. Build estimates from the actor's permitted live or last-known
station observations, own available funds/cargo capacity and known local inventory. Unseen
depth/price estimates retain their observation age; exact current destination availability
is validated when an action executes. Neither nominal production nor remembered quote depth
guarantees an executable quantity. Route ranking and candidate selection must not use hidden
current market state; see the [market-information plan](market-information-plan.md).

Keep working capital for destination charges. An expected future sale is not spendable cash
or a guaranteed buyer. Quotes can expire or change while travelling unless a specific
accepted purchase/sale contract locks them. Jobs need cost/quantity limits, stale-price
handling and alternatives or a visible blocked state; none require a connected player.
Broker stock/fund exhaustion and unprofitable routes are legitimate outcomes.

## Planetary tech and production-facility dependencies

Tech advancement is a physical development project. Its definition identifies earlier tech
nodes, required built/operational production facilities, material quantities and work/progress
requirements. Inputs include resource exploitation and imports when the planet lacks the
resource/capability, or when the target region lacks access to it. Same-planet external
supply still needs explicit physical delivery. Buying a tier for credits alone is insufficient
for this new hosted
progression path. Existing development-harness skill purchases remain current behaviour
until their planned hosted replacement exists.

The government prioritises projects/budgets and procures inputs competitively. Suppliers can
source imported materials through brokers and physical shipping. Public goods stay in their
own location/reservation records; company ownership of a port or control of government does
not make every stored commodity public or free. Immediate payment cannot substitute for
uncompleted delivery. Public project fees/booking terms follow the linked contract plan.

Separate knowledge prerequisites from installed capacity. The selected first project checks
its required operational facility before work starts and pauses work if it becomes unavailable.
Persist material reservation/consumption and progress so resuming cannot spend inputs twice.
Its completed knowledge remains unlocked after facility loss, while production requires an
operational workshop. Exact maintenance, operational-state criteria and rules for other
projects still need design.

Completion enables the defined new capabilities or capacity. Recompute affected production,
consumption and future quote inputs without regenerating existing population, stored goods,
physical deposits or identity. Scope local effects to the target region unless explicitly shared.
The existing `ProgressionManager` node/resource-pool model does not establish timed research,
facility/material dependencies or per-region job state; those are native Manifest work using
Jettison world registration, definitions, commands, archives and hashing.

Validate more than the tech graph: recipes, required facility levels, geographic resource
availability and transport access can form cross-system dependency cycles. A primitive
planet must not need a material produced only by its own locked future technology unless
an accessible external supplier can provide it. Seed appropriate AI industry/stock/access
for bootstrap as explicit initial conditions. Preserve existing authored IDs/YAML keys and
use Jettison schema/layering mechanisms for any planned definition extensions.

### First tech example: Bronze metallurgy (selected)

The user selected a Bronze metallurgy project for the separate governance scenario. Under
the regional correction, its public project and local production unlock target one region.
Require basic smelting knowledge (existing `SkillSmelting`) and an operational local
metalworking workshop. Seed the initial workshop explicitly so access to it does not require
the technology being researched; its construction recipe remains later work. Workshop/project
state is planned Manifest gameplay, not an already implemented facility or SDK capability.

The project requires 90 kg of `ItemCopper` and 10 kg of `ItemTin`, physically delivered into
regional public project inventory. The demonstration region supplies copper locally and imports tin
through competitive procurement and shipping. Provide an explicit reachable external tin
supplier/bootstrap state; the project must not require locally unlocking the result before
it can import its input. Existing item IDs, age metadata, unit types and authored prices remain
stable; the hosted eligibility contract must support that prerequisite import.

After all inputs are secured and the workshop is operational, consume the project materials
once and advance a fixed-step work counter. Workshop interruption pauses progress; resuming
does not consume another set of materials. These project inputs are consumed by development,
not simultaneously credited as a saleable bronze batch. Completion unlocks the local
production capability once, without advancing sibling regions or regenerating the target world.

Each subsequent production batch consumes 9 kg of copper and 1 kg of tin to produce 10 kg of
`ItemBronze`. These authored commodities are metal ingots, not raw ore. Every batch needs fresh
physical inputs and an operational workshop. Knowledge survives workshop loss; production
cannot continue without the required operational facility. Specify input/output ownership,
facility access and batch timing/interruption semantics before implementation.

The project and batch quantities are starting test values. Work duration, production
throughput, workshop construction/operating costs and profitability remain balance/design
work; the recipe is not evidence of a profitable conversion at existing prices and fees.
Use the separate governance fixture, retaining the small eight-good trading fixture and
existing full/simple modes rather than silently inserting bronze into them.

Required implementer evidence includes local copper plus physically imported tin, partial
procurement without premature work, rejection of missing/reserved inputs, facility interruption
before/after input consumption, retained knowledge after facility loss, and independently
accounted repeat production. Replay/recovery at consumption, progress and completion must
preserve inputs/outputs and apply the unlock once. No fixture, content or test was changed or
executed for this planning decision.

## Established core and resource frontier

Generate a map with an established inner region of generally higher-tech, historically
exploited worlds, and a generally lower-tech outer region with richer remaining resources.
Distance from a declared map centre/settled core can guide distributions; exact radial bands,
exceptions and resource mixes are not chosen. Do not confuse this with star-orbit habitable
zones. Production/extraction capacity and remaining deposits are separate generation values.

Represent historical exploitation through explicit initial depleted reserves/history
parameters rather than requiring a long simulated prehistory or changing already-generated
worlds later. Initial factories, populations, treasuries and warehouse stocks must be declared
bootstrap state with known provenance. High initial tech does not require every industry to
be self-sufficient; core demand for frontier imports is part of the intended trade pattern.

The gold rush is a race for resource rights and costly enabling infrastructure. Outer riches
are not immediately sellable stock: companies need funded exploitation, a port, warehouses,
transport and buyers. One initial orbital port-construction opportunity is available per
eligible own, AI or unclaimed region. A neighbour's port does not block it; other players'
governed regions are protected against unsolicited builds. The initial resource right is
time-limited exclusivity, followed by rival entry rather than a permanent monopoly. The
selected grant trigger is the first eligible frontier spaceport becoming operational together
with activation of its explicitly region-scoped exploitation concession. This grants rights
only over that region's eligible resources; neighbouring regions remain independently available
under their own eligibility rules. It does not grant government control. A shared port may
serve several regions on that planet under owner terms and protected bookings. Deliveries name
a destination region and require physical transport, without pooling inventories/funds,
granting government/resource rights or consuming a neighbour's initial construction opportunity.
Market visibility remains station-specific. Detailed transport costs/capacity/times and
starter fees/capacity/funds remain open; do not apply the old planet-wide port restriction as a resource
lockout over every region. Publish the duration before construction
starts; the initial balance-testing term is 72 real server-running hours. A construction
reservation alone does not start the resource grant or confer government control.
The selected [construction-site rules](planetary-development-plan.md#company-built-stations-and-control-of-shipping)
give the first eligible funded project a reservation, requiring non-refundable mobilisation
and reserved construction funds. A fixed completion deadline cannot be renewed indefinitely
by token deliveries; abandonment/expiry reopens the site while preserving ownership of unspent
funds and unused goods. Salvage of completed work, exact amounts and the deadline clock/duration
remain unresolved. Construction priority does not itself grant resource or government rights.

Late joiners and AI retain the previously planned capacity considerations. Regional port
eligibility supersedes the old unassigned-planet rule. Onboarding reservations, existing funded
claims and control changes during construction need an explicit compatibility policy;
acquisition ineligibility alone does not settle these construction cases. Gold-rush urgency
and regional first-port races amplify this issue. The selected starting-access rule requires
an operational public station supporting the starter ship and a physical delivery connection
to the allocated region. Basic docking, trading and regional delivery remain available and
cannot be revoked by corporate players. Service charges, finite capacity and finite goods/funds
still apply; private ports keep commercial access policies. Balance exact fees, capacity and
starting funds before claiming equal viability or long-running capacity. The guarantee is
service access rather than a source of goods, spendable credits or assured profit.

Later expansion should follow the same generally rich, less developed frontier profile.
It must append compatible generated world data without moving/regenerating the core and
preserve routes, identities, in-flight goods and rights after restart. Static-map supply
chains and tech progression should be proven before live topology expansion.

The selected [stargate access project](stargate-expansion-plan.md) requires government or
corporate funding and physical automated/unmanned delivery at FTL speed before opening a new
system to trade/development. Distance, configurable FTL speed and pacing determine a quoted
hours/days duration in server-running wall time. Shutdown pauses flight; disconnected clients
do not. Operational gate owners control access and fees, including refusing rivals. Generation,
delivery, gate activation, port construction and resource concessions retain distinct state;
the gate does not itself grant the 72-hour regional exploitation term or government control.

## Real-time exclusive exploitation

The user selected the operational-port/concession trigger and provisional 72-hour term above,
and subsequently scoped each concession to eligible resources within one explicitly named
frontier region. Neighbours remain independently available under their own eligibility rules.
Activation/expiry in one region neither starts another's timer nor changes its resource rights.
The duration must be disclosed before construction starts. Shutdown pauses the entitlement;
idle facilities and owner disconnection do not. Ownership transfers, rebuilding and government
changes cannot restart or extend it. These rules do not establish the remaining colonial
powers or the handling of pre-existing AI governments. The term is a balance-testing value,
not evidence that construction investment pays back or that server pacing supports it.

The exclusivity term is measured in real-world hours/days or other chosen wall-clock units.
Simulation step count is not the entitlement timer. The corporation can try to extract as
much as physical capacity allows before rivals arrive; competitors may find depleted or
exhausted deposits. Do not add replenishment, reserved rival stock or extraction quotas as
though the user selected them. The incumbent's earlier infrastructure, supply contracts,
warehouses and accumulated capital can preserve an advantage after access opens.

Resource access, station ownership, government control and legal planet title remain
separate records. Expiry does not transfer the first port or planet to another player.
Rivals may build their own spaceports after expiry; this explicitly amends the earlier
one-initial-port construction condition for reopened frontier regions. The incumbent may
still deny new bookings at its own facility while rivals fund independent mining, warehouses
and transport. Rival entry offers opportunity, not free infrastructure or guaranteed reserves.
Frontier reopening remains valid under a puppet government. Home regions retain separate
protection against unsolicited port construction. Store that region/right
classification explicitly: changing a government,
owner or company name cannot relabel a frontier concession as a protected home. Define
onboarding/home-pool eligibility without retroactively cancelling existing frontier rights.
The regional construction rule supersedes the earlier planet-wide restriction. Shared service
does not consume a neighbour's construction opportunity or change its resource rights.
A region-scoped concession cannot itself block ports or extraction in
sibling regions, and home classification cannot cancel frontier reopening.

Preserve deterministic simulation by placing real-time observation in the server host.
Propose one saved cumulative server-running clock per world, advanced from a host monotonic
clock while that world runs. A grant stores its ID, parent planet, named region, company,
eligible resource scope, original duration, grant value on that running clock,
accumulated/remaining entitlement and lifecycle revision.
An absolute UTC expiry date is unsuitable because shutdown time does not count. Record
UTC only for diagnostics/estimated dates, without using it to deduct offline entitlement.

The host records time observations and due-expiry commands/events with assigned simulation
ordering before changing authoritative access. Systems use saved rights and recorded events,
never a direct clock read. Replay consumes the recorded inputs without reading the replay
machine's date or applying the same elapsed time twice. Reuse world services, archives,
commands and the accepted-order journal; the TUI owns no entitlement scheduler.

Persist elapsed-running-time checkpoints with committed world/journal boundaries and flush
on clean shutdown. Restart resumes the last committed clock with a fresh monotonic origin;
it must not deduct the offline gap. A crash can lose elapsed time since the last durable
observation unless an external process measured it. Specify and measure the maximum checkpoint
extension instead of claiming exact crashed-process uptime is knowable. Do not choose wall
time since the last checkpoint as a shortcut: it would count downtime. Define which lifecycle
states count as running; propose excluding startup/recovery and stopping, with deliberate
administrative pause behaviour still to be specified if that feature is added.
Commit the time observation with the corresponding extraction/state batch so production
cannot survive a crash while its elapsed-time accounting is discarded. Durable rival rights
must retain the expiry transition that made them valid. This requires recovery evidence,
not just displaying a countdown in the client.

Expiry is idempotent and irreversible for that grant revision. A restart, booking, company
rename, station transfer or reissued RPC cannot restart the exclusion period. Rebuilding and
government changes cannot extend it either. Scope the remaining ownership-transfer terms,
including who receives the existing right, before exposing them. Order due events before dependent commands
at the chosen boundary, recheck rights at execution and journal the transition before durable
results. Delayed/duplicate timer observations must not grant a second lockout or strand an
already-open right. Authoritative time comes from the server, never a client's clock.

Clock-origin changes, crash checkpoints and due-at-restart cases need fault tests. Never
decrease the committed running clock or re-close a concession after expiry. Display remaining
server-running duration, with any calendar estimate explicitly assuming continued operation.
The simulation may lag under load even while real time passes;
measure extraction achievable in a real-time term at the selected server pacing. Faster
test stepping alone does not expire real-time rights: use an injected host clock and replay
the recorded inputs. This exception does not make every development or tender timer real-time.

## Corporate power and long-running circulation

Controlling scarce inputs, stations, handling, broker liquidity or import access can constrain
other planets' tech development. Merchant income can finance more resource infrastructure,
creating the intended accumulation of wealth and influence. Other players' planets remain
protected from the agreed acquisition/port-building mechanic, but not from economic dependence
or refusal of new station bookings. Existing bookings must still be honoured.

Large merchant cuts can drain producers, public budgets or traders of working capital. Give
every actual purchase a funded buyer and every charge one beneficiary. Long-running tests
must distinguish intended wealth concentration from an accounting leak, unsupported infinite
demand or a world that stops trading because no economic actor can finance the next step.
The aggregate household/wage foundation, initial wage-tax rule and first-scenario monetary
rule below are selected. Detailed wage and demand policy and further public-income rules
remain to be designed. A public budget may pause when exhausted. The first scenario has no
periodic issuance or automatic replenishment; any later continuing currency source needs a
separate decision informed by circulation evidence.

There is no win condition or automatic victory reset. Before promising days/months of viable
idle play, measure turnover, concentration, depleted reserves, stranded inventory, broker
liquidity, project bottlenecks and availability of alternate suppliers. Adjust authored
scenario/balance intentionally; do not hide missing demand with teleport trades or minted
broker accounts.

### Aggregate households, wages and consumer demand (selected)

Use exactly one pooled household account per inhabited region, independent of population
size. Track aggregate population, available credits, demand by commodity and needs fulfilment.
Individual citizens and families have no separate simulated accounts, agents or shopping
jobs. A population increase changes aggregate quantities, not the household entity count.
The public treasury, employers, brokers and household pool retain separate accounts.

The selected regional population unit is an aggregate count of people. Keep population
under Manifest's float/double policy; this does not create individual citizen entities.
Author consumption rates per 1,000 people per simulation day (24 simulation hours, or
86,400 simulation seconds). For rate `r`, regional population `P` and interval duration
`dt` in simulation seconds, required quantity is `r × (P / 1,000) × (dt / 86,400)`.
Use each need's declared physical unit; staple-food demand is in kilograms. Preserve
fractional interval quantities instead of rounding each purchase interval to whole units.

A region of 25,000 people needs 25 times the authored daily basket, spread over 1,440
normal 60-second intervals per simulation day. It still has one household pool. Zero
population generates no population consumption demand. Initial population sizes, growth
and migration rules remain open; the first staple-food quantity is selected below.
The prototype's `Population` and `Civilisation` use abstract world units. Any supported
migration must declare an explicit conversion to people and preserve regional/planet
totals; do not silently reinterpret one legacy world unit as one person.

Employers pay aggregate wages from their own available funds into the local household pool.
Use an authored essentials basket whose aggregate quantities scale with the region's
population and elapsed simulation time represented by the economic update. Settle scheduled
payroll before household purchases. The pool buys essentials within its available funds
and physically delivered local stock. Optional purchases may proceed only after that
interval's essential needs are met; an essential stock/funding shortage leaves optional
purchases blocked. Carry unspent household credits forward to later updates.

The selected first basket has three equally weighted essential needs: staple food, drinking
water and basic medical supplies. Following the initial bread/water/medicine selection,
the user selected rice, wheat, oats and barley as the only qualifying staples. They are
alternative ways to satisfy one food need; households do not have to purchase every grain.
Bread remains a tradable commodity outside this first essential basket. The selected
medical need uses a distinct Basic Medical Supplies commodity, traded in kilograms and
representing routine medical consumables. Existing antibiotic/anaesthetic/analgesic items
remain separate commodities; this selection does not make them substitutes for the basic
supplies need. The selected medical rate is 5 kg per 1,000 people per simulation day;
remaining item authoring values are open.
Drinking water is `ItemDrinkingWater` ([D-060](decision-ledger.md#d-060)). Keep all three need weights at 1
initially; adding grain alternatives cannot increase food's weight relative to water or medicine.

Trade all four food grains in kilograms. For initial balance testing, 1 kg of any qualifying
grain satisfies 1 kg of staple-food demand; mixtures count equally. A 100 kg requirement can
be met by 60 kg rice and 40 kg barley. Each grain retains its own price, stock and storage
properties. Consumption removes the actual grain used; it does not create a separate food
commodity, turn one grain's inventory into another or count consumed cargo twice. The
equivalence is a selected game-balance rule.

The selected initial staple-food requirement is 500 kg per 1,000 people per simulation
day, summed across the qualifying grains. This is an authored balance-testing value.
For 25,000 people the target is 12,500 kg (12.5 tonnes) per simulation day, or
`12,500 / 1,440` kg (approximately 8.68056 kg) per normal 60-second update. Preserve
fractional quantities; do not round each interval to whole kilograms. A qualifying
single grain or mixture meets the same food requirement, not 500 kg of each grain.
The selected requirement generates demand, not guaranteed purchases or deliveries.
Finite household funds, actual physical supply, recorded shortages and no catch-up
purchasing still apply. This rate does not establish economic viability.

The selected initial drinking-water requirement is 3,000 litres per 1,000 people per
simulation day. Agricultural and industrial water are separate demands, outside this
household drinking-water requirement; their quantities and rules remain unselected.
For 25,000 people, drinking-water demand is 75,000 litres (75 cubic metres) per simulation
day, or `75,000 / 1,440` litres (approximately 52.08333 litres) per normal 60-second
update. Prorate by elapsed simulation time and retain fractional quantities. Count only
actual accessible potable water consumed once, within physical supply and household funds.
Insufficient supply/funds record unmet water needs under the existing severity and
condition rules; no automatic catch-up purchasing follows. This is a balance-test setting.
The water commodity is `ItemDrinkingWater`, traded by the kilolitre as every fluid is
([D-060](decision-ledger.md#d-060)): the need is authored as 3 kL per 1,000 people per day, the
same volume.

The selected initial Basic Medical Supplies requirement is 5 kg per 1,000 people per
simulation day. For 25,000 people, this is 125 kg/day, or `125 / 1,440` kg (approximately
0.086806 kg) per normal 60-second update. Preserve fractional quantities and consume only
actual accessible basic supplies once, within household funds and physical stock. Missing
supplies contribute to essential-shortage severity and condition losses; excess food/water
cannot compensate. Retain fresh needs and no automatic catch-up demand. This is an authored
balance-test rate, with no patient-level treatment model or economic viability result.

The complete selected initial daily basket is:

| Essential need | Per 1,000 people per simulation day | Per 25,000 people per simulation day |
| --- | --- | --- |
| Staple food from the four qualifying grains | 500 kg total | 12,500 kg total |
| Drinking water | 3,000 litres | 75,000 litres |
| Basic Medical Supplies | 5 kg | 125 kg |

Each need retains importance weight 1. Population/time scaling and fractional interval
requirements apply to all three; physical units do not set importance.

Surplus qualifying food cannot compensate for missing water or
medicine. Retain physical goods/ownership, finite household budgets and the single-consumption
rule. See the grain implementation handoff below.

Actual sales replenish the relevant sellers through the existing ownership, broker and
transport chain; a broker sale is not a second payment to its supplier. Consume goods once
at their actual location and record unmet needs when goods or funds are insufficient.
Need alone cannot create stock, a funded order or sale proceeds. Exact item definitions,
remaining item authoring values, within-basket allocation, optional demand and
additional savings targets still need decisions. The condition response is selected below.

Each interval generates fresh consumption needs from its population and elapsed simulation
time. Record missed consumption as unmet needs and retain shortage measures for future
consequence rules. Missed consumption adds no automatic catch-up purchases to later
intervals. For example, missing food during one interval does not automatically double the
next interval's food target. Starting a fresh target must not erase the recorded shortage
or count missing goods as consumed. Shortages lower tracked population health and happiness,
and both lower production work rate under the selected response below. Fresh buying targets
do not reset these values.

### Basic medical supplies commodity (selected; implementation handoff)

Add a distinct tradable Basic Medical Supplies commodity representing routine medical
consumables, with `Kilogram` units. It satisfies the first basket's basic-medical-supplies
need at 5 kg per 1,000 people per simulation day. This is one aggregate need and commodity,
without individual patient, illness, prescription or treatment simulation. Its shortage
uses the existing equally weighted essential-need severity and condition rules; adding
the item does not add a separate health bonus or reset condition.

Proposed new authored ID is `ItemBasicMedicalSupplies`; this is not an implemented
definition. Read-only inspection found no item with that ID. Preserve existing
`ItemAntibiotics`, `ItemAnesthetics` and `ItemAnalgesics`, including their authored names,
units, prices and uses. They remain separate tradable specialised medicines and do not
automatically satisfy or convert into the new basic-supplies item. No production recipe
or specialised-medicine demand is selected by this decision.

The user or a user-selected non-OpenAI implementer owns this content addition. A bounded
authoring path is `data/trade-items/misc.yaml`, which already contains medicine items and
is included by `data/build.sh`, plus explicit selection in `data/game-mode/full.yaml` and
the separate governance fixture. `CategoryMedicine` is an existing category available
for the implementer to assess. Preserve the original eight-good trading fixture. Use
ordinary Jettison definition/registry/archive and market/inventory/cargo paths; no new
generic SDK gap is demonstrated. Do not edit generated `data/definitions` records as
authored source. Availability/age, category choice, base price, density, units per cubic
metre and packing factor remain unselected; daily consumption is selected as 5 kg per
1,000 people per simulation day.

Required implementer evidence: one distinct basic-supplies ID with valid category/age/unit
references and finite physical/price fields; unchanged specialised medicine definitions;
full-mode and directory/pack visibility; separate stock, orders, trades and physical cargo
capacity. Verify 5 kg/day for 1,000 people and 125 kg/day for 25,000 people, including
fractional 60-second requirements and daily totals. At this rate, actual accessible supplies
must be consumed once, within household funds, without specialised stock silently substituting or surplus
food/water compensating for missing supplies. Preserve shortages, fresh needs, no automatic
catch-up demand and save/replay accounting. Content/market suites and required gates belong
to the implementer, with each machine's concurrency (D-051), local clang-tidy opt-in. No commodity
YAML, code, fixture, asset, build script or test was changed in this planning work.

### Staple grain commodities (selected; implementation handoff)

Add rice, wheat, oats and barley as distinct tradable food commodities. These are harvested
food staples, separate from the planting-seed items already present in
`data/trade-items/seeds.yaml`. Preserve those existing seed IDs, uses, units and values.
Do not rename seed commodities or reuse their prices as food-grain prices.

| New food commodity | Proposed new ID | Selected quantity unit | Existing seed ID to preserve |
| --- | --- | --- | --- |
| Rice | `ItemRice` | `Kilogram` | `ItemRiceSeeds` |
| Wheat | `ItemWheat` | `Kilogram` | `ItemWheatSeeds` |
| Oats | `ItemOats` | `Kilogram` | `ItemOatSeeds` |
| Barley | `ItemBarley` | `Kilogram` | `ItemBarleySeeds` |

These IDs are proposed authoring names, not implemented definitions. Repository inspection
found the seed definitions but no food items with these four proposed IDs. Use the existing
`CategoryConsumables` and ordinary item/market/inventory/transport paths; no additional
generic SDK capability is demonstrated for four commodities. Seed-to-grain production,
processing recipes and regional preferences are separate design
work, not automatic consequences of adding a tradable item.

The user or a user-selected non-OpenAI implementer owns the content addition. Suggested
authoring boundary is a new `data/trade-items/grains.yaml`, included in `data/build.sh`'s
definition list and selected in `data/game-mode/full.yaml`. Use the existing YAML/archive/
registry and packaging mechanisms. Preserve the tiny eight-good trading fixture; select
the new essentials explicitly in the separate governance fixture rather than silently
changing unrelated mode selections. Do not edit generated `data/definitions` records as
the source of truth.

The implementer must specify each grain's food form, availability/age, base price, density,
units per cubic metre and packing factor. The food-grain unit is selected as `Kilogram`,
with equal 1 kg-to-1 kg staple-food substitution; existing seed units remain unchanged.
Preserve existing authored IDs/keys/units, price-only Decimal, floating quantities and
whole-credit ledgers. The remaining food-form, availability, price and physical authoring
values are unselected; this handoff does not invent prices or physical measurements.
Use the selected food demand of 500 kg per 1,000 people per simulation day. The selected
new regional model counts people, with consumption rates per 1,000 people per simulation
day. Existing `Population` and `Civilisation` still
use abstract world units; any supported migration needs an explicit conversion to people.

Required implementer evidence: four distinct food IDs and unchanged seeds, valid category/
age/unit references and finite physical values, full-mode visibility through directory/pack
loads, independent tradable stock/orders/settlement and physical cargo capacity. At the
selected daily food rate, one qualifying grain or a measured mixture can satisfy
the single food need without requiring the other three. Check 100 kg from one grain and
60 kg rice plus 40 kg barley, preserving separate stocks/prices/storage. Debit actual grains
once without generating a food item or converting inventory; no relief of water/medicine
shortages from surplus food. Run relevant content/market suites and repository gates with two
local compile jobs, leaving local clang-tidy opt-in. No content, code, build script, fixture
or test was changed and no implementation/build/gameplay validation ran in this planning work.

Declare initial household and business balances as world-generation funds with explicit
provenance. GDP is a measure of economic activity, rather than an automatic award of credits
or development currency. Public development spends actual treasury funds; the selected first
revenue source is wage-tax withholding below. This supersedes the GDP-to-spendable-currency
suggestion in `idea.md`; GDP measurement details remain open. The initial monetary rule below
defers any later continuing currency source until circulation has been measured.

Performance is a design requirement. Batch regular wage transfers and household purchases
every 60 seconds of simulation time within the authoritative fixed-step world, as an initial
balance-testing setting. Production continues on fixed steps. Batch completion and
cancellation settle remaining wages when processed, independently of the regular payment
schedule. Shutdown pauses simulation and the economic cadence; disconnected clients do not
stop otherwise eligible world progress. Persist the schedule's position with authoritative
state so recovery cannot duplicate updates or reset the wait for the next one. This is a
simulation-time interval; its real elapsed duration depends on simulation pacing.

Measure cash flow and workload before tuning the interval. Household work
must not iterate over individual inhabitants or expand purchases into per-person orders.
Expected work depends on regions, participating employers/facilities, commodity entries and
actual transactions, rather than the numeric population represented by a pool. Preserve
native stock/fund checks, deterministic ordering and recovery across batched settlements.
This is an intended workload shape, not a measured complexity or throughput guarantee.

Payroll funding, progress-based earnings, whole-credit rounding and interruption/cancellation
settlement follow the selected batch rule below. Numeric wage amounts, employment, consumer
commodity definitions, within-basket allocation,
optional demand, additional savings
targets, remaining movement/market integration ordering, GDP accounting and transaction bounds
remain design work.
The health/happiness response and work-rate multiplier are selected below, requiring balance
validation. The selected economic sequence is production using prior-step condition and
completion settlement, then due regular payroll/tax, household purchases/consumption and
condition update; the new multiplier applies from the next fixed step. Full movement/market
integration still needs a resolved scheduler graph. Pooling does not establish
individual wealth distribution.
No economic or performance result is claimed: finite wage-paying funds
and consumer spending alone do not demonstrate sustainable circulation under accumulation.

Implementer evidence must cover separate accounts, insufficient payroll/consumer funds,
essential-first purchases with scarce stock, physical delivery before consumption and
exactly-once wages, purchases and consumption through retries/recovery. Verify that scheduled
payroll can fund purchases in the same update, optional purchases stay blocked when essential
needs are unmet despite optional stock/remaining money, and funded optional purchases become
eligible after essentials are met. Preserve unspent credits across updates and recovery,
and verify basket scaling with population and elapsed simulation time. At a fixed population,
1,440 normal 60-second requirements must total the authored daily requirement within the
declared numeric tolerance; retain fractional quantities and generate no population demand
at zero population. At the selected food rate, verify 500 kg/day for 1,000 people and
12,500 kg/day for 25,000 people, using one grain or mixtures without multiplying demand
by the number of alternatives. Verify water demand of 3,000 litres/day for 1,000 people
and 75,000 litres/day for 25,000 people, including fractional 60-second requirements and
volume-preserving conversion to inventory units. Agricultural/industrial water use must
not satisfy household drinking-water consumption or debit the same stock twice.
Verify basic-supplies demand of 5 kg/day for 1,000 people and 125 kg/day for 25,000 people,
with fractional interval quantities and no specialised-medicine substitution. Across the
complete basket, shortages remain need-weighted rather than raw-quantity-weighted.
Supported legacy migration must apply its declared conversion once,
preserving totals across regional splits. Check that a missed
or partially fulfilled interval records the actual shortage, later intervals use fresh
needs without automatic catch-up demand, and shortage measures survive retries/recovery
without duplicate entries. Returning supply/funds must enable purchases against current
needs without erasing earlier shortages or consuming nonexistent goods. Compare different
population magnitudes at fixed region/employer/commodity and transaction counts, then scale
regions separately from physical planets; report timings, allocations and economic update
work. Prove no
per-citizen entity/order expansion and preserve zero-client progress, shutdown pause and
replay. Verify regular payments/purchases at the 60-second simulation-time boundaries under
the declared fixed-step configuration, immediate settlement when completion/cancellation is
processed, and schedule continuity through save/load and crash/retry boundaries. Measure cash
flow and update workload before changing the initial interval. These checks belong to the
implementer and have not run.

### Population health, happiness and production (selected)

Track two distinct aggregate values for each inhabited region's population: health and
happiness. Both are authoritative simulation state, updated by native Manifest systems,
saved/hashed and restored with the world. They are not individual citizen agents or UI-only
labels. Shortages lower those values, and reduced health and reduced happiness both
contribute to lower production output. Starting a fresh needs interval preserves the
population's condition and recorded shortages. Regions on the same planet keep distinct
values and production effects; any planet-wide condition summary is derived presentation.

Health and happiness each range from 0 to 100, allowing fractional changes: 100 represents
ideal condition and 0 the lowest modelled condition. Both start at 100 in the first
balance-testing scenario. This is explicit scenario initial state, not a reset on player
arrival, acquisition or governor change. Joining or acquiring an existing region retains
its current values; restoring a save retains the saved state. Keep continuous values under
Manifest's float/double numeric policy, independently of whole-credit account balances.

Update both condition values after household purchases on every 60-simulation-second
economic update. Unmet essential needs lower both values, with larger shortages causing
greater losses. Meeting all essential needs allows gradual recovery; restoring deliveries
does not instantly restore full condition. Happiness declines and recovers faster than
health, with both values kept within 0–100. Apply one condition update for the interval's
settled needs, preserving state rather than resetting it at the next interval. This remains
simulation time: disconnection does not pause updates and server shutdown does.

Selected initial balance-testing rates, in condition points per simulation hour:

| Essential supply | Health | Happiness |
| --- | --- | --- |
| All essentials unmet | −6 | −12 |
| All essentials met | +3 | +6 |

Partial shortages scale losses proportionally to their measured severity. For a normalised
severity `s` in `(0, 1]` and elapsed simulation hours `t`, health loses `6 × s × t` points
and happiness loses `12 × s × t` points. At full essentials, health instead recovers `3 × t`
and happiness `6 × t`, capped at 100. Bound losses at 0. This notation is planning arithmetic,
not a new save/API contract or implemented formula. These rates use simulation time,
separately from real server-running-time frontier concessions.

Calculate each required essential need's percentage unmet for the current interval, then
combine those percentages using authored positive importance weights. Initially give every
required essential need the same weight (1). The first basket counts food, water and medical
supplies once each; the four food grains are alternatives within the food need. Importance
is independent of commodity price and physical quantity. For positive required quantity
`q_i`, actually satisfied quantity `c_i` and importance weight `w_i`, fulfilment is
`f_i = min(1, c_i / q_i)` and
severity is `s = sum(w_i × (1 − f_i)) / sum(w_i)`. Only quantities actually satisfying that
region's current essential needs count; pending deliveries and inaccessible stock do not.
For staple food, sum actual consumed kilograms across the four qualifying grains before
computing fulfilment against the required kilograms, under the selected equal substitution.
This sum is fulfilment accounting, not a new stock item. Optional goods do not enter this calculation.
Exclude needs with no current requirement; with no required essentials there is no essential
shortage. Recovery still requires all
current essential needs met, rather than a rounded display percentage indicating completion.

Cap each need's fulfilment at 100% before combining, so surplus cannot compensate
for a different missing essential. Qualifying grains can substitute within the food need
at 1 kg-to-1 kg. With two equally weighted essentials, one fully supplied
and the other half supplied gives severity 0.25: health loses 1.5 and happiness 3 points
per simulation hour, or 0.025 and 0.05 per normal economic interval. Recovery begins only
when all essentials are met. That two-need example illustrates the formula; the selected first
basket has three needs. Full staple-food and water coverage with no medical supplies gives
severity one third and hourly losses of 2 health/4 happiness. Remaining commodity authoring
values are separate choices; all three initial consumption rates are selected above.

At the selected 60-second cadence, complete shortage loses 0.1 health and 0.2 happiness per
interval, while full essentials recover 0.05 and 0.1 respectively. One simulation hour of
complete shortage takes 100/100 to 94/88; a following hour with full essentials yields 97/94.
Recovery is half the maximum decline rate. The values are provisional balance settings,
not evidence of a viable economy or acceptable shortage/production feedback.

The selected production mechanism reduces work rate as health and happiness fall. Completed
batches retain their declared material requirements, output quantities and total gross wage
bill. Wage accrual remains proportional to completed work, so slower production also slows
wage payments. A condition change affects subsequent production steps, including remaining
work in an active batch; it cannot reverse completed progress or revise already earned/paid
wages. Interruption, completion and cancellation retain their selected payroll rules.
At half work rate throughout, an illustrative normally ten-minute batch takes twenty minutes
for the same completed goods and total wages. This example does not select recipe duration
or wage amount.

The selected initial balance formula multiplies otherwise eligible production work rate by
`m = (0.5 + health / 200) × (0.5 + happiness / 200)`. Each condition contributes a factor
between 0.5 and 1, so valid 0–100 values yield a multiplier between 0.25 and 1:

| Health | Happiness | Work-rate multiplier |
| --- | --- | --- |
| 100 | 100 | 100% |
| 50 | 100 | 75% |
| 50 | 50 | 56.25% |
| 0 | 0 | 25% |

This limits the slowdown caused by condition to 75%; it is not a guarantee of positive
production. Missing materials, insufficient funds or an inactive facility can still stop
production entirely. Preserve input/funding/operation checks and other baseline constraints.
The multiplier is calculated from current regional condition; it does not create another
independent population-condition value or change recipe yields/payroll. These are initial
balance values, not validated rates or a demonstrated recovery guarantee.

The selected economic ordering is:

1. Advance production every fixed step using condition from the previous completed step;
   settle completed batches under their existing goods/payroll rules.
2. When the 60-simulation-second economic update is due, pay accrued regular payroll and
   withhold tax. Subtract earlier completion/cancellation payments as required; do not pay
   the same earned wages twice.
3. Households purchase and consume essentials, then permitted optional goods, within their
   actual funds and physically available supplies.
4. Update health/happiness from that interval's essential shortages or recovery. The new
   multiplier applies from the next fixed step, including remaining work in active batches.

Production continues on intervening fixed steps; regular payroll, household purchasing
and condition retain their selected 60-second cadence. Completion/cancellation settlement
still occurs when processed. Newly detected shortages cannot change completed work or
already earned wages. Persist cadence position and settled interval state through replay
and recovery. This selects the production/economic sequence, not the complete movement,
docking, market-order or pricing scheduler graph; see the stage 4 plan. Preserve physical
recipe/input/output accounting and progress-based payroll. Population-size changes remain separate.

Existing evidence: `Population` in `source/game/ecs/include/manifest/ecs/Components.h` stores
world units and a ceiling; `Civilisation` in `source/game/sim/include/manifest/sim/Simulation.h`
records unmet demand and shortage ticks. Neither defines health or happiness. The body-effect
stats in `source/game/types/include/manifest/types/EffectTypes.h` include `MedicineCoverage`,
which is service coverage rather than population health. No implemented health/happiness
state, shortage response or production penalty is claimed.

Manifest owns these values, needs/condition rules and production effects. At SDK pin
`f850920`, public `SimWorld::RegisterComponent`, `RegisterSystem`, `WorldService`, `Save`,
`Load` and `StateHash` provide generic state/system/persistence facilities; use the existing
Jettison definition/archive mechanisms for authored policy. No missing generic SDK capability
for these values has been demonstrated. Provision any additional reusable gap only after
demonstrating it; do not add population-health rules to the engine sample economy.

Required implementer evidence: shortages change the correct region's health/happiness,
without directly changing sibling regions on the same planet;
demonstrate each value's independent production effect and the combined rule at declared
test points away from any penalty floor/cap. Check the declared 100%, 75%, 56.25% and 25%
multiplier examples, symmetry on swapping health/happiness and independent monotonic effects
throughout valid condition ranges. Even at the 25% condition floor, missing materials/funds
or an inactive facility must still block production. Retain values across fresh needs intervals,
zero-client progress, save/load, replay and crash/retry boundaries without duplicate penalties. Check declared
bounds and fractional changes, both initial test values at 100, and no reset on joining,
acquisition or governor change. Check physical material conservation, progress-based wages and
long-running shortage/production feedback. Compare small and large essential shortages:
both lower condition, with greater loss for greater severity away from the bounds. With all
essential needs met, both recover gradually; happiness responds faster than health in both
directions away from the bounds. Neither can exceed 100 or fall below 0. Apply the response
after household purchases once per due interval, including across retry/recovery, and prove
that resumed supply does not instantly reset either value. Include the declared 100/100 →
94/88 → 97/94 hourly example, fractional per-interval changes, proportional partial-shortage
losses and both bounds. Check the two-good 25%-severity example, a surplus alongside a
missing essential, price/unit changes that cannot alter importance, optional/zero-required
goods excluded, and recovery only after all current essentials are met. Reject invalid
nonpositive/nonfinite importance weights. Faster test stepping must use elapsed simulation time rather than
host wall time for condition changes. With a controlled half work rate, a batch must take
twice the baseline duration for identical completed material/output/payroll totals. Change
condition mid-batch and prove that only subsequent work and its proportional wage accrual
change; retain earlier progress, earned wages and settlement, including interruption/cancellation
and recovery. At an economic boundary, prove production uses prior-step condition, due
regular payroll/tax precedes household purchases and condition changes apply only from the
next fixed step. Include a batch completing on that boundary without double payroll and a
restored interval without duplicate consumption/condition changes. No implementation or
gameplay/performance checks have run.

### Production-batch payroll funding (selected)

Each production batch declares its gross wage cost. Before it starts, the employer reserves
that amount from available funds alongside the required inputs. Insufficient funds block
new batches. Existing stock remains sellable so actual sales can restore working capital.
The same reserved payroll cannot fund another batch, project or purchase.

Each production recipe specifies its gross payroll cost as authored balance data alongside
its ingredients. Show the wage bill before starting and keep that quoted amount fixed for
the batch. Market-price changes affect the employer's margin without changing the batch's
wage bill. Balance adjustments apply to future batches. Preserve the quoted gross cost with
the batch's reservation, progress and payment state through recovery.

Choose initial wage values through balance work against input costs, handling and other
fees, and expected sale proceeds. The explanatory 10 kg bronze example used 60 credits of
inputs, 20 gross wages, 5 fees and 100 sale proceeds, leaving 15 for the employer; all credit
figures were illustrative. No actual bronze wage, input cost, fee, selling price or
profitability result was selected or demonstrated.

This is aggregate batch accounting, with no individual worker entities or payroll
transactions. The reserved amount is gross payroll, including the eventual split into
household receipts and the selected wage-tax withholding. Reservation alone is not payment
and generates neither household income nor tax receipts.

Wages accrue in proportion to completed production work and are paid in batches on economic
updates from the reserved gross payroll, applying the selected 10% withholding at payment.
Facility interruptions pause both work and wage accrual. Preserve completed progress and
the remaining payroll reservation; resuming continues the same batch without paying twice.
Shutdown pauses progress under the existing world contract; disconnecting does not stop
otherwise eligible work.

Cancellation settles earned but unpaid wages before releasing the unearned payroll
reservation into the employer's available funds. Completion settles the remaining wages.
Previously paid wages and taxes stay settled. For a batch with 100 credits of gross payroll,
cancellation at 40% completed work means 40 credits earned in total. With no interleaved
payroll for that employer/region, this splits into 36 to households and 4 to the regional treasury,
including amounts already paid. Settle only the unpaid part and release the unearned 60
credits.

During production, round the batch's cumulative earned gross wages down to whole credits
and subtract gross wages already paid to determine the next payment. Retain fractional work
progress for later payments in the same batch rather than rounding each update's increment
independently. Completion pays the full declared gross payroll less earlier payments.
Cancellation rounds cumulative earned gross upward to whole credits once, leaving exact
whole-credit earnings unchanged and capping the result at the originally reserved gross
payroll, then subtracts earlier payments. Release the remaining reservation only after this
final settlement. Zero completed work earns zero wages. Apply tax withholding to gross
credits actually paid under these rounding rules.

For example, a 100-credit payroll batch cancelled at 40.5% completed work owes 41 gross
credits in total. If 40 have already been paid, pay one more and release the remaining 59.
The cancellation rule can cost the employer less than one additional credit above exact
proportional earnings, funded from the existing reservation. Work progress and tax remainders
are accounting state; spendable balances remain whole credits.

Numeric wage amounts still need decisions. Regular payments use the selected 60-second
simulation-time economic cadence; completion/cancellation settles when processed. Tax
remainders follow the selected cumulative rule below.
Material consumption, unfinished goods and salvage on cancellation require separate
production rules; this payroll decision does not select them.

Required implementer evidence includes rejecting a batch with insufficient/unavailable
funds, preventing double commitment, retaining sellable old stock, and preserving payroll/
input reservations and progress through retries/recovery. Verify that the displayed recipe
wage bill matches the reserved gross amount and stays fixed through market-price changes,
payments, interruption and recovery; later balance changes affect future batches.
Check interrupted/resumed work,
zero-progress and partial-progress cancellation, cancellation after earlier payments, and
completion after earlier payments. Earned wages must be paid once, withholding must apply
only to actual payments, and only unearned payroll may be released. Recovery around accrual,
payment and release must preserve matching balances and prevent paying or releasing the same
reserved credit twice. Small-payment and fractional-progress checks must follow the selected
rules above: repeated sub-credit progress must accumulate, intermediate payments must not
exceed rounded cumulative earnings, and final settlement must subtract earlier payments.
Check zero progress, exact and fractional whole-credit boundaries, the 40.5% example,
completion at the full declared payroll and cancellation near completion without exceeding
the original reservation. Preserve progress, cumulative payments and terminal settlement
through retries/recovery. No such checks have run.

### Wage-tax withholding and public revenue (selected)

Withhold 10% of wages actually paid and credit the applicable regional public treasury at payroll
settlement. This is an initial balance-testing rate. The employer pays the gross wage amount;
split that payment between the household pool and treasury rather than adding tax to the
employer's gross cost. A 100-credit payment debits the employer by 100, credits households by
90 and credits the treasury by 10. Unpaid or merely scheduled wages generate no tax receipts.

Keep gross wages, household net receipts and treasury withholding in the same authoritative
settlement. A retry or restart cannot withhold twice or credit one recipient without the
matching employer debit. The household and tax shares must sum to the actual gross payment.
Batch withholding with existing payroll work, making one aggregate treasury transfer when
withholding is nonzero, without per-citizen accounts or transactions.

Carry the tax remainder across payments for each employer and region, shared by that
employer's production batches/facilities in that region. Every ten cumulative gross credits
actually paid produces one whole treasury credit. Start each new employer/region remainder
at zero. Add each gross payment to the carried remainder, withhold the number of complete
groups of ten, and carry the remaining zero to nine toward later payments. Credit households
with that payment's gross less its withholding. The remainder is accounting state, not a
fractional-credit account balance or additional currency.

For example, starting from zero, the first nine separate one-credit gross payments each
credit households by one and the treasury by zero. The tenth credits the treasury by one
and households by zero, giving nine household credits and one treasury credit in total.
Batch completion or cancellation does not reset the employer/region remainder. Splitting
the same paid gross across batches within that jurisdiction must not reduce total tax.
Keep each employer/region's
remainder separate; advance it in authoritative payment order and preserve it with the
matching settlements through saves, retries and recovery. Whole-credit rounding of wages
earned from fractional production progress follows the selected production-batch rule above.

Each payment resolves one applicable regional household pool and treasury. The region is
an explicit payroll/settlement reference, not inferred from the station or employer's owner.
Cross-region employment and payroll allocation need explicit rules before those cases are
enabled; do not duplicate a payment across regions or reset carry by changing a facility.

This is the first selected public-revenue rule. The local treasury can spend received credits
through its authorised public budgets and procurement. Taxation transfers existing funds;
it does not create currency or establish sustainable wage funding or development income.
Player rate-setting powers and other tax bases remain open; the initial monetary rule is below.

Implementer evidence includes the 100/90/10 split, no tax on unpaid payroll, insufficient or
reserved employer funds, ten one-credit payments matching one ten-credit payment, and
remainder persistence across batch completion, cancellation and restart. Check isolation
between employers and regions, including sibling regions on one planet, correct treasury
routing, deterministic ordering and
exactly-once balance/remainder updates through retries/recovery. Measure the extra work
within batched payroll. No implementation or accounting/performance check ran.

### Initial monetary rule and circulation evidence (selected)

The first economy scenario uses only declared starting credit balances. There is no periodic
currency creation, automatic bailout or hidden refill of household, employer, broker or public
accounts. Wages, taxes, purchases and fees transfer existing credits between the appropriate
accounts. GDP, need, quoted demand and elapsed time do not automatically award spendable money.

Cash shortages remain visible and constrain actual spending. Measure whether accumulation
drains household, employer or government spending capacity before selecting any continuing
currency source. This is an initial scenario rule, not evidence that a fixed money supply
supports the intended long-running game. Future currency issuance or any funding supplied
when players join or systems are added requires an explicit source/rule; this decision does
not define those later lifecycle events.

Implementer evidence must reconcile declared starting credits against all current balances,
counting reserved/escrowed credits exactly once. Verify matching debits and credits for wages,
taxes, purchases and fees, with correct handling when accounts share an owner. Track cash
distribution, payroll shortfalls, unfunded consumer needs, broker liquidity, public-budget
pauses and trade turnover over extended zero-client runs. Replay/recovery and retries must
preserve balances without refills. Use those results to assess circulation and any proposed
later currency source. No such simulation or monetary-policy result has been produced.

## Capability map, delivery and evidence

| Requirement | Existing evidence | Required work/owner |
| --- | --- | --- |
| Instant credit transfers | Manifest integer `Treasury`; existing market settlement/checks; Jettison fixed-step commands and saved world services | Manifest authorised general transfer and atomic ledgers/receipts with reservation/retry checks. |
| Physical warehouse stock | `Stockpile` plus planned owner/venue inventory; Jettison spatial/docking state | Manifest custody/location/transport jobs, warehouse capacity and exactly-once handoffs; generic SDK seams only where demonstrated. |
| Principal brokers | Decimal quotes, reservations, matching and fees in `MarketplaceService` | Manifest finite broker accounts/policy and location-aware adapter; existing owner fields are civilisation-based development APIs. |
| Fleet-observed market intelligence | Raw `Quotes`/`QuoteFor` and global `PlanTrade` have no actor-observation gate; pinned docking and saved/hashed world state provide generic building blocks | Manifest observation ownership, live/stale reports, one-time neighbour-market seeding and filtered query/AI/script facts; see the market-information plan for evidence and checks. |
| Handling income | Existing market fee accounting, but no demonstrated warehouse-service chain | Manifest service/beneficiary/charge definitions and receipts, integrated with bookings. |
| Production/material-gated tech | Existing production/demand maps, progression graph and effect domains | Manifest facility/resource/recipe/project state and dependency validation using SDK definition/archive mechanisms. |
| Regional population, accounts and production loss | Manifest's current `Civilisation` combines a planet economy's population/treasury/production; population/shortage and body-effect domains have no health/happiness values. Pinned `SimWorld` provides component/system registration and saved/hashed world services | Manifest separates planet/region/organisation, aggregate regional households/condition, payroll/tax, local development and both production effects. Demonstrate sibling-region isolation, conserved totals, material/payroll correctness and replay/recovery; no additional generic SDK gap is demonstrated. |
| Core/frontier geography | Manifest seeded development generation; Jettison explicit galaxy/system definitions | Manifest spatial economic generation and initial-depletion profiles; reusable incremental topology/persistence gaps stay Jettison work. |
| Real-time concessions | Jettison ordered commands, world services/save/hash and journal contracts in the host plans | Manifest resource-right policy and host time-to-event bridge; provision reusable timer/operation seams only if a gap is demonstrated. No wall-clock reads inside deterministic systems. |

Source anchors: `source/game/sim/{include/manifest/sim/Simulation.h,src/Simulation.cpp}`,
`source/game/ecs/include/manifest/ecs/Components.h`, `source/game/market/` and
`source/game/progression/include/manifest/progression/ProgressionManager.h`. The development
`PlanTrade` estimates global surplus/deficit and freight; `AdvanceTick` directly moves stock.
Neither is the physical broker/warehouse loop required here. Reuse the actual rules with
the hosted world; do not run this separate economy alongside the market.

Suggested slices: owner/location/credit accounting; one extraction-to-warehouse job; a finite
broker and origin/destination handling; one complete idle trade; one local-plus-imported-input
tech project with an actual facility prerequisite; static core/frontier distribution; then
resource rights, colonies and later dynamic expansion. Integrate public supply tenders at the
actual project delivery point. Commit reusable Jettison provision before updating the pin.

Acceptance: a mine cannot overproduce; unshipped ore cannot sell at another location; brokers
cannot sell/buy without stock/funds; origin and destination handling charges reach the correct
accounts once; the same stored goods cannot be reserved twice; credits cross distance without
moving goods. A planet advances only after real local/imported inputs and its facility gates
are satisfied. Shipping or facility interruption pauses appropriately without duplicating
material consumption, payment or effects. Core/frontier initial reserves/tech distributions
replay from the same seed, and saves retain depleted resources and in-transit cargo.

With an injected host clock, prove that fast simulation stepping does not expire a running-time
grant, shutdown consumes no entitlement, committed elapsed time survives restart, and the
recorded expiry replays identically. Rival exploitation of the named region's covered resources
is refused during exclusivity and permitted afterwards despite a puppet government. A sibling
region's eligibility/timer remains independent; no government control transfers on expiry.
Prove independent initial construction opportunities in two eligible regions on one planet,
a funded race within one region, rejection of unsolicited builds in player-governed regions,
and competing frontier ports at concession expiry despite a puppet government. Demonstrate a
shared port delivering physically to two named regions without merging funds/inventories,
changing rights or consuming neighbours' construction opportunities. Honour bookings across
owner access changes and retain station-specific visibility. Reject onboarding without a
starter-compatible operational public station and physical regional delivery connection;
require available working life support and establish its connection at spawn, pausing both
onboard depletion streams. Missing or exhausted service capacity rejects allocation;
retry/recovery cannot duplicate spawn/resupply or reset reserves. Verify initial starter
stock of 30 kg oxygen and one fresh installed 30-occupant-day cartridge, granted once.
Operating reserves use dedicated service space outside the starter's 20 m³ hold; supplies
for trade or loose spares use cargo volume. Check explicit locations and conserved stock
through transfers and enforce the starter's selected 30 kg operating oxygen maximum.
Compartment dimensions, tank physical volume, station provisioning and later outages remain open.
Later corporate restrictions cannot revoke basic public docking/trading/delivery access.
Preserve charges, finite capacity/goods/funds and private-port policies. Detailed transport,
starter fees/capacity/funds, onboarding/claim compatibility and construction/control changes
need separate rules.
Rights/owner changes and retries cannot reset the grant. Test crashes with paired production/
time checkpoints and report the bounded elapsed-time loss instead of claiming perfect uptime.

Implementer checks include existing suites, long-run finite-funds/inventory scenarios,
directory/pack equivalence, cross-build replay and crash/retry boundaries, using presets and
required gates with each machine's concurrency (D-051), local clang-tidy opt-in for
cross-repository work.
Only source inspection and documentation checks have occurred; no economic/scale result is
claimed. The first material/facility example, aggregate household foundation, pre-funded
production payroll with authored recipe wage bills, progress-based earnings and
interruption/cancellation settlement, whole-credit wage rounding, initial wage-tax withholding
with employer/region remainder carry, the 60-second simulation-time cadence for regular
payroll/household purchasing, the essentials-first basket/optional-purchase gate, unspent
household credit carry, scheduled payroll before household purchases, fresh interval needs
with recorded shortages and no automatic catch-up purchases, tracked population health and
happiness with the selected decline/recovery, severity and production work-rate multiplier,
economic ordering, the grain/water/medicine basket and first-scenario monetary rule are selected.
Next choose grain/item authoring values, numeric payroll amounts,
purchase allocation, full movement/market integration ordering
and the remaining production
material/cancellation, construction funding, deadline and salvage terms. Validate
the selected recipe, initial cadence and provisional 72-hour grant against actual construction
costs, extraction capacity, production throughput, prices and server pacing.
