# Physical economy, brokers and planetary development

Product planning, 2026-11-15. Extends the [roadmap](roadmap.md),
[planetary/corporate goals](planetary-development-plan.md) and
[supply/booking contract handoff](procurement-and-station-contracts-plan.md).
The decisions below are requirements, not implemented capabilities. The first tech/production
recipe is selected below; schemas, other recipes, rates, fees and prices remain design work.
No code, content, SDK pin or
development environment was changed by this assessment; no builds or game simulations ran.

## Confirmed rules

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
- Consumer demand uses one aggregate household pool per inhabited planet. Employers pay
  wages from available funds; the pool buys physically delivered consumer goods within its
  budget, prioritising essentials. Initial household/business funds are explicit generation
  state. GDP measures activity rather than automatically generating spendable credits.
  Batch wage payments and purchases on an economic schedule; do not simulate individual
  citizens or families. Exact cadence, wage/demand rules and performance limits remain open.
- Withhold 10% of wages actually paid for the local planetary treasury, as an initial
  balance-testing rate. Split the employer's gross payment between households and the
  treasury at payroll settlement: 100 credits pays 90 to households and 10 to government.
  Whole-credit rounding and later rate-setting powers remain unresolved.
- The first economy scenario uses declared starting credit balances without periodic currency
  creation or automatic account replenishment. Wages, taxes, purchases and fees transfer
  existing credits. Expose cash shortages and measure circulation before choosing any
  continuing currency source for later scenarios.
- Each production batch declares a gross wage cost. Reserve that payroll alongside required
  inputs before starting; insufficient funds block new batches while existing stock remains
  sellable. Reserved payroll cannot fund another project or purchase. Wage amounts and
  payment timing remain to be selected.
- Planetary tech advancement requires exploited resources, purchased/imported resources the
  planet cannot gather, and dependencies on actual production facilities.
  The first selected example is Bronze metallurgy: an operational local workshop and basic
  smelting knowledge, 90 kg copper plus 10 kg tin for the project, followed by repeatable
  production batches of 9 kg copper plus 1 kg tin into 10 kg bronze. Quantities are initial
  test values; work duration, throughput and profitability remain to be balanced.
- The inner map starts with higher civilisation technology and fewer remaining natural
  resources due to historical exploitation. Outer worlds are generally less developed and
  resource-rich, encouraging a gold rush for control of remaining deposits and access.
- A corporation receives an initial exclusive exploitation/lockout period, followed by
  opening to rival exploitation at a disadvantage. Measure the period in wall-clock units,
  counting elapsed real time only while the server runs. Shutdown time does not count.
  Rival spaceports become permissible after the period; the one-port restriction is an
  initial protection rather than a permanent cap on such worlds. Frontier concessions reopen
  even under an incumbent's puppet government; starting home planets remain protected.
  Start the grant when the first eligible frontier spaceport becomes operational and its
  exploitation concession activates. Publish the duration before construction starts; use
  72 hours of actual server-running time as the initial balance-testing term. Idle facilities
  and disconnected owners do not pause it. Ownership transfers, rebuilding and government
  changes cannot restart or extend it. The concession does not itself transfer government
  control; extraction capacity, construction costs and server pacing still need validation.

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
this plan. Precise broker algorithms, margins, work bounds and information scope remain open.

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
subtract those spreads again. Estimate using affordable, physically available quantities and
cargo volume, not nominal production or unlimited quote depth.

Keep working capital for destination charges. An expected future sale is not spendable cash
or a guaranteed buyer. Quotes can expire or change while travelling unless a specific
accepted purchase/sale contract locks them. Jobs need cost/quantity limits, stale-price
handling and alternatives or a visible blocked state; none require a connected player.
Broker stock/fund exhaustion and unprofitable routes are legitimate outcomes.

## Planetary tech and production-facility dependencies

Tech advancement is a physical development project. Its definition identifies earlier tech
nodes, required built/operational production facilities, material quantities and work/progress
requirements. Inputs include resource exploitation and imports when the planet lacks the
resource/capability. Buying a tier for credits alone is insufficient for this new hosted
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
physical deposits or identity. Scope effects to the target planet unless explicitly shared.
The existing `ProgressionManager` node/resource-pool model does not establish timed research,
facility/material dependencies or per-planet job state; those are native Manifest work using
Jettison world registration, definitions, commands, archives and hashing.

Validate more than the tech graph: recipes, required facility levels, geographic resource
availability and transport access can form cross-system dependency cycles. A primitive
planet must not need a material produced only by its own locked future technology unless
an accessible external supplier can provide it. Seed appropriate AI industry/stock/access
for bootstrap as explicit initial conditions. Preserve existing authored IDs/YAML keys and
use Jettison schema/layering mechanisms for any planned definition extensions.

### First tech example: Bronze metallurgy (selected)

The user selected a per-planet Bronze metallurgy project for the separate governance scenario.
Require basic smelting knowledge (existing `SkillSmelting`) and an operational local
metalworking workshop. Seed the initial workshop explicitly so access to it does not require
the technology being researched; its construction recipe remains later work. Workshop/project
state is planned Manifest gameplay, not an already implemented facility or SDK capability.

The project requires 90 kg of `ItemCopper` and 10 kg of `ItemTin`, physically delivered into
public project inventory. The demonstration planet supplies copper locally and imports tin
through competitive procurement and shipping. Provide an explicit reachable external tin
supplier/bootstrap state; the project must not require locally unlocking the result before
it can import its input. Existing item IDs, age metadata, unit types and authored prices remain
stable; the hosted eligibility contract must support that prerequisite import.

After all inputs are secured and the workshop is operational, consume the project materials
once and advance a fixed-step work counter. Workshop interruption pauses progress; resuming
does not consume another set of materials. These project inputs are consumed by development,
not simultaneously credited as a saleable bronze batch. Completion unlocks the local
production capability once, without advancing every planet or regenerating the target world.

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
transport and buyers. Existing rules permit an expensive orbital port only where there is
none and another player does not own/control the planet. The initial resource right is
time-limited exclusivity, followed by rival entry rather than a permanent monopoly. The
selected grant trigger is the first eligible frontier spaceport becoming operational together
with activation of its exploitation concession. Publish its duration before construction
starts; the initial balance-testing term is 72 real server-running hours. A construction
reservation alone does not start the resource grant or confer government control.
The selected [construction-site rules](planetary-development-plan.md#company-built-stations-and-control-of-shipping)
give the first eligible funded project a reservation, requiring non-refundable mobilisation
and reserved construction funds. A fixed completion deadline cannot be renewed indefinitely
by token deliveries; abandonment/expiry reopens the site while preserving ownership of unspent
funds and unused goods. Salvage of completed work, exact amounts and the deadline clock/duration
remain unresolved. Construction priority does not itself grant resource or government rights.

Late joiners and AI retain the previously planned capacity considerations. Unassigned starter
planets with no ports are eligible for corporate construction under the confirmed rule;
reservation alone is not a hidden exemption. Gold-rush urgency and one-port scarcity amplify
this issue. Choose starting regions, existing ports/access, initial capital and any support
explicitly before claiming equal viability or long-running capacity.

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
the gate does not itself grant the 72-hour planetary exploitation term or government control.

## Real-time exclusive exploitation

The user selected the operational-port/concession trigger and provisional 72-hour term above.
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
no-existing-port construction condition for reopened concession worlds. The incumbent may
still deny new bookings at its own facility while rivals fund independent mining, warehouses
and transport. Rival entry offers opportunity, not free infrastructure or guaranteed reserves.
Frontier reopening remains valid under a puppet government. Starting home planets retain
separate protection. Store that world/right classification explicitly: changing a government,
owner or company name cannot relabel a frontier concession as a protected home. Define
onboarding/home-pool eligibility without retroactively cancelling existing frontier rights.

Preserve deterministic simulation by placing real-time observation in the server host.
Propose one saved cumulative server-running clock per world, advanced from a host monotonic
clock while that world runs. A grant stores its ID, planet/company, original duration, grant
value on that running clock, accumulated/remaining entitlement and lifecycle revision.
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

Use exactly one pooled household account per inhabited planet, independent of population
size. Track aggregate population, available credits, demand by commodity and needs fulfilment.
Individual citizens and families have no separate simulated accounts, agents or shopping
jobs. A population increase changes aggregate quantities, not the household entity count.
The public treasury, employers, brokers and household pool retain separate accounts.

Employers pay aggregate wages from their own available funds into the local household pool.
The pool purchases physically delivered consumer goods within its available budget,
prioritising essentials. Actual sales replenish the relevant sellers through the existing
ownership, broker and transport chain; a broker sale is not a second payment to its supplier.
Consume goods once at their actual location and record unmet needs when goods or funds are
insufficient. Need alone cannot create stock, a funded order or sale proceeds.

Declare initial household and business balances as world-generation funds with explicit
provenance. GDP is a measure of economic activity, rather than an automatic award of credits
or development currency. Public development spends actual treasury funds; the selected first
revenue source is wage-tax withholding below. This supersedes the GDP-to-spendable-currency
suggestion in `idea.md`; GDP measurement details remain open. The initial monetary rule below
defers any later continuing currency source until circulation has been measured.

Performance is a design requirement. Batch wage transfers and consumer purchases on a
declared economic update schedule within the authoritative fixed-step world. Household work
must not iterate over individual inhabitants or expand purchases into per-person orders.
Expected work depends on planets, participating employers/facilities, commodity entries and
actual transactions, rather than the numeric population represented by a pool. Preserve
native stock/fund checks, deterministic ordering and recovery across batched settlements.
This is an intended workload shape, not a measured complexity or throughput guarantee.

Payroll funding follows the selected pre-funded batch rule below. Exact update frequency,
wage amounts, employment, payment timing and interruption/cancellation terms, consumer
baskets/priorities, savings policy, GDP accounting, whole-credit rounding and transaction
bounds remain design work. Pooling does not establish individual
wealth distribution. No economic or performance result is claimed: finite wage-paying funds
and consumer spending alone do not demonstrate sustainable circulation under accumulation.

Implementer evidence must cover separate accounts, insufficient payroll/consumer funds,
essential-first purchases with scarce stock, physical delivery before consumption and
exactly-once wages, purchases and consumption through retries/recovery. Compare different
population magnitudes at fixed planet/employer/commodity and transaction counts, then scale
those counts separately; report timings, allocations and economic update work. Prove no
per-citizen entity/order expansion and preserve zero-client progress, shutdown pause and
replay. These checks belong to the implementer and have not run.

### Production-batch payroll funding (selected)

Each production batch declares its gross wage cost. Before it starts, the employer reserves
that amount from available funds alongside the required inputs. Insufficient funds block
new batches. Existing stock remains sellable so actual sales can restore working capital.
The same reserved payroll cannot fund another batch, project or purchase.

This is aggregate batch accounting, with no individual worker entities or payroll
transactions. The reserved amount is gross payroll, including the eventual split into
household receipts and the selected wage-tax withholding. Reservation alone is not payment
and generates neither household income nor tax receipts.

Wage amounts, when wages are earned/paid, and treatment of interruptions, cancellation and
unused payroll reservations still need decisions. Do not infer payment at batch start or
completion from the funding rule. Required implementer evidence includes rejecting a batch
with insufficient/unavailable funds, preventing double commitment, retaining sellable old
stock, and preserving the payroll/input reservation through retries/recovery. Settlement and
release checks must follow the eventual payment/cancellation rules. No such checks have run.

### Wage-tax withholding and public revenue (selected)

Withhold 10% of wages actually paid and credit the local planetary treasury at payroll
settlement. This is an initial balance-testing rate. The employer pays the gross wage amount;
split that payment between the household pool and treasury rather than adding tax to the
employer's gross cost. A 100-credit payment debits the employer by 100, credits households by
90 and credits the treasury by 10. Unpaid or merely scheduled wages generate no tax receipts.

Keep gross wages, household net receipts and treasury withholding in the same authoritative
settlement. A retry or restart cannot withhold twice or credit one recipient without the
matching employer debit. Whole-credit rounding and any carried remainder still need an
explicit rule; the household and tax shares must sum to the actual gross payment. Batch
withholding with existing payroll work, adding a treasury credit per payroll batch without
per-citizen accounts or transactions.

This is the first selected public-revenue rule. The local treasury can spend received credits
through its authorised public budgets and procurement. Taxation transfers existing funds;
it does not create currency or establish sustainable wage funding or development income.
Player rate-setting powers and other tax bases remain open; the initial monetary rule is below.

Implementer evidence includes the 100/90/10 split, no tax on unpaid payroll, insufficient or
reserved employer funds, small whole-credit payments under the eventual rounding rule,
correct planet/treasury routing and exactly-once settlement through retries/recovery. Measure
the extra work within batched payroll. No implementation or accounting/performance check ran.

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
| Handling income | Existing market fee accounting, but no demonstrated warehouse-service chain | Manifest service/beneficiary/charge definitions and receipts, integrated with bookings. |
| Production/material-gated tech | Existing production/demand maps, progression graph and effect domains | Manifest facility/resource/recipe/project state and dependency validation using SDK definition/archive mechanisms. |
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
recorded expiry replays identically. A rival port is refused during exclusivity, permitted
afterwards despite a puppet government, and still refused at a protected starting home.
Rights/owner changes and retries cannot reset the grant. Test crashes with paired production/
time checkpoints and report the bounded elapsed-time loss instead of claiming perfect uptime.

Implementer checks include existing suites, long-run finite-funds/inventory scenarios,
directory/pack equivalence, cross-build replay and crash/retry boundaries, using presets and
required gates with two local compile jobs and no local clang-tidy for cross-repository work.
Only source inspection and documentation checks have occurred; no economic/scale result is
claimed. The first material/facility example, aggregate household foundation, pre-funded
production payroll, initial wage-tax withholding and first-scenario monetary rule are
selected. Next choose payroll amounts, payment timing, interruption/cancellation and rounding
rules, then refine consumer policy and the remaining construction funding, deadline and
salvage terms. Validate the selected recipe and provisional 72-hour grant against actual
construction costs, extraction capacity, production throughput, prices and server pacing.
