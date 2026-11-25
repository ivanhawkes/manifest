# Regional populations, economies and government

Selected correction, 2026-11-16, against Manifest `feature-base` at `2094b55` and SDK pin
`f850920`. This supersedes planning assumptions that each inhabited planet has exactly
one population, economy, household pool, public treasury or government. These are planning
requirements; no implementation, content/schema migration, build or gameplay validation
has occurred. Earlier session-handoff sections remain historical.

## Selected scope

A physical planet can contain several governed economic regions, also called districts
in this plan. Each region has separately tracked population and economic outcomes. The
user's examples of different parts of Earth express the required granularity; they do
not select a real-world map, political classification or authored region list.

The first playable governance scenario must demonstrate multiple regions on one planet.
A player initially controls one region's public budget and development alongside their
separate trading company and 160-tote starter ship. AI governs other regions. Several players
can therefore have home regions on the same planet. One region is an initial entitlement,
not ownership or government control of the whole planet. Preserve the tiny two-venue
trading fixture separately; it cannot establish regional governance isolation by itself.

Each inhabited region owns its aggregate population record, household pool, unmet needs,
health and happiness, public budget, development jobs and local economic policies. Keep
company funds and privately owned facilities/inventory separate from regional public
funds. Corporate government does not merge those accounts. The previously selected wage,
household, shortage and production-consequence rules apply at regional scope.

## Identity and relationships

The required distinction is physical location, population/economy and controlling or
associated organisation. The following mapping is the planning contract for that distinction:

| Concept | Identity and scope |
| --- | --- |
| Planet | Stable physical body, environment and physical sites; parent of regions. Population/economy totals are derived summaries, not a second spendable account or population owner. |
| Economic region/district | Stable ID with a parent planet; authoritative population, household funds, needs/condition, public accounts and locally scoped development. |
| Government/control right | Explicit authority over a region's permitted public decisions. Do not infer authority over sibling regions from the shared planet or civilisation. |
| Corporation | Stable organisation owning its commercial assets/accounts and possibly holding government rights. A corporation can operate in several regions without creating a second population there. |
| Trading bloc or wider government | Explicit membership/control relationships among regions or organisations. Grouping does not duplicate member population, household money, public budgets or goods. Detailed shared powers remain open. |
| Civilisation affiliation | Existing authored cultural/technology identity, distinct from planet, region and company. Shared technology/effect scope needs explicit rules. |
| Facility/site | Physical location plus regional jurisdiction and private/public ownership. The employer, workers' household destination and tax destination must be resolvable explicitly. |
| Market/station | A physical trading venue with its own stable identity, operator, access and inventories. It may serve several regions; neither region nor planet is automatically a unique market ID. |

Use a single population-owning region for each represented population aggregate. A government,
corporation and trading bloc may overlap as relationships, but must not each instantiate
the same people and demand. A corporate-governed district can have its own population and
outcomes; an employer's activity in an existing region is not automatically another district.
Detailed nested borders, overlapping jurisdiction powers and cross-region commuting are
not selected. Individual citizens and families remain unsimulated.

## Economy and development consequences

- Population is an aggregate count of people, retaining float/double quantities and one
  household pool per region. Author consumption per 1,000 people per simulation day
  (86,400 simulation seconds), scaled by population and elapsed interval duration.
  A 25,000-person region needs 25 daily baskets, divided across 1,440 normal 60-second
  updates. Preserve fractional requirements. Initial population sizes and population-change
  rules remain open. Legacy abstract world units require an explicit conversion for any
  supported migration, preserving totals rather than silently relabelling units as people.
- One household pool per inhabited region replaces one per inhabited planet. Scale each
  essentials basket by that region's population and elapsed simulation time. Keep fresh
  interval needs, essentials-first purchasing, credit carry and no catch-up consumption.
  The selected first basket has three needs, each initially weighted 1: staple food satisfied
  only by rice, wheat, oats or barley; drinking water; and basic medical supplies. The grains
  are alternative tradable food commodities, distinct from planting seed. Bread remains
  tradable outside this essential basket. Food grains trade in kilograms; any qualifying
  kilogram satisfies one kilogram of staple-food demand, including mixtures. Keep distinct
  prices/stocks/storage and debit actual grains once, without creating a food commodity.
  Initial food demand is 500 kg per 1,000 people per simulation day across all qualifying
  grains, prorated by elapsed simulation time. A 25,000-person region needs 12,500 kg/day,
  or `12,500 / 1,440` kg per normal 60-second interval. This is a balance-test requirement,
  still limited by actual funds and physical supply; retain shortages and no catch-up demand.
  Initial drinking-water demand is 3,000 litres per 1,000 people per simulation day,
  separate from agricultural/industrial water demand. A 25,000-person region needs
  75,000 litres/day, or `75,000 / 1,440` litres per normal 60-second interval. Preserve
  fractional quantities and physical volume through any conversion to inventory units.
  This balance-test requirement retains actual supply/fund limits and shortage rules.
  Basic Medical Supplies is a distinct tradable `Kilogram` commodity representing routine
  medical consumables. Existing antibiotics, anaesthetics and analgesics retain separate
  identities and do not automatically substitute for it. Initial basic-supplies demand is
  5 kg per 1,000 people per simulation day: 125 kg/day for 25,000 people, or `125 / 1,440`
  kg per normal 60-second update. Retain fractional quantities, finite funds/stock and
  recorded shortages. Remaining medical item authoring values and the exact water definition
  remain open;
  agricultural/industrial water quantities and rules are separate design work.
- Wages go to the applicable regional household pool and selected 10% withholding to the
  applicable regional public treasury. Carry tax remainders per employer and region across
  its batches/facilities there. The region is an explicit settlement key; selecting a
  different station or another company facility cannot silently redirect tax or reset carry.
  Cross-region work/residence and multi-region payroll allocation need explicit rules
  before those cases are enabled. Do not silently pay one wage to several regions.
- Retain regional shortages, health and happiness. Each condition value ranges from 0 to
  100 with fractional changes allowed; 100 is ideal and 0 the lowest modelled condition.
  Both start at 100 in the first balance-testing scenario. Joining or acquiring an existing
  region preserves its current values; a governor change cannot reset condition.
  Update after household purchases every 60 simulation seconds. Unmet essentials lower
  both values, with greater loss for larger shortages; meeting all essential needs permits
  gradual recovery. Happiness declines and recovers faster than health. Bound both to 0–100;
  initial test rates per simulation hour are health −6/happiness −12 at complete shortage,
  with partial-shortage losses proportional to severity, and health +3/happiness +6 recovery
  at full essentials. Severity averages the unmet fractions of current required essential
  needs with authored importance weights, initially all 1. Count food once, after converting
  consumed kilograms across qualifying grains with equal substitution. Cap fulfilment at 100%; surplus
  cannot offset another missing essential. Prices and physical quantities do not set importance.
  Both condition values slow relevant regional production work rate through the initial
  multiplier `(0.5 + health / 200) × (0.5 + happiness / 200)`, ranging from 25% to 100% of
  otherwise eligible work rate. Missing materials/funds or inactive facilities can still
  stop work entirely. Completed batches retain their declared inputs, outputs and gross wage bill;
  slower work slows proportional wage accrual/payments. Changes affect subsequent steps
  without reversing completed progress or earned/paid wages. Production uses previous-step
  condition; on each due economic boundary, follow production/completion settlement with
  regular payroll/tax, household purchases/consumption and condition update. New condition
  applies from the next fixed step. A shortage in A must
  not directly overwrite B's condition or apply a planet-wide penalty; trade and supply
  dependencies can still cause legitimate economic spillovers.
- Public projects, spending limits, installed local infrastructure and locally earned
  progression target a region. The Bronze metallurgy example's materials, recipe and
  facility dependency remain selected; its public project/local unlock belongs to a
  target region. Do not grant its local improvement to every region sharing a planet.
  Planet-wide environmental work and explicitly shared technology require declared scopes.
- Facilities and resource sites need regional attribution without duplicating physical
  deposits, stock or reservations. A shared station can hold separately owned goods for
  several regions. Domestic/inter-region transport still needs explicit custody, route,
  access and delivery rules; being on the same planet does not teleport goods or pool cash.
- Planet/bloc dashboards aggregate regional results. Sum population, compatible flows and
  account balances only as read-only totals; define weighting for condition summaries.
  Do not feed a displayed average back into every region's production or spend a derived total.

Regular payroll/household purchasing remains every 60 simulation seconds. Population
magnitude does not create more agents; workload now also scales with region count,
regional facilities/commodity entries and actual transactions. Measure those separately
from planet count. A one-planet world can contain many independently active economies.

## Control, construction and discovery boundaries

Onboarding allocates no region since W16 ([D-089](../decision-ledger.md#d-089)): a player founds one
homeworld in a region no one governs ([D-110](../decision-ledger.md#d-110), W13). Below, read
allocation as founding: the eligible, reserve and acquisition counts are of regions open to
founding, and the station, delivery and life-support conditions are founding's, which `Found`
does not yet check.
Onboarding allocates a region rather than an entire planet. Count eligible home regions,
late-join reserve regions and an AI economy separately; reserved entries cannot also be
sold as the acquisition pool. Allocate a starting region only when an operational public
station supports the starter ship and has a physical delivery connection to that region.
An available working station life-support connection is also required; the new player
spawns connected, pausing onboard oxygen and scrubber consumption. Initial starter reserves
are 30 kg oxygen and one fresh installed 30-occupant-day cartridge, provisioned once.
Station provisioning and later service-outage policy remain separate choices.
Guarantee continuing basic docking, trading and regional delivery through that public station;
corporate players cannot revoke this baseline access. Service charges and finite capacity
still apply, and goods and spending funds remain finite. Private ports retain their commercial
access policies. Exact fees, capacity and starting funds need balancing; the access guarantee
does not establish affordable supplies or profitable routes. Available region count alone
does not guarantee a playable start.

The selected protection/acquisition rule applies to each player-governed region, including
later acquired regions. Another player cannot purchase or seize it through the AI-acquisition
mechanic. Eligible AI-governed regions remain purchasable even when they share a planet with
a player's protected home. Reserved onboarding regions remain outside that acquisition pool.
Governing one region grants no veto over acquisitions in sibling regions.

Purchase transfers the named region's public decision rights. Its treasury remains public
money, and privately owned companies, ships and stations retain their owners. Sibling
regions and their population, accounts and control remain independent. This rule does not
select purchase pricing, the payment recipient or a voluntary player-to-player transfer
mechanic; those remain separate design work.

The selected frontier resource concession names one economic region and covers its eligible
resources. Exclusivity does not extend to sibling regions: neighbours remain independently
available under their own eligibility rules. Keep the accepted 72-hour server-running duration
and operational-port/concession activation trigger. The concession grants resource rights;
government control remains separate. An operational port or activation in A must not silently
activate, consume the entitlement of or grant resource rights over B.

The selected port-construction rule provides one initial construction opportunity per eligible
region. A company may build for its own region, an eligible AI region or an eligible unclaimed
region. A port associated with a neighbouring region on the same planet does not block it.
Other players' governed regions are protected against unsolicited port construction. Retain
the funded construction race and allow competing ports when a frontier concession expires;
government changes cannot turn the frontier reopening rule into permanent exclusion.

Ports remain physical orbital stations. A named regional association determines construction
eligibility, while a port may serve several regions on the same planet. Its owner chooses
offered services, fees, priorities and access terms, honouring accepted bookings. Deliveries
name a destination region and require physical transport; regional inventories, household
funds and public budgets remain separate. Serving a neighbour grants no government control
or resource rights there and does not consume its initial port-construction opportunity.
Market visibility remains station-specific; shared service does not reveal other stations'
live trading data. Transport costs, capacity and delivery times remain separate design work.

Station ownership, public control, resource rights and port services are distinct. Authored region
boundaries, cross-border deposit attribution and control changes during construction still
need rules; do not duplicate a deposit or extend a concession by changing its ID, owner or
government. Onboarding must explicitly address existing construction claims and viable access.

Stations should express the cultures and corporations that own and operate them, with
customisable looks, corporate colours, logos and distinct station identities. The detailed
visual/customisation design is deferred to the station presentation work described in the
[development plan](../planetary-development-plan.md).

Fleet market visibility remains station-specific. Regional government, corporate activity,
bloc membership or shared planetary location grants no new live station data. Starting
reports remain for every station market in directly connected systems at spawn; a region
is not another star system. Preserve observation ownership and ship-transfer rules.

## Capability map and implementer evidence

| Requirement | Existing evidence | Required work and owner |
| --- | --- | --- |
| Separate physical and economic identity | `Simulation.h::Civilisation` describes a generated planet civilisation and combines population, treasury, production, demand and shortage state. `MarketplaceService::LoadSimulation` and `BodyInfo` retain civilisation/body coupling. | Manifest introduces explicit planet/region/government/company/venue relationships in hosted state. Existing authored IDs and YAML keys remain stable references, not interchangeable runtime owner IDs. |
| Region-scoped state and persistence | Public pinned `SimWorld::RegisterComponent`, `RegisterSystem`, `WorldService`, `Save`, `Load` and `StateHash` provide generic facilities already used in the host plans. | Manifest registers regional state, validation, commands and migrations through Jettison archives/definition mechanisms. No additional generic SDK gap is demonstrated for regional populations/accounts. |
| Regional decisions and physical services | Existing company/account, venue inventory, docking and host/script plans provide partial building blocks, not implemented regional government. | Manifest scopes permissions, public procurement, project effects, payroll and query facts to region IDs while keeping physical transport and station observation rules. Provision further reusable engine gaps only when demonstrated. |

Implementer acceptance must include:

1. Two inhabited regions on one planet with distinct governors, populations, household/public
   balances, facilities and health/happiness; at least one human-controlled region and one
   AI-controlled region. Include multiple player home regions on one planet in isolation tests.
2. A supply shortage in one region and adequate supplies in the other: distinct consumption,
   retained condition and appropriate local production consequences without duplicated people
   or planet-wide condition writes. Check both values independently once formulas are selected.
   Both begin at 100 in the first balance-testing scenario and remain within 0–100, allowing
   fractional changes. Joining, acquisition, governor changes and save/load retain existing values.
   Condition updates follow purchases once per 60-simulation-second economic interval. Larger
   essential shortages cause greater losses; full essentials permit gradual recovery, with
   happiness changing faster in either direction away from bounds. Test replay/retry without
   duplicate responses and supply restoration without an instant reset. At the selected rates,
   one hour of complete shortage takes 100/100 to 94/88; one following full-essentials hour
   yields 97/94. Partial-shortage losses scale with severity; each 60-second update preserves
   fractional changes, bounds and simulation-time semantics.
   With two equally weighted essentials, one fully and one half supplied, severity is 25%
   and hourly losses are 1.5 health/3 happiness. Surplus of one good cannot cancel another's
   shortfall; optional goods and zero requirements do not enter the severity calculation.
   For the first three-need basket, qualifying grain can meet food without requiring the
   other grains. Full food/water but no medicine gives one-third severity and hourly losses
   of 2 health/4 happiness. Bread and planting seed cannot silently substitute. Implementer
   checks include 100 kg of one grain and 60 kg rice plus 40 kg barley satisfying a 100 kg
   food requirement, with separate inventories/prices/storage and no synthetic food stock.
   Verify the selected food rate: 500 kg per 1,000 people per simulation day, with 25,000
   people requiring 12,500 kg daily across the qualifying grains. Retain fractional interval
   requirements and record shortages when funds or supplies are insufficient. Verify
   water demand of 3,000 litres/day per 1,000 people and 75,000 litres/day for 25,000 people,
   including fractional intervals, physical volume conversion and separate agricultural/
   industrial uses without double consumption. Check distinct kilogram-based Basic Medical
   Supplies stock and consumption, with specialised medicine items preserved and no automatic
   substitution. Verify 5 kg/day per 1,000 people and 125 kg/day for 25,000 people, fractional
   intervals and daily totals. The three essential needs retain importance weight 1 each.
   A controlled half work rate doubles baseline batch duration without changing completed
   material/output/payroll totals. Mid-batch condition changes affect only subsequent work
   and proportional wage accrual, preserving progress and earlier settlement across recovery.
   Check multiplier examples: 100/100 → 100%, 50/100 → 75%, 50/50 → 56.25%, 0/0 → 25%.
   The condition floor must not bypass material/funding/operation requirements; demonstrate
   independent effects from both condition values without changing sibling regions.
   At due economic boundaries, use prior-step condition for production, settle completion
   without duplicate regular payroll, then purchase/consume and update condition for the
   next fixed step. Save/replay/retry preserves cadence position and settled interval state.
3. One employer paying in two regions: correct household/treasury destinations, isolated
   employer-region tax carries and conserved gross funds through interruption and recovery.
4. A region's public project, including the selected Bronze example, cannot spend sibling
   public funds or propagate a local unlock by planet/civilisation association. Cross-region
   supply can arrive through an explicit delivery without merging inventories.
5. Player/public/company permissions remain distinct under a shared government, corporation,
   trading bloc, planet or station. Shared dashboards reveal only permitted fields; station
   market data still obeys fleet observation. Reject AI-acquisition attempts against another
   player's home and acquired regions. Allow otherwise eligible AI-region purchases on the
   same planet without a sibling governor's veto; preserve public funds and private owners.
   Control acquisition cannot silently take siblings. Reserved onboarding regions stay ineligible.
6. Generation and any supported legacy migration preserve total population, funds, goods,
   pending work and physical identities. Do not clone an old planet's treasury/population
   into every new region. An old one-economy fixture may map explicitly to one region;
   producing several regions needs authored allocation, not guessed splits or renamed IDs.
   Apply the declared world-unit-to-people conversion once. Verify daily per-1,000-person
   demand, fractional 60-second requirements and zero-population demand without citizen agents.
7. Save/hash/replay and crash/retry preserve parent links, control, regional accounts,
   condition, tax carries, projects and routes. Measure scaling by regions as well as planets,
   with zero clients and without individual-population agents. Implementer runs relevant
   presets and gates with each machine's concurrency (D-051), local clang-tidy opt-in.
8. Two frontier regions on one planet have independently scoped resource eligibility and
   concession lifecycles. Activation, extraction and expiry in A cannot grant rights, start
   a timer or block otherwise eligible exploitation in B. Preserve the named region and
   original timer through restart/replay and ownership/government changes. Neither activation
   nor expiry transfers government control.
9. Two eligible regions on one planet can each host an initial port project; a neighbour's
   port or funded claim cannot consume the other's opportunity. Race companies for the same
   regional opportunity, charging only an accepted funded start. Reject unsolicited builds
   for another player's governed region; permit competing frontier ports at concession expiry
   despite a puppet government. Preserve association, claims and original concession timers
   through restart/retry. Control changes during construction need separate rules before
   those cases are enabled and validated.
10. One port serves two regions on the same planet under its owner's offered terms. Deliver
    goods to the named region through an explicit physical route without pooling accounts or
    inventory, duplicating cargo or changing neighbouring government/resource rights. Retain
    each served neighbour's construction opportunity. Exercise new access denial and protected
    accepted bookings independently, with equal player/NPC rules. Shared service grants no
    additional live market visibility. Preserve service/destination and operation identities
    through save/replay/retry once transport and service details are selected.
11. Reject starting-region allocation without an operational public station compatible with
    the starter ship, a physical delivery connection to that region and available working
    life support. Verify successful spawn establishes the service connection and pauses
    onboard oxygen/scrubber use; unavailable or exhausted service capacity rejects allocation.
    Retry/recovery preserves the connection without duplicate spawn/resupply. Basic docking,
    trading and regional delivery access must remain available under later corporate bans
    or access-policy changes. Preserve normal service charges, finite capacity/goods/funds
    and private-port policies. Save/replay the public-service guarantee and served-region
    relationship; validate starter fees/funds and capacity once their values are selected.

Remaining product choices include region generation/allocation, precise organisation powers,
starter service fees/capacity/funds, transport costs/capacity/times and authored region boundaries,
inter-region transport/access, cross-region
employment/tax routing and shared technology. Numeric household/condition/production balance
remains open as before. This correction resolves the simulation unit; it does not establish
implementation, migration compatibility, economic behaviour or performance.
