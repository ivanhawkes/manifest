# Planetary development, corporate power and frontier growth

Planning assessment recorded 2026-11-15. This extends the [roadmap](roadmap.md),
[gameplay/server plan](gameplay-and-server-plan.md) and [development stages](development-stages.md).
It records product direction and proposed implementation slices, not working capabilities.
OpenAI work is limited to investigation and planning documentation; implementation and
build/gate execution remain with the selected implementer.

Inspection baseline: Manifest `feature-base` at `8f02d54c2935dc929052a4e753c66bcf9f777a5c`,
Jettison `feature-base` at `0564f19ed41bc7e0a278952153567d4a137a2f5b`. Manifest still pins
`f85092027eda49c29001f7589608df1f0b8827fb`. Both working trees were clean before these
Manifest documentation edits. Read the [boundary assessment](engine-game-boundary-assessment.md)
and the selected SDK's game-integration guidance before implementation.

The [regional correction](regional-economy-and-governance-plan.md) supersedes the earlier
one-government/population-per-planet assumption. Public control, households, condition and
local development are region-scoped; multiple governed economies must coexist on one planet
in the first governance scenario. Player-governed regions are protected from the
AI-acquisition mechanic; eligible AI neighbours on the same planet remain purchasable.
Frontier resource concessions name one region and cover its eligible resources; neighbours
remain independently available under their own eligibility rules. Historical planet-wide
port eligibility is superseded by one initial construction opportunity per eligible region;
neighbouring ports do not block it, and other players' governed regions are protected against
unsolicited construction. A port may serve several regions on that planet under its owner's
offered terms, with explicit physical delivery and separate economies/rights. Detailed
transport remains open; starting regions have guaranteed basic public-port access. These rules
do not make one governor sovereign
over siblings.

## Confirmed direction and open choices

- The game is a persistent, networked idle game. One headless server runs the authoritative
  simulation, NPCs, autopilot, enabled auto-trading and development with zero clients connected.
  The existing TUI becomes a `connect`/`disconnect` gameplay client; a GUI uses the same
  commands and server state with the selected public engine capabilities. Manual TUI trading
  remains a stretch goal; automatic physical trading is required.
- Each player initially controls one region's development and public budget alongside a
  separate trading company. Other inhabited regions have AI governors. A disconnected player
  retains control; a server-side executor continues the player's saved standing policies.
- The first development controls are project selection, investment budgets and public
  procurement. Taxes, subsidies and broad regulatory powers are outside this first slice.
- Public procurement requires competing corporate bids, including a governor's own company.
  After basic tendering works, add a simple government corruption trait and abstract bribery
  that can influence awards. Detailed officials, political networks and investigative systems
  are not required for this first corruption slice.
- Pay suppliers for accepted partial deliveries as they arrive. Honour accepted station
  bookings under their agreed terms; owner price/access changes apply to future requests.
- Design corporate leverage through infrastructure, essential supplies and access contracts
  first. Government financing/debt and media influence remain later goals.
- A sufficiently powerful corporation can build and control a space station. It may set fees
  and shipping priorities and refuse rival companies access to its station. Station ownership
  does not by itself give the company sovereignty over the system or every possible route.
- Initially a corporation may build a very expensive orbital spaceport around any planet without an
  existing spaceport, provided the planet is not owned/controlled by another player. Resource
  exploitation and colonies with company-controlled puppet governments provide long-term
  returns. Exact cost, exploitation rights and the scope of colonial authority remain design work.
- Subsequent concession refinement: exclusivity lasts for real elapsed server-running time,
  pausing during shutdown. Afterwards rivals may exploit and build competing spaceports even
  under an incumbent's puppet government. Starting homes stay protected. This supersedes the
  no-existing-port condition on reopened frontier concessions, not home protection.
- Credits transfer instantly; goods follow extraction/industry, haul, station warehouses,
  ships and destination delivery. Brokers use their own finite funds/stock, with buy/sell
  spreads and separate warehouse handling charges at both ends. Supply is physically capped;
  scarcity/demand set prices. See [physical economy/development](economy-and-development-plan.md).
- Planet tech needs exploited/imported materials and actual production facilities. The inner
  map begins more technologically developed and resource-depleted; outer regions are richer
  and less developed. These dependencies create the intended rush for control of fresh resources.
- Later purchases of eligible AI regional control rights and frontier settlement remain
  goals. All player-governed regions are protected from the AI-acquisition mechanic;
  otherwise eligible AI neighbours remain purchasable without a sibling governor's veto.
  One region is the starting entitlement, not a permanent cap or planet-wide sovereignty.
- The long-term objective is a trading corporation with enormous power over government and
  media, comparable in scale to Earth's largest corporations. There is no win condition.
  Players accumulate wealth, power and control, using them to establish a stranglehold on
  trade, economies and governments. There is no victory threshold or automatic end-of-game reset.
- Size the initial world for the group expected to join over the server's life, with additional
  AI planets. If necessary, expand at the map's outer reaches with generally less developed
  systems and potentially uninhabited worlds for terraforming and settlement.
- Opening a new system requires a government/corporation-funded stargate expedition: automated,
  unmanned FTL delivery, timed by distance and configurable speed/pacing over hours or days of
  server-running wall time. Operational gate owners control access and fees, including rival
  refusal. See the [stargate handoff](stargate-expansion-plan.md); gate ownership does not itself
  grant planetary control or resource exclusivity.
- Days versus months, server lifespan/reset policy, exact development permissions, acquisition
  pricing, influence mechanics and expansion triggers remain undecided. Server downtime still
  pauses the simulation initially; disconnected-player progress is not downtime catch-up.

## Gameplay impact: a company shapes its own trading environment

Planetary development makes trade and corporate expansion interdependent. Investment can
change production capacity, consumption, infrastructure and transport throughput. Those changes
create opportunities for the company's ships; shipping supplies future projects. A company
can grow from one local operation into a network that other economies and governments depend on.

The intended loop is: earn credits through trade, direct development, supply projects, expand
logistics, acquire additional control rights, and convert economic dependence into government
and media leverage. AI companies and governors need enough agency to supply demand, compete,
seek alternatives and react to changed conditions while human players are away.

Influence needs effects beyond an ever-growing score. Candidate channels include government
procurement and financing, infrastructure concessions, dominant supply contracts, control of
transport capacity and ownership or financing of media institutions. These are proposals for
later design, not an approved list of implemented systems. Each needs an explicit asset or
agreement, cost, duration, target, measurable effect and rules for resistance or replacement.
Media influence is neither a production multiplier nor automatic obedience by every population.

Monopolisation and coercive economic power are desired possibilities. The design question is
how dependencies are created and exercised, and how rivals respond. Supply withholding,
exclusive contracts and government pressure can be candidate mechanics; combat, conquest and
piracy do not become requirements through this goal. Influence cannot silently bypass the
agreed protection against acquiring another player's governed region through the
AI-acquisition mechanic.

With no win condition, reaching a dominant position must leave meaningful decisions: managing
a larger portfolio, protecting dependencies, negotiating access and confronting new markets
or challengers. Frontier opportunities can create new contests, but adding territory alone
does not stop a wealthy incumbent buying every opportunity first. Measure concentration,
entry opportunities and the effectiveness of corporate leverage before choosing upkeep,
administrative constraints, protected supply or other balance rules. Do not introduce an
arbitrary empire cap or automatic redistribution as though the user approved one.

## Identity, public budgets and control

Keep these concepts explicit even where the first scenario has one of each:

| Concept | Responsibility |
| --- | --- |
| Player/principal | Authenticated identity and granted command permissions. |
| Trading company | Commercial credits, ships, cargo, orders, contracts and corporate holdings. |
| Planet | Stable physical identity, environment and physical sites; parent of multiple regions. Population and economy totals are derived summaries. |
| Economic region/district | Stable regional identity and planet link; population, household funds, needs/condition, public accounts, local development and policies. |
| Government/control right | Authority over a target region's permitted development programme and public budget, without implied sibling-region control. |
| Corporation or trading bloc relationships | Explicit ownership, control or membership links; do not duplicate a region's population, goods or accounts when groupings overlap. |
| Civilisation affiliation | Cultural/technology identity and explicitly shared effects; not automatically the player's company. |
| Corporate influence | Recorded relationships and leverage over institutions; not interchangeable with legal control. |

The player may direct both a company and a public budget without making them one wallet.
Public procurement from the player's company can be an explicit transaction with defined
rules; it cannot be a hidden treasury alias. Visiting ships, private inventory and other
traders' escrow retain their existing owners. Decide taxes,
tariffs, subsidies, borrowing and direct budget transfers separately; the current decision
authorises development control, not every possible government power.

The current model conflates these identities: `Civilisation` is a generated world with one
treasury, while `MarketplaceService::LoadSimulation` uses its ID for the planet, civilisation
and venue. `BodyInfo` has a civilisation association, and effects select all bodies/markets
of that civilisation. Preserve existing authored IDs and references; introduce explicit
relationships and migrate runtime state rather than renaming content or mapping every new
planet or region back to its controller's ID. The development harness is evidence of the
coupling to replace, not the required multi-region identity model.

Persist independent regional development state. Classify each progression/effect as regional,
physical-planet, company-wide or civilisation-wide before applying it. Buying another region
must not grant
all local improvements everywhere, reset its age, or copy the acquiring company's whole tree
onto it. A civilisation's shared technology and a region's installed infrastructure can
coexist, but their concrete sharing rules remain open.

## Development and unattended operation

A useful first slice is one infrastructure project with a visible effect on production or
port capacity. The player sets a priority and budget; the server reserves funds/inputs,
advances work at fixed-step boundaries, and completes the effect once. Prove queueing,
cancellation/refund policy, missing-input pauses and restart continuity before adding a
complete government simulation. Exact project costs, durations and construction rules
remain design work; existing instant skill purchases are not a timed construction system.

Development should provide choices about specialisation, capacity, import dependence and
investment timing. It must not require logging in to restart a queue after every small job.
Standing policies need spending limits and visible reasons for pauses; an absent player must
not accidentally grant an AI governor permission to spend all company funds or buy planets.
The policy executor and autonomous AI governors use the same native validation rules.

Keep economic cadence, development work, upkeep and travel on explicit simulation schedules.
Do not run every region's economic AI at physics frequency merely because ships need frequent
movement updates. Cadence and deterministic work limits are game data/contracts; wall-clock
timeouts cannot determine which region finishes a project or gets a purchase first.

Population, age and habitability need separate transitions. The current generation scales
population with age and fixes production/demand decks; increasing a tier must not regenerate
the planet/region or create people and stock without a defined source. Age advancement can unlock
production options without instantly installing them. A low-tech starting region needs
access to the player's starting space-trading operation or external services so the core
loop is usable before local space technology exists.

On reconnect, TUI and GUI should show public/company balances separately, projects, shortages,
current trade jobs, control rights and a bounded activity report. Server-side queries filter
private budgets, company plans and influence agreements. Losing a session neither cancels
work nor transfers control to a new player. Abandonment/reassignment after long absence is
an unresolved server policy, not a consequence of disconnecting.

Market information also follows the [fleet-observation contract](market-information-plan.md):
live data requires a fleet member docked at that station; other views use dated remembered
reports, including one-time spawn reports for all markets in directly connected systems.
The [economy plan](economy-and-development-plan.md) adds saved aggregate population health
and happiness; shortages lower both and each contributes to lower production. Detailed
condition response/recovery and production penalties remain design work.

## Public procurement, corruption and abstract bribery

The [supply-contract and station-booking handoff](procurement-and-station-contracts-plan.md)
now details a proposed first tender, payment/default rules and booking acceptance boundary.
It records the selected partial-payment and existing-booking protections and supplier-funded
performance bond. Other defaults remain proposals, including detailed numeric settlement.

Competitive bidding is a confirmed rule. Controlling a public development budget lets a
player choose projects and funding, not directly award the contract to their company. Their
company must bid through the same tender as other eligible companies. A puppet government's
control link does not bypass that requirement. Privately funded corporate spaceport
construction is not a public procurement award merely because it occurs near an AI planet.

First implement a clean tender lifecycle: specification and reserved public budget, opening,
eligible bids, a fixed-step closing boundary, one award, physical fulfilment/milestones,
payment and defined cancellation/default. The specification states required delivery,
service/capacity, price rules and deadline. The selected bond starts at 10% of the winning
offer, funded at award and held until close; complete delivery returns it, while default
forfeits the undelivered share to the public treasury. Accepted goods remain payable immediately.
Do not accidentally reserve the same shipment or capacity for multiple winning contracts.
An award cannot pay a supplier twice or leave both spent and refundable public funds available.

A proposed initial auction has sealed bids and deterministic evaluation of qualified bids
using authored price and delivery/capability criteria. Eligibility and closing deadlines are
native validation. At the closing simulation step, late bids are rejected even if command
processing runs before award evaluation. Tie-breaks use a declared stable order, not client
latency measurements, unordered iteration or parallel handler completion. Bid visibility,
revision/withdrawal rules and precise scoring weights are still design choices.

The selected next slice adds one authoritative corruption-susceptibility trait per government
and an abstract company-funded influence/bribe expense associated with a tender. It can change
the ranking of eligible bids: a corrupt government may favour a more expensive qualified bid
after influence spending. Zero corruption means bribery cannot influence the award. Exact
costs, influence bounds, disclosure and whether unsuccessful influence spending is refundable
remain balance decisions. The trait is not an unrestricted player slider; its initial source
and any later changes need authored/server-side rules.

For the pilot, use a deterministic scoring contribution from corruption and influence rather
than modelling individual officials or adding random political outcomes. Keep normal bid
quality relevant and bound arithmetic/work. Bribery affects award preference; it does not
make an invalid/late bid eligible, create delivery capacity, bypass public-budget limits or
override another player's protected regional control. The payer is the company, separately
from the public procurement budget. Record the expense's explicit destination/sink and result;
do not invent a recipient simulation or duplicate funds to make it work.

This limited model is worth including because it directly connects commercial wealth to
government decisions and makes differing governments meaningful. Its trade-off is additional
award-state, information and balance work. Establish the clean tender first so that corrupted
awards have a testable baseline. Detailed patronage networks, enforcement, investigations,
scandals and media cover-up are possible later mechanics, not dependencies of the first loop.

Idle companies need explicit standing bid policies: acceptable work, price/budget limits,
capacity constraints and optional influence-spending limits. Automation cannot infer permission
to bribe merely because the player disconnected. NPCs use the same bid/expense/award rules
and only information their role permits. Persist open tenders, bids, committed influence,
reserved funds, award progress and fulfilment so retries/restarts cannot re-award or repay.

Test the honest baseline, zero/nonzero corruption, differing bid quality/influence, invalid
influenced bids, simultaneous/retried bids, closing-step boundaries, one-time expenses/payment,
private bid filtering and recovery on both sides of award. Replay must obtain the same award
and ledgers. Procurement, corruption and scoring are Manifest rules using SDK world services,
commands and archives; no bribery model belongs in genre-neutral Jettison.

## First corporate-leverage slice

The agreed first focus is infrastructure, essential supplies and access contracts. A proposed
pilot connects a public port-capacity project to physical procurement and one company service
or access contract. Public funding creates the specified public asset; a separate agreement
grants the company defined operating, supply or capacity rights. Spending public money does
not implicitly give the company ownership of the port. The governor's company must win a
competitive tender; corruption and abstract bribery may affect that outcome in the later slice.

First specify the parties, asset owner, permitted decisions, effective step, duration,
capacity/input reservation, payment source, termination and failure consequences. Procurement
must pay for delivered goods or a defined milestone, not create both a refundable public
reservation and freely spendable company revenue for the same work. Supply commitments count
available cargo and capacity once. Contract retry/cancellation/expiry must have deterministic
results shared by the TUI, GUI and AI.

Then demonstrate real leverage: a planet relies on a company's delivered input or contracted
capacity; the company can use the price, renewal and access decisions granted by that
agreement. Company-owned stations also have the confirmed owner rights below. Exclusivity
and government-policy concessions through other contracts remain undecided. Protected
player-region control does not imply immunity from economic pressure: station access denial
is allowed, but cannot transfer another player's regional control. Wider government powers require
their own rules rather than hidden administrator privileges.

Measure delivered throughput, fulfilment, alternative suppliers/routes, dependence and the
economic effects on public/company budgets. These provide an evidence base for later government
financing and media ownership. A generic influence score alone does not demonstrate that a
company can exercise the power the user wants.

## Company-built stations and control of shipping

Station construction and operation are an agreed corporate-growth goal. The concrete expansion
asset is an orbital spaceport associated with a named region for construction eligibility.
There is one initial construction opportunity per eligible region: a company may build for
its own regions, eligible AI regions or eligible unclaimed regions. Another region's port
on the same planet does not block construction. Other players' governed regions are protected
against unsolicited port construction. No discretionary government permit is an additional
agreed eligibility requirement. A company funds and physically supplies a very expensive
construction project; completion creates an owned, operational spaceport with defined services
and capacity. The selected reservation rules below establish the initial funding commitment;
exact costs, stages and construction funding amounts remain to be designed.
Save work/cargo/reservations and create the port once even when
completion is replayed or a request retried.
Purchasing regional control is not a prerequisite for a port at an otherwise eligible AI
region; acquisition and port-led exploitation are alternative corporate growth paths.

For this slice, use station/spaceport to describe that orbital shipping facility. Other station
types may be designed later. Initially there is one first-port opportunity per eligible region;
reopened frontier concessions permit competing ports as specified in the economy/development plan. The user
selected these rules for the initial first-port construction race:

- The first eligible company to start a funded construction project secures the site.
- Starting requires a meaningful, non-refundable mobilisation cost and reserved construction
  funds. A request without that commitment does not hold the site.
- The reservation has a fixed completion deadline; token deliveries cannot renew it indefinitely.
- Abandonment or expiry reopens the site. Unspent funds and unused company-owned goods remain
  company property; completed work needs separate salvage rules.
- The reservation grants construction priority. Resource exclusivity and government control
  require their own grant rules.

For an explicitly named eligible frontier region, the selected resource grant starts when
the first eligible spaceport becomes operational and its exploitation concession activates.
Publish the duration before construction starts; use 72 hours of actual server-running time as
the initial balance-testing
term. Shutdown pauses it; idle facilities and disconnected owners do not. Ownership transfers,
rebuilding and government changes cannot restart or extend the term. At expiry rivals may
exploit and build ports; the incumbent retains its infrastructure and may refuse new bookings
at its own port. Protected homes retain separate rules. This concession does not transfer
government control. It covers only that region's eligible resources; neighbouring regions
remain independently available under their own eligibility rules, without sharing its timer.
The regional construction rule supersedes the earlier planet-wide port restriction. A port
may serve several regions on that planet without consuming neighbours' construction
opportunities. See the
[running-time contract](economy-and-development-plan.md#real-time-exclusive-exploitation).

These selected rules prevent an indefinitely renewable claim without the intended construction
commitment. They do not establish a measured anti-squatting result or a limit on funded projects.
The mobilisation amount, payment beneficiary, scope of reserved funds, construction recipe,
deadline duration/clock basis, and treatment of completed work still need design. Retaining
ownership of unused goods does not define their collection, relocation or storage terms.
Specify what happens if a region's control changes while its port is being built; neither
completion nor an AI-region sale may silently violate the other player's control protection.

Implementation handoff: accept eligibility, mobilisation payment, fund reservation and site
claim atomically in authoritative order, rejecting competing starts without charging them.
Persist the site/project identity, company, frozen deadline, funds, cargo and work state.
Completion, abandonment and expiry must settle once across retries/recovery. Required evidence
includes racing companies, insufficient funds, token deliveries, deadline boundaries,
abandonment, unused-property accounting and restart/retry at each transition. The exact clock
and salvage/control-change contracts must be specified before claiming those cases are covered.

### Station appearance and corporate identity

Stations should have design and flair tied to the cultures and corporations that own and
operate them. Provide a means to customise their appearance, including corporate colours,
logos and other branding, while keeping individual stations visually distinct even within
one corporate identity. Record this as a presentation requirement; detailed art direction,
asset/modular design and customisation controls are outside this economic/governance plan.
The treatment of differing owners/operators and later rebranding remains for that design.
No visual assets or customisation system have been implemented by this planning work.

### Station access and operation

Owners may set fees, service/shipping priority and deny rivals access. Represent those as
authoritative station policy, editable only by authorised company actors, with effective
step and revision. Admission checks must cover player ships, NPCs and auto-traders through
one public engine boundary; an AI calling docking APIs directly cannot bypass refusal.
Queue scheduling needs deterministic policy, stable tie-breaks and visible waiting reasons.
Bounded internal execution work is required, but equal commercial priority is not an agreed
rule: favouring the owner's ships can be an intentional competitive advantage.

A port may serve several regions on its planet under its owner's chosen services, fees,
priorities and access terms. Accepted bookings remain protected. Each delivery identifies
its destination region and requires physical transport; shared use cannot pool regional
inventories, household funds or public budgets. Offering services to a neighbour grants no
government control or resource rights there and does not consume its initial port-construction
opportunity. A region can use a shared port and later develop its own eligible port. Shared
services do not reveal other stations' live market data. Transport costs, capacity and
delivery times still need design. Starting regions require continuing basic public-port
access as specified in the onboarding policy below.

The inspected `Dock::Station` has a compatibility-aware insertion queue, not admission or
owner-priority policy hooks. `RequestPad` checks authority to command the ship, not permission
to enter a station. Jettison needs the smallest reusable admission-decision/refusal and queue
ranking seams, exercised by an independent non-Manifest consumer; Manifest supplies company
rules, priorities and payment. `Docks::Park` is a direct startup-parking bypass: restrict it
to trusted world creation/restore composition rather than using it for runtime arrivals.
All ordinary player/NPC arrivals must pass the same policy. Re-evaluation of waiting ships
after policy changes depends on the explicit commitment rules below.

Define which services a station supplies: docking, loading/unloading, storage, refuelling,
repairs, markets or onward transport are candidates, not an approved mandatory service list.
Ownership, operating concession, market authority and planet development rights can have
different holders. A station's location near a planet is not evidence that the corporation
owns that planet or all goods inside the station. Preserve visitor/private inventory and
escrow identities independently of facility ownership.

The pricing/policy boundary must state when an arriving ship obtains a quote, reserves funds
or capacity and commits to a service. Accepted bookings retain their agreed access, price
and priority terms when the owner changes future policy. A quote or pending request is not
yet an accepted booking. The detailed handoff proposes bounded arrival/queue/service windows;
notice, refunds, reroute, release and incapacity liability still need final design. Station
exclusion can create real
commercial pressure without teleporting, deleting or silently confiscating another company's
ship/cargo. A failed or refused visit returns a durable operation state that automation can
reason about; it cannot wait forever without a visible reason or bounded retry policy.

Control of shipping is physical and economic. A well-placed station may dominate services
other companies depend on, and its owner may refuse them. Mandatory use by every regional
ship is not yet agreed; bypass routes, spaceports at other eligible planets and contested
contracts are possible responses to assess. Rival ports become possible on a frontier planet
after its real-time exclusive period, even under a puppet government. Protect starting homes
separately; do not use a controller field to cancel frontier reopening. Model essential-input
dependence and route
costs before declaring a station a universal toll gate. Operation/upkeep, rival construction
and station-transfer rules remain balance/design work; combat/destruction is not required
by this goal.

First prove construction, one usable service, owner fees/priorities and rival denial on a
static map, including auto-trader rerouting or a reported blocked job. A later scenario can
connect station leverage to public procurement and planetary supply dependence. Both TUI
and GUI query ownership, service policy, queues and outcomes through the shared interface.

## Resource exploitation and company-controlled colonial governments

The spaceport is an investment in access to a planetary economy, not merely a docking-fee
generator. The approved direction connects it to resource exploitation and the establishment
of colonies with puppet governments controlled by the company. A mature corporation can use
those dependencies and governments to direct development and strengthen its trade position.
The specific legal title, colony powers, exclusivity and relationship to a pre-existing AI
government remain to be defined; building a port does not yet imply automatic transfer of
every planetary asset or immediate replacement of an inhabited world's government.

Represent resource sites/capacity, extraction projects, local inventory, transport and sale
through the same physical economy. Company resources move through production/loading/shipping
and settlement; avoid a parallel daily credit bonus unrelated to goods or buyers. Extraction
rates, deposit depletion/renewal, labour, equipment and upkeep are open balance rules. The
high initial cost targets long-term profit from exploitation, shipping and services; actual
returns depend on viable resources, demand and operating choices rather than a guaranteed yield.
Define the surface-to-orbit logistics abstraction: debits, in-transit quantities, port receipt,
capacity and delivery time must be explicit even if individual shuttle flights are abstracted.
Manual surface landing is not a prerequisite for this economic loop.

Colonies need supplied equipment, population sources and habitability/life support appropriate
to the destination. Their puppet governments should carry explicit company-control links,
public budgets and standing development/procurement policies. This allows the company to exert
control while preserving the agreed separation of public and company accounts. Transfers,
contracts and remittances need explicit rules. Do not turn all colonial public money into
company funds merely by using the same player controller.

Start with an eligible static-map planet, one spaceport, one exploitation project and one
company-directed colony. Frontier generation and full terraforming can follow once that
physical, saved loop works. On inhabited AI worlds, distinguish a new company colony from
governing the existing population; decide whether further influence/acquisition can extend
control planet-wide. No automatic right to build around another player's planet is granted.

## Initial world size and joining over time

Plan for registered participants over the server's expected life, not just simultaneous
connections. Four connected clients is a network test size, not four lifetime home-region slots.
An initial capacity model is:

`eligible inhabited regions >= intended admitted players + late-join reserve regions + protected AI regions + acquisition pool`.

These are disjoint allocations, not settled counts. The reserve must not also be sold as
the acquisition pool. Several regions may share a planet and its station services. Port
construction eligibility is regional: a neighbour's port does not consume a region's initial
opportunity. Existing construction claims, onboarding reservations and control changes still
need an explicit compatibility policy; do not infer planet-wide authority from one district.
The selected starting-access policy requires an operational public station supporting the
starter ship and a physical delivery connection to the allocated region. Basic docking,
trading and regional delivery remain available; corporate players cannot revoke this baseline
access. Service charges and finite capacity still apply, with finite goods and spending funds.
New players must spawn connected to working station life support, with available service
capacity required at allocation. This pauses onboard oxygen/scrubber use. Initial starter
reserves are 30 kg oxygen and one fresh installed 30-occupant-day cartridge, granted once;
operating reserves use dedicated space outside its trade hold, while trade supplies and
loose spares occupy cargo volume. Storage dimensions, station provisioning and later outages remain open.
Private ports retain their commercial policies. Exact fees, capacity and starting funds still
need balancing; access alone does not establish profitable routes or affordable supplies.
Spare region count alone does not establish viable starts. All inhabited regions
continue their economies under AI policies
until allocated; reservation is an eligibility rule, not a frozen planet. A protected AI
minimum can preserve counterparties, but its size and sale restrictions remain proposed
balance decisions. At capacity, joining needs an explicit wait/expansion result rather
than stealing an existing player's region.

Starting regions need comparable viable opportunities, not identical production decks.
Evaluate imports, transport access, population needs, development headroom and location.
AI hubs can be more developed than player starts. Late joiners enter an evolved economy:
initial funds, access contracts or catch-up support, if selected, need authored sources
and limits. Do not quietly undo earlier players' development to equalise the map.

Keep the original two-venue scenario as a tiny engine/trading integration fixture. Add a
separate governance fixture with multiple independently governed regions on one planet,
one regional home per test player and AI neighbours. Capacity
measurements must count all simulated regions as well as planets, NPC companies, ships and queued projects,
including worlds with no human connected. No planet count or month-long server capacity
has been demonstrated.

## Acquiring additional planets

This historical heading now covers a portfolio of regional control rights; the regional
correction does not authorise purchasing a whole physical planet by buying one district.
Player-governed regions, including acquired regions, cannot be purchased or seized through
the AI-acquisition mechanic. Otherwise eligible AI regions on that same planet remain
purchasable; a sibling governor has no veto. Reserved onboarding regions remain ineligible.
Purchase transfers an eligible AI region's public decision rights under an explicit offer
naming its jurisdiction.
Proposed safeguards are a revisioned offer, current eligibility/affordability checks and
one atomic debit/control transfer at a step boundary. Two buyers racing cannot both pay
successfully. Retry, reconnect or recovery cannot duplicate payment or award control twice.
An offer becoming invalid produces a structured rejection without partial transfer.

Specify who receives the purchase price and how public obligations and ongoing projects
continue. The region's treasury remains public money; privately owned companies, ships and
stations retain their owners. Existing private inventories, orders, reservations and visitor
access need defined continuity. Population and its needs remain part of the continuing
regional economy. Voluntary player-to-player control transfers are a separate, unselected mechanic.

Every acquired region keeps its identity, parent planet, development history and permitted local
budget. Sibling regions remain independent. The company gains a portfolio of control rights
with per-region policies. Consider
administration and infrastructure costs as balance candidates without deciding a hard cap.
AI governors can continue execution under the new player's authorised policies; they must
not retain an independent purchasing right after control transfers.

## Frontier systems, terraforming and settlement

First prove a sufficiently large static world. Dynamic expansion is a separate later
milestone requiring physical inter-system access and persistent topology. New systems should
extend the reachable frontier without moving existing systems, changing their IDs, or
regenerating their populations and economies. New stars require usable links to the
existing graph, not just map markers. Trade/development access requires the selected
[stargate expedition](stargate-expansion-plan.md): government or corporate funding, physical
automated/unmanned FTL delivery and operational connection. Distance-based delivery uses
configurable speed/pacing over hours or days of server-running wall time, pausing in shutdown.
The owner controls gate access and fees, including rival refusal. When to generate/reveal
targets based on population, available land, exploration or operator action remains undecided;
it cannot replace the required access project.

Frontier development should usually be lower than the established region, with exceptions
left possible. An uninhabited world has no civilisation rank, population demand or functioning
market by default; it is not a zero-population Stone Age civilisation. Distinguish survey,
claim/control rights, habitable environment, life support, settlement and market opening.
Sealed habitats might allow settlement before full terraforming; do not force a universal
ordering before the environmental model is designed.

Colonisation needs supplied equipment/materials, funding, transport and a defined source of
settlers. Preserve physical delivery and record population transfers/sources; new colonies
cannot create cargo, labour or demand solely because a client opened their screen. Persist
claim races, projects, environment progress and failure/waiting states. Existing habitat and
terraform skills provide authored prerequisites/effects, not a complete settlement mechanic.

Freeze authored content/rules for a running world as before. Generated topology is versioned
world state, not arbitrary hot-reloaded balance data. Persist the exact generated definitions,
stable identity map, topology revision, expansion sequence and generator identity/seed as
needed. Restore compatible definitions/services before loading entities or in-flight travel.
Include their identity in the authoritative hash/recovery contract. A shipped galaxy path
or seed without a pinned generator/version is insufficient to reconstruct an expanded world.

Build an expansion candidate, validate it, then publish a consistent topology revision at a
controlled boundary. Define failure/recovery across physics registration, lane travel,
routing and persistence. Existing routes held by index must be rebuilt or resolved through
stable IDs; ships already travelling must still reach their original destinations. Bound
expansion work and client delivery so generating a frontier cannot stall the world indefinitely.
Prefer append-only additions initially; removals and topology rewrites are outside this slice.

## Capability map and demonstrated gaps

| Required mechanic | Existing public provision/evidence | Missing work and owner | Acceptance evidence |
| --- | --- | --- | --- |
| Governed idle world | Jettison `SimWorld::Step`, `WorldService`, commands, save/load/hash; Manifest world/effects components | Manifest control rights, independent budgets, saved project/policy state; Jettison neutral CPU host/build provision | AI and player projects progress with zero clients; replay and restore agree. |
| Scoped development | Manifest `ProgressionManager`, `CivilisationEffectEngine`, habitat/service skill definitions | Manifest separates physical planet, region, organisation and immutable definitions from per-owner/per-region tiers, projects and effect scope | Multiple regions on one planet retain distinct population, public/household funds, condition and control. A local project affects only its target; rejected spending leaves all treasuries unchanged. |
| Public tenders and corruption | Jettison fixed-step commands, saved/hashed world services and archives; no Manifest tender/bribery system was demonstrated | Manifest competitive bid/award/fulfilment lifecycle, government corruption trait and abstract influence expense after clean tendering | Honest and corrupt award cases, invalid bids, deadline boundaries, separate funds, exactly-once costs/payment and replay/recovery. |
| Purchase of AI control | Jettison command handlers, serialised state and deterministic execution | Manifest offer eligibility, asset/liability scope, atomic transfer and request ledger | Race, unauthorised buyer, changed offer, retry and restart tests. |
| Company power over institutions | Manifest companies/markets are planned; no government/media influence system was demonstrated | Manifest corporate holdings, contracts, leverage and rival response rules | A defined economic dependency permits a specific influence action; effects and limits survive replay. |
| Company-controlled stations | Jettison `Docks`, `RequestPad` and compatibility-aware insertion queues; no admission/priority hooks were found | Jettison reusable admission/refusal and queue-ranking seams, with startup `Park` bypass scoped; Manifest construction, corporate ownership, fees and contracts | Independent docking-policy consumer plus Manifest build-once, fee-once, deterministic priority, equal NPC/player refusal and saved-policy/queue tests. |
| Resource exploitation and puppet colonies | Manifest production, stockpiles, body effects and authored habitat skills; no complete colonial government or extraction loop was demonstrated | Manifest regional site eligibility, resource/project/colony schemas, company-control links and public procurement; reuse/provision public spatial/docking facilities | Eligible regions on one planet retain independent initial port opportunities and resource grants. Reject duplicate funded claims and unsolicited builds in other players' regions; allow competing frontier ports at expiry without transferring government control. |
| Remote TUI/GUI play | Jettison capture/shown-component snapshots and controller primitives | Jettison reusable transport/session/encoding provision; Manifest authentication policy, commands, filtering and frontend views | Connect/disconnect, private-view filtering, stale revision and retry tests without FTXUI/rendering. |
| More physical systems | Jettison `Galaxy`/`StarSystem` resolved YAML data, `PhysicsWorld::DefineStarSystem`, `LaneTravel::DefineStarSystem`/`SetGalaxy`, `PlotRoute` | Manifest frontier generation/policy; demonstrate whether existing composition suffices, then provision minimal reusable topology publication/identity gaps in Jettison | Independent two-to-three-system consumer plus Manifest expansion without broken transit. |
| Stargate delivery and controlled access | Galaxy positions, lane-based routing/transit, `Gates::SetTollRule`, commands/services/archives | Manifest funded unmanned FTL expeditions, running-time arrival and gate-owner policy; Jettison reusable ungated-transit, policy-context or topology provision where existing APIs do not suffice; see the stargate capability map | An initially unconnected destination opens only after delivered/operational gate, with zero-client progress, shutdown/recovery correctness and owner-controlled access/fees. |
| Persistent expanded world | Jettison `SimWorld::Save/Load/StateHash`, game save sections and reflected `OnLane` | Physics system definitions are not saved/hashed; Manifest must persist generated world data through SDK archives and bind its identity into recovery; generic missing archive/topology facilities belong in Jettison | Restart outside the source tree reproduces expanded topology, state hash and active journeys. |

Source inventory: Manifest [Simulation.h](../source/game/sim/include/manifest/sim/Simulation.h),
[Components.h](../source/game/ecs/include/manifest/ecs/Components.h),
[ProgressionManager.h](../source/game/progression/include/manifest/progression/ProgressionManager.h),
[CivilisationEffects.cpp](../source/game/effects/src/CivilisationEffects.cpp) and
[habitat definitions](../data/skill-tree/habitats.yaml). Engine evidence is in
`source/engine/celestial/include/jettison/celestial/{Galaxy,StarSystem}.h`,
`source/engine/ai/include/jettison/ai/{Routes,Lanes}.h`,
`source/engine/dock/include/jettison/dock/Docks.h`, `source/engine/dock/src/Docks.cpp`,
`source/engine/physics/include/jettison/physics/PhysicsWorld.h`,
`source/engine/physics/src/PhysicsWorld.cpp` and
`source/engine/sim/include/jettison/sim/{SimWorld,Snapshot,SaveGame}.h` in the selected SDK.

The inspected engine has registration and route-building pieces, not a demonstrated live
expansion protocol or public procedural galaxy generator. Name-derived system IDs coexist
with positional route indices; collision/rename rules need review. Do not present these
gaps as solved, require Manifest to use private app headers, or create a second custom
loader/physics scheduler. The implementer commits reusable engine provision before updating
Manifest's immutable pin and matching development environment.

## Delivery amendments and acceptance

Retain existing stage numbers as foundational work packages, with these planning amendments:

1. Host/content/authority: retain CPU-only composition and shared loaders. Resolve planet,
   company and public-budget identity before freezing authoritative schemas. Keep the small
   trading fixture; add governance definitions without implementing an entire political model.
2. Network/idle slice: move the TUI to real remote commands and snapshots after minimal
   authority exists. Prove server progress without clients, physical auto-trading, NPCs and
   reconnect continuity. The old optional administration-only TUI is superseded.
3. Regional development: multiple economies on one planet, one controlled home region per
   participant, AI neighbours, bounded
   saved development queues, meaningful local effects and separate public/company finances.
   Public procurement uses competitive tenders; add the simple corruption/bribery slice
   after the clean tender lifecycle works.
4. Persistent release: extend stage 5's journal/recovery contracts to control rights,
   project reservations, automation, government budgets and request outcomes. No production
   durability claim before its fault/restart evidence exists.
5. Corporate growth: eligible AI regional control purchases and per-region portfolio policies, then
   infrastructure/essential-supply/access-contract leverage and company-built stations with
   owner fees, shipping priority and rival denial, then physical resource exploitation and
   company-controlled colonies at eligible planets.
   Preserve player-region acquisition protection and late-join capacity; do not invent a win threshold.
6. Frontier: static multi-system navigation first; then deterministic, durable expansion,
   claims and one physical settlement/terraforming scenario. This does not block the first
   governed-planet idle loop. Broader political/media simulation follows proven mechanics.

Required scenario: multiple players receive distinct regions on one planet; AI neighbours run; one player
starts development and trading and disconnects; work progresses; a late joiner receives a
valid reserved region; reconnecting shows the same control and separate budgets. A later
scenario purchases an eligible AI region while refusing a player-region purchase, allowing
independent regional port opportunities on the same planet. A final
frontier scenario expands with an in-flight ship, restarts, supplies a colony physically and
continues from identical topology/project state. Each scenario checks unauthorised requests,
duplicate requests, bounded histories/queues, inventory accounting and deterministic replay.

The implementer should run long simulated workloads for short and extended server-life
profiles, measuring economic concentration, entrant viability, project/route costs, memory,
save growth and recovery time. Fast stepping tests determinism and game pacing but do not
prove month-long real-time availability. Include a real unattended soak separately. Profile
before changing route selection/indexes or adding simulation parallelism.

Use relevant presets and both repositories' required gates for touched implementation, with
two local compile jobs and no local clang-tidy for this cross-repository work. Validation
is limited to source/API inspection and documentation whitespace/link checks; no builds,
economy runs, benchmarks or recovery tests were performed for this assessment.

## Next planning cycle

The [contract/booking handoff](procurement-and-station-contracts-plan.md) proposes the first
clean tender lifecycle and station commitment boundary. The [economy/development plan](economy-and-development-plan.md)
records instant credits, physical commodities, brokers/handling, material/facility-gated tech
and core/frontier generation. Partial payment, booked terms and temporary running-time
concessions followed by rival ports are confirmed. Supplier-funded performance security is
selected with a starting 10% bond and proportional forfeiture. Funded construction-site
reservation, non-refundable mobilisation, a fixed completion deadline and reopening on
abandonment/expiry are also selected. Frontier exclusivity covers eligible resources in one
named region, starting at operational port plus concession activation, with a disclosed
provisional 72-hour server-running term. Neighbours retain independent eligibility and timers;
government control stays separate. Regional construction and shared service scope are selected;
owners may serve multiple regions on the same planet under their offered terms, with explicit
physical delivery, independent economies/rights and protected bookings. Detailed transport
remains open. Starting regions require continuing public access to basic docking, trading and
regional delivery, with an operational starter-compatible station and physical delivery route
at allocation. Require an available working life-support connection and spawn connected;
reject missing or exhausted service capacity, preserving the connection through retry/recovery
without duplicate spawn/resupply. Fees/capacity/funds still need balancing. Cultural styling, corporate branding and distinct customisable
station appearances are recorded for later presentation design. The first
[material/facility tech example](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
is Bronze metallurgy, using local copper, imported tin and an operational workshop. Detailed
settlement, construction amounts/deadline/salvage and long-running demand/public-income
circulation remain design work; selected quantities, throughput and the exclusivity term
still need economic/pacing validation.
Keep no-win open-ended play, company identity, player-region acquisition protection and late-join
capacity as constraints. Server pacing, detailed influence rights, acquisition pricing and
expansion triggers remain open choices.
