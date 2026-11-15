# Physical economy, brokers and planetary development

Product planning, 2026-11-15. Extends the [roadmap](roadmap.md),
[planetary/corporate goals](planetary-development-plan.md) and
[supply/booking contract handoff](procurement-and-station-contracts-plan.md).
The decisions below are requirements, not implemented capabilities. Schemas, exact recipes,
rates, fees and prices remain implementation/design work. No code, content, SDK pin or
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
- Planetary tech advancement requires exploited resources, purchased/imported resources the
  planet cannot gather, and dependencies on actual production facilities.
- The inner map starts with higher civilisation technology and fewer remaining natural
  resources due to historical exploitation. Outer worlds are generally less developed and
  resource-rich, encouraging a gold rush for control of remaining deposits and access.
- A corporation receives an initial exclusive exploitation/lockout period, followed by
  opening to rival exploitation at a disadvantage. Measure the period in wall-clock units,
  counting elapsed real time only while the server runs. Shutdown time does not count.
  Rival spaceports become permissible after the period; the one-port restriction is an
  initial protection rather than a permanent cap on such worlds. Frontier concessions reopen
  even under an incumbent's puppet government; starting home planets remain protected.
  Duration and exact grant trigger still need choices.

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

Separate knowledge prerequisites from installed capacity. A proposed first rule checks
required operational facilities before work starts and pauses work if they become unavailable.
Persist material reservation/consumption and progress so resuming cannot spend inputs twice.
Completed knowledge can remain unlocked while a facility's absence prevents production;
exact maintenance, operational/idle and loss-of-facility semantics still need a decision.

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
proposed grant trigger is first operational spaceport and its exploitation concession,
rather than a cheap indefinite construction claim; this trigger is not yet confirmed.
Construction-site races need a clear saved commitment boundary.

Late joiners and AI retain the previously planned capacity considerations. Unassigned starter
planets with no ports are eligible for corporate construction under the confirmed rule;
reservation alone is not a hidden exemption. Gold-rush urgency and one-port scarcity amplify
this issue. Choose starting regions, existing ports/access, initial capital and any support
explicitly before claiming equal viability or long-running capacity.

Later expansion should follow the same generally rich, less developed frontier profile.
It must append compatible generated world data without moving/regenerating the core and
preserve routes, identities, in-flight goods and rights after restart. Static-map supply
chains and tech progression should be proven before live topology expansion.

## Real-time exclusive exploitation

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
rename, station transfer or reissued RPC cannot restart the exclusion period. Scope exact
ownership-transfer behaviour before exposing it. Order due events before dependent commands
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
The fiscal/wage/consumer-income and currency-source rules needed for sustained circulation
remain to be designed; this plan does not silently add automatic subsidies, unlimited credit,
household simulation or money creation. A public budget may pause when exhausted.

There is no win condition or automatic victory reset. Before promising days/months of viable
idle play, measure turnover, concentration, depleted reserves, stranded inventory, broker
liquidity, project bottlenecks and availability of alternate suppliers. Adjust authored
scenario/balance intentionally; do not hide missing demand with teleport trades or minted
broker accounts.

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
claimed. Next refine resource rights, construction-site commitment and the first concrete
facility/material recipe, then long-running demand/public-income circulation.
