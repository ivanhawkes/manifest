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
  to hours or days. No exact speed, pacing factor or duration range has been chosen.
- The user selected owner control of operational gate access and fees, including refusing
  rivals. This applies to government and corporate gate owners.

Shipping/activating a stargate, constructing a planetary spaceport, acquiring government
control and granting resource rights are distinct operations. The stargate does not itself
grant a planet, populate a colony or begin the selected 72-hour exploitation term. That term
still starts at an eligible operational spaceport plus its exploitation concession.

## Expedition lifecycle (proposed handoff)

Track funded preparation, launch, interstellar flight, arrival, deployment/commissioning and
operational connection separately. A prepared or in-flight gate is not a usable trade route.
Arrival must not publish a functioning lane before required endpoints and deployment are ready.
The exact pairing/network design, origin-end construction and autonomous deployment equipment
remain to be specified; do not require a completed destination gate to carry its own delivery.

The sponsoring government spends authorised public funds; a corporate sponsor spends company
funds. Keep ownership and obligations explicit. Public procurement rules apply when the
government tenders supply/transport; public financing does not silently transfer the gate to
the governor's company. Specify launch hardware, gate materials, costs and construction work
before claiming an affordable expedition. Gate equipment is physical committed property,
not spendable origin inventory while it is in flight or a free destination duplicate.

The unmanned carrier needs no passenger/settler source. Colonisation and inhabited demand
still require their own supplied settlement process. Autonomous propulsion, arrival and
deployment requirements need definitions rather than an implicit remote construction crew.
Cancellation, recall, failure, salvage, competing expeditions and ownership-transfer terms
remain design work. No random losses, combat or guaranteed exclusivity are selected here.

## Distance and running-time contract

Compute separation from stable galaxy-space coordinates, not an existing gate route: the
expedition establishes access where no operational lane yet exists. Propose launch terms
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
a settled gate-booking model: quote acceptance, policy-change timing, in-flight arrivals,
exit rights and charging terms still need design. A policy update cannot silently strand or
confiscate existing ships/cargo without an explicitly selected rule.

Frontier concession expiry permits rival exploitation/ports; it does not compel a private
gate owner to carry rivals. Independent gate construction, alternative links and the ability
to reach reopened worlds under gate denial need explicit rules. Do not turn gate ownership
into automatic planetary sovereignty or assume a one-gate-per-system monopoly was approved.

## Capability map and implementer evidence

| Required mechanic | Existing public SDK evidence | Gap and owner |
| --- | --- | --- |
| Distance and regular connected travel | `Celestial::GalaxySystem::position` is in light-years; `Ai::PlotRoute` follows existing lanes; `Ai::EnterLane` requires a defined lane | Manifest owns expedition choices, speed/pacing and finance. Existing lane travel cannot bootstrap its absent destination link; demonstrate suitable public movement provision or provision the smallest reusable ungated transport seam in Jettison. |
| Long-duration autonomous arrival | `SimWorld` commands/services, archives/hash; `Ai::OnLane::arrivalStep` is simulation-step timing | Manifest owns expedition state and recorded running-time arrival policy. Existing step-based `LaneTravel` is not evidence of a shutdown-aware running-time expedition. Reuse the host bridge; any missing generic movement/event capability belongs in Jettison. |
| Gate admission and fees | `Ai::Gates::SetTollRule` supplies refusal/charge callbacks; `Celestial::Galaxy` has a global distance toll | Manifest owns gate ownership, tariffs, funding and access. Demonstrate per-owner policy/fee context and coverage of transit entry paths; provision a minimal reusable SDK policy seam if needed, without using sample toll accounting as Manifest settlement. |
| Runtime connection and recovery | `LaneTravel::SetGalaxy`, system registration, `Celestial::Resolve` and save/load | No demonstrated atomic live publication or persisted generated topology contract. Manifest owns generated world data and activation policy; reusable topology/identity gaps belong in Jettison, as already recorded in the frontier assessment. |

Required evidence: a government and a company can fund distinct expeditions without mixing
accounts; missing equipment/funds reject launch atomically; an unmanned flight progresses with
zero clients; differing distances produce the quoted duration; shutdown pauses progress and
accelerated test stepping does not substitute for elapsed time. Use an injected host clock.
Replay/recovery must preserve equipment, funding, flight progress and an activation applied once.
Crashes around arrival/deployment/topology publication cannot duplicate a gate or expose a
half-connected route. Existing journeys still arrive. Owner bans and fees affect ordinary/NPC
traffic through the same authority checks, without creating extra resource/government rights.

Prove two pre-existing systems plus one initially unconnected destination; then prove appending
a generated target. Jettison provision needs an independent consumer exercising ungated arrival
and connection activation without Manifest's economy. Implementers run appropriate presets and
gates with two local compile jobs and no local clang-tidy. No such evidence has been produced.
