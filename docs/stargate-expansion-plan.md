# Stargate expeditions and opening new systems

Planning requirements, 2026-11-15. Extends [frontier planning](planetary-development-plan.md#frontier-systems-terraforming-and-settlement)
and the [physical economy](economy-and-development-plan.md). This records approved direction
and proposed delivery contracts, not implemented gameplay. Inspection used Manifest
`feature-base` at `066b5cc` plus the current planning edits, the pinned Jettison SDK
`f85092027eda49c29001f7589608df1f0b8827fb`, and the sibling's integration guidance at `0564f19`.
No implementation, assets, configuration, SDK change or build/gate execution occurred.

## Confirmed direction

- Opening a new system for trade and development requires a government or corporation to
  fund and physically ship a stargate across interstellar space.
- Delivery flights are fully automated and unmanned, travelling at a multiple of lightspeed.
  They progress without connected clients over a meaningful amount of server-running wall time.
- The user selected distance-based duration, with configurable FTL speed and pacing tuned
  to hours or days. The initial balance-testing rate is six server-running hours per light-year.
  Freeze each expedition's quoted rate and duration at launch; later tuning applies to future
  launches. The authored FTL/pacing values and unit convention must realise this selected rate.
- The user selected owner control of operational gate access and fees, including refusing
  rivals. This applies to government and corporate gate owners.
- The user selected one protected return crossing for the same ship after entry through a
  gate, even if the owner subsequently bans its company. Quote and reserve outward/return
  fees together before entry, charging each crossing once at transit commitment. Honour
  accepted terms after price increases or bans. The right has no time expiry during the
  destination visit; the ship's first departure closes that visit. Returning through the
  same pair consumes the right without creating another; another exit releases unused
  return funds. Detailed transfer and repair contracts remain unresolved.
- The user selected initial per-crossing energy/supply costs, deferring recurring idle upkeep.
  At admission the owner reserves physical supplies for the protected return until the visit
  closes. Shortages stop new admissions; genuine equipment outages preserve rights/funds until
  repair. Owners may close to new business immediately, but must honour accepted passages and
  returns. Dismantling waits for those obligations to end, potentially indefinitely because
  visiting ships' return rights do not expire.
- The user selected a fixed pair of gates for each connection, rather than one gate serving
  several destinations. Each endpoint belongs to its named connection; another destination
  requires another pair. A per-system gate limit and later retargeting/transfer rules are
  separate undecided matters.
- The user selected independent rival gate construction into already-connected systems,
  without incumbent gate-owner permission. Several independently owned pairs may serve a
  system, including parallel connections between the same systems. A rival may deliver its
  remote endpoint by its own funded unmanned FTL expedition despite an incumbent gate ban.
  Each owner controls only its own gates; protected planets and resource concessions remain
  unaffected.
- The user selected building both endpoints at an accessible origin, installing one and
  sending the other with the power, controls and equipment for unmanned autonomous deployment.
  Open the connection only when both endpoints are ready; show flight arrival separately
  from expected trade opening. Initially the sponsor owns both endpoints, as public assets
  for a government expedition or company assets for a corporate expedition.
- The user selected a capped, reserved project budget with payment for delivered equipment
  and completed services during preparation. Require a commissioned origin gate, complete
  physical flight/deployment equipment and funded remaining contracted services before launch.
  A launched mission needs no routine cash top-ups; overruns during preparation require an
  explicit budget increase. Pre-launch cancellation releases only uncommitted funds while
  preserving equipment ownership, paid work and awarded supply-contract obligations.
- The user selected an initial two server-running hours of remote deployment/commissioning,
  quoted before launch. Work starts after arrival when deployment equipment can operate;
  shutdown or equipment interruption pauses it, disconnection does not. Preserve completed
  work. The one-way carrier's useful power, controls and installation hardware become part
  of the remote gate, with the carrier's cost included in the expedition budget.

Shipping/activating a stargate, constructing a planetary spaceport, acquiring government
control and granting resource rights are distinct operations. The stargate does not itself
grant a planet, populate a colony or begin the selected 72-hour exploitation term. That term
still starts at an eligible operational spaceport plus its exploitation concession.

## Expedition lifecycle (proposed handoff)

Track funded preparation, launch, interstellar flight, arrival, deployment/commissioning and
operational connection separately. A prepared or in-flight gate is not a usable trade route.
Arrival must not publish a functioning lane before required endpoints and deployment are ready.
Use the selected fixed-pair construction/deployment model below; do not require a completed
destination gate to carry its own delivery. Equipment recipes, costs and origin construction
duration remain unresolved; funding stages and the initial remote commissioning term are
selected below.

The sponsoring government spends authorised public funds; a corporate sponsor spends company
funds. Keep ownership and obligations explicit. Public procurement rules apply when the
government tenders supply/transport; public financing does not silently transfer the gate to
the governor's company. Specify launch hardware, gate materials, costs and construction work
before claiming an affordable expedition. Gate equipment is physical committed property,
not spendable origin inventory while it is in flight or a free destination duplicate.

The unmanned carrier needs no passenger/settler source. Colonisation and inhabited demand
still require their own supplied settlement process. Autonomous propulsion, arrival and
deployment requirements need definitions rather than an implicit remote construction crew.
Pre-launch cancellation follows the selected funding rules below. Post-launch cancellation,
recall, failure, salvage, competing expeditions and ownership-transfer terms remain design
work. No random losses, combat or guaranteed exclusivity are selected here.

### Fixed-pair deployment and initial ownership (selected)

Build two gate endpoints at an accessible origin. Install one there; package the
other with the unmanned FTL carrier, power, controls and installation equipment needed for
autonomous deployment. The sponsor must obtain/pay for that equipment physically; destination
stock, workers and an existing gate are not assumed. Exact materials, construction recipes
and funding amounts are still to be designed.

At launch, bind the expedition, origin endpoint and remote endpoint to stable pair identities
and a destination. The carrier follows its quoted interstellar journey without a connected
player or manual piloting. On arrival, its carried equipment deploys and commissions the
remote gate; no crew or staffed destination port is a prerequisite for this first
connection. The selected commissioning/one-way carrier and initial operating rules are below;
exact supply and repair requirements remain unresolved.

Open an operational link only after both endpoints are installed, commissioned and ready.
If the remote endpoint arrives while its counterpart is unavailable, retain a visible blocked
activation state rather than publishing a usable lane. Arrival, commissioning and opening
have separate outcomes, and retries/recovery cannot create another gate or connection.
Disclose flight duration separately from unfinished construction/commissioning so the quoted
flight-arrival time is not presented as guaranteed trade opening.

For the first slice, the sponsor owns both endpoints: public ownership for a
government project, corporate ownership for a company project. This simplifies the initial
access/fee contract while preserving separate public/company accounts. Joint ownership,
independent endpoint sales and conflicting endpoint policies need later contracts. Exact
equipment costs, operating supply quantities and repair requirements remain design work.

### Launch equipment and funding (selected)

Authorise and reserve a capped budget for the gate pair and expedition. During preparation,
pay for delivered equipment and completed services from that commitment, retaining separate
public/company accounts and the existing partial-delivery procurement rules. The same credits
cannot remain refundable or available for another project after payment. If preparation would
exceed its authorised budget, pause until an explicit authorised increase; do not silently
overdraw or divert funds from another project.

Before launch require the origin gate to be commissioned and the remote gate, carrier, FTL
propulsion, power, autonomous controls, deployment equipment and all required consumables to
be physically ready. Reserve/assign actual equipment to the expedition; promised future
stock is insufficient. Fund any remaining contracted flight/deployment services before
departure. The launched mission needs no routine cash top-ups to carry out its provisioned
flight and deployment. Exact equipment quantities, construction recipes and credit costs
remain balance/design work; no new authored equipment IDs or numeric prices are selected.

Pre-launch cancellation releases uncommitted funds. Existing equipment remains the sponsor's
property, paid work remains paid and awarded supply contracts retain their obligations.
Cancelling this project does not create a right to terminate those contracts or refund their
settled deliveries. Asset collection, reuse and any future sale remain subject to their
physical location and existing ownership; the unused remote gate is not remotely delivered.
Post-launch recall/cancellation still needs a separate contract; initial gate upkeep is below.

### Commissioning and one-way carrier (selected)

Use two server-running hours of remote deployment/commissioning as the initial balance-testing
value, quoted before launch. Start work when the expedition has arrived and its deployment
equipment can operate. Count eligible server-running time using recorded host observations;
shutdown or equipment interruption pauses progress, while disconnection does not. Resume
saved completed work without restarting the full term or consuming another installation kit.
This is a running-time project, not a counter that faster simulation stepping can complete.

After remote commissioning, open the connection only when both endpoints are ready. An
unavailable origin endpoint delays opening; it does not undo completed remote work. Continue
to show arrival, commissioning progress and activation readiness separately. Actual equipment
operability and repair/maintenance requirements still need definition; no unselected random
failure or additional deployment charge is introduced by these rules.

Treat the carrier as a one-way deployment package. Incorporate its useful power, controls and
installation hardware into the remote gate, retaining them in the sponsor's installed asset
accounting. Include the carrier cost in the expedition budget. Installation does not leave
another reusable carrier or produce an automatic salvage payout. The initial two-hour term
and equipment recipe require balance testing; gate upkeep follows the selected model below.

## Distance and running-time contract

The selected initial flight rate is six server-running hours per light-year. Quote and freeze
each expedition's rate/duration at launch; subsequent tuning changes future launches only.

| Separation | Flight duration | Flight plus initial commissioning |
| --- | --- | --- |
| 2 light-years | 12 running hours | 14 running hours |
| 4 light-years | 24 running hours | 26 running hours |
| 8 light-years | 48 running hours | 50 running hours |

These are accepted starting balance-test values, not measured pacing results. Preparation
adds time, and interrupted commissioning or unavailable endpoints delay opening. The table
assumes uninterrupted two-hour commissioning with both endpoints otherwise ready; it does
not promise the opening time under those failures or change existing star positions.

Compute separation from stable galaxy-space coordinates, not an existing gate route: the
expedition establishes access where no operational lane yet exists. Record launch terms
containing origin/destination IDs, distance, FTL multiple, expedition pacing factor, conversion
version and quoted server-running duration. Freeze those terms at launch; setting changes
must not silently rewrite an existing expedition's arrival time.

A dimensional proposal is `running duration = distance / (FTL speed * expedition pacing)`.
Convert light-years and seconds with one versioned unit convention. FTL speed is a multiple
of light speed; pacing converts fictional travel time to elapsed server-running time. It is
a quoting parameter, not the current measured tick throughput or a client's playback speed.
Reject nonfinite values, FTL multiples at or below one, nonpositive pacing/durations and invalid
routes/distances. Specify
the zero-distance case rather than accidentally opening a second system instantly. With the
same speed/pacing, twice the distance takes twice the running time; no unselected duration
clamp or fixed expedition wait replaces that relationship.

Reuse the [host running-time bridge](economy-and-development-plan.md#real-time-exclusive-exploitation)
for recorded elapsed observations and ordered arrival events. Shutdown consumes no flight
time; disconnection does not pause it. Fast simulation stepping alone cannot complete it.
Keep wall-clock reads out of deterministic systems/replay. Persist elapsed progress with the
same authoritative state/journal batches as expedition state, preserving the existing bounded
crash-checkpoint-loss contract rather than counting downtime or promising exact crashed uptime.
At a due boundary, distinguish arrival from successful deployment; a blocked deployment is
visible and cannot invent an operational connection merely because time elapsed.

## Connection, ownership and frontier rights

System discovery/selection, generation, physical arrival, lane activation and economic opening
need distinct saved states. Target cataloguing/generation may occur before arrival; exactly
when to generate or reveal it remains undecided. Gate delivery is the required access project,
not a decision about automatic population/land-capacity generation triggers.

Activate a validated, consistent topology revision at a controlled authoritative boundary.
Existing systems/IDs, route destinations and journeys must survive the addition. Persist the
generated world data and link identities before acknowledging durable activation, restoring
them before entities/transit on restart. Reuse SDK definitions, archives, VFS and registries;
do not introduce a parallel galaxy loader. See the frontier plan for topology recovery gaps.

An operational government/corporate owner may set fees and refuse rivals. Save ownership and
policy revisions, validate payment/access at authoritative execution, and expose denied access
to auto-traders and route queries. Existing station-booking protections are not automatically
the gate-booking model: the user selected the permit and initial operating rules below, with
detailed repair/compensation and transfer contracts still to design. A policy update cannot silently
strand or confiscate existing ships/cargo without an explicitly selected rule.

### Gate permits and protected return (selected)

Entry through a gate includes one protected return crossing for the same ship, even if the
owner later bans its company. This applies to ordinary traffic through operational gate pairs,
not to the one-way FTL expedition carrier incorporated into a newly delivered gate.
Owner refusal remains available for new entry; it cannot revoke the selected return right.

Quote the outward and return fees together and reserve both before entry. Charge each crossing
once when its transit commits. Honour accepted passage and return terms despite subsequent
price increases or company bans; new permits use the current policy. A displayed quote or
pending request is not itself accepted permission. Fee amounts and detailed quote/acceptance
validation still need specification using the existing whole-credit ledger policy.

Bind the return right to the same ship and gate pair. It has no time expiry while that ship
remains in the destination system. Its first departure closes the visit: a protected return
through that pair consumes the right and creates no further return entitlement, while departure
through another route releases the unused return reservation. Track the paid visit explicitly;
a reverse arrival must not recursively mint another return right or an unlimited entry pass.
The owner may still refuse subsequent visits under its current policy.

The operating/closure rules below govern initial service unavailability; detailed repair,
compensation and ship/gate ownership transfers still need contracts. A saved permission does
not physically operate a broken gate. Keep the right, reservations, charged fees and terminal
outcome with authoritative ship/visit state and preserve them through disconnection/recovery.

### Operating supplies, outages and closure (selected)

Initially consume energy and supplies per crossing; defer recurring idle upkeep. Exact goods,
quantities and equipment recipes remain design/balance work. At admission the owner reserves
the physical supplies needed for the protected return and keeps them committed until the
visit closes. The passenger's reserved return fee is a financial commitment, not physical
energy or stock. The owner must have actual available resources to reserve without promising
the same supplies to several ships. Retire the physical reservation with the corresponding
visit outcome, consuming it for the return or releasing unused supplies when that visit ends
through another route.

Supply shortages stop new admissions while preserving resources reserved for returns. Genuine
equipment outages visibly block service and preserve outstanding rights and funds until
repair. Do not treat policy refusal or withdrawal of committed resources as a technical
excuse to revoke a return. Fault criteria, repair work, compensation and any future recurring
maintenance remain to be designed; no random breakdown system is selected here.

An owner may close a gate to new business immediately, while honouring accepted passages and
protected returns. Dismantling waits until those obligations are discharged. Because return
rights have no time expiry, retirement may remain pending indefinitely while a visiting ship
stays in the destination system. Closure does not impose a forced departure or confiscate
ships/cargo. Keep closure distinct from technical incapacity and persist pending obligations.
This retirement policy does not demonstrate safe live topology removal or select a salvage
payout; those require their own implementation/design contract.

### Independent rival connections (selected)

Governments and corporations may fund independent gate pairs into already-connected systems
without the incumbent gate owner's permission. Several independently owned pairs may serve
one system, including parallel connections between the same systems. Each pair has its own
endpoints, ownership and access/fee policy; another owner's gate is not shared automatically.

A rival can deliver its remote endpoint on its own unmanned FTL expedition, following the
same funding, launch-readiness, travel and commissioning rules. An incumbent's gate ban cannot
block that flight. The sponsor still needs an accessible origin and physically supplied
equipment; this is an independent investment, not free use of the incumbent's infrastructure.
Delivery of additional gate equipment through an existing operational network, gate-site
eligibility/placement and any capacity limits still need design.

Building another gate does not bypass protected planets or active planetary resource
concessions. Frontier concession expiry permits rival exploitation/ports without compelling
a private gate owner to carry rivals. Gate ownership grants no automatic planetary sovereignty
or system-wide construction veto. Independent connections let rivals challenge a gate monopoly
by investing capital and time; no affordable or profitable competition has yet been measured.

## Capability map and implementer evidence

| Required mechanic | Existing public SDK evidence | Gap and owner |
| --- | --- | --- |
| Distance and regular connected travel | `Celestial::GalaxySystem::position` is in light-years; `Ai::PlotRoute` follows existing lanes; `Ai::EnterLane` requires a defined lane | Manifest owns expedition choices, speed/pacing and finance. Existing lane travel cannot bootstrap its absent destination link; demonstrate suitable public movement provision or provision the smallest reusable ungated transport seam in Jettison. |
| Long-duration autonomous arrival | `SimWorld` commands/services, archives/hash; `Ai::OnLane::arrivalStep` is simulation-step timing | Manifest owns expedition state and recorded running-time arrival policy. Existing step-based `LaneTravel` is not evidence of a shutdown-aware running-time expedition. Reuse the host bridge; any missing generic movement/event capability belongs in Jettison. |
| Gate admission and fees | `Ai::Gates::SetTollRule` supplies refusal/charge callbacks; `Celestial::Galaxy` has a global distance toll | Manifest owns gate ownership, tariffs, funding and access. Demonstrate per-owner policy/fee context and coverage of transit entry paths; provision a minimal reusable SDK policy seam if needed, without using sample toll accounting as Manifest settlement. |
| Fixed-pair connection and recovery | `JumpLane` names two endpoints; `Celestial::Resolve` allows only one lane per jump point; `LaneTravel::SetGalaxy`, system registration and save/load exist | The static SDK graph supports fixed pairs, but does not prove physical endpoint manufacture, delivery or atomic live activation/recovery. Manifest owns pair/expedition identity and activation policy; reusable topology/identity gaps belong in Jettison. |
| Rival pairs in existing systems | `Gates::DefineStarSystem` rejects redefinition; `MakeGates` creates a system's gates together and rejects a repeated call | Adding further physical endpoints in an already registered system is not demonstrated by this public lifecycle. Manifest owns funded competing projects and site policy; Jettison must supply any missing reusable incremental definition/entity registration and recovery seam, preserving existing endpoints and journeys. |

Required evidence: a government and a company can fund distinct expeditions without mixing
accounts; missing equipment/funds reject launch atomically; an unmanned flight progresses with
zero clients; differing distances produce the quoted duration; shutdown pauses progress and
accelerated test stepping does not substitute for elapsed time. Use an injected host clock.
Replay/recovery must preserve equipment, funding, flight progress and an activation applied once.
Crashes around arrival/deployment/topology publication cannot duplicate a gate or expose a
half-connected route. Existing journeys still arrive. Owner bans and fees affect ordinary/NPC
traffic through the same authority checks, without creating extra resource/government rights.
Verify that each pair connects only its assigned endpoints and cannot be used to switch to an
unfunded destination. Additionally prove self-contained
remote deployment, no activation while one endpoint is unavailable, and government/company
ownership without account aliasing. Those required checks have not run.
Also cover budget reservation/payment without double spending, explicit overrun approval,
an uncommissioned origin gate refusing launch, complete onboard equipment/service funding,
flight/deployment without routine cash top-ups, and pre-launch cancellation retaining supplier
obligations and physical equipment ownership. No launch-cost or affordability result is claimed.
With an injected host clock, also cover eligible commissioning work, shutdown/equipment pauses,
zero-client progress, restart after partial work and unavailable-origin activation delays.
Flight and deployment phase accounting must not double-count elapsed work or installation
inputs. Completion incorporates carrier equipment once, without duplicating an installed
asset or a reusable ship. No commissioning-time or lifecycle result has been validated.
Gate-permit checks must cover outward/return fee reservation and one-time capture, price/ban
changes before and after acceptance, a return after a long destination stay, alternate-route
exit/refund, and reverse arrival without recursive rights. Include NPC/player parity and
restart/retry at acceptance and both crossing commitments. None of these checks ran.
Operating checks must additionally prove owner-side physical return reservations, no duplicate
stock commitments, shortage/closure refusing new entries while preserving old rights, genuine
outage/repair continuity, alternate exit releasing unused supplies and retirement blocked by
outstanding commitments. Exercise long stays and restart/retry; none of this evidence exists yet.

Prove two pre-existing systems plus one initially unconnected destination; then prove appending
a generated target and an independently owned parallel pair into an already connected system.
The incumbent's ban must not block the rival expedition or change the rival's policy, fees or
ownership. Existing journeys, gate obligations and planetary protection/concession state must
survive the added endpoints and recovery. Jettison provision needs an independent consumer exercising ungated arrival
and connection activation without Manifest's economy. Implementers run appropriate presets and
gates with two local compile jobs and no local clang-tidy. No such evidence has been produced.
