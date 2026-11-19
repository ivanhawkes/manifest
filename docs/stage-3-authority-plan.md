# Stage 3: companies, authoritative state and commands

Subsequent requirements: read [planetary/corporate planning](planetary-development-plan.md)
before freezing schemas and apply the [regional correction](regional-economy-and-governance-plan.md).
Players also direct one region's development/public budget; keep physical planet, region,
population/household/public accounts, government, company assets and control rights separate.
The first governance scenario must include multiple regions on one planet. The original
AI-only civilisation scenario below is a fixture, not a restriction on governed regions.
Networking/TUI play is now required; its connection slice follows minimal authority.

Planning handoff for Claude, prepared 2026-11-15. Depends on
[stage 1](stage-1-host-plan.md) and [stage 2](stage-2-content-plan.md).
This refines stage 3 of [development stages](development-stages.md), retaining the
[gameplay/server contract](gameplay-and-server-plan.md). It is a proposed design,
not implemented functionality. No builds or gates were run.

Apply the [engine/game boundary assessment](engine-game-boundary-assessment.md) before
binding durable hosted state: generic spatial identity and save mechanisms belong to
Jettison, while company ownership, permissions and retry policy remain Manifest's.

Inspection baseline: Manifest `feature-base` at `0c6e544`, Jettison `feature-base` at `f0b8a37`
(Manifest's SDK pin `30ea3f3`), refreshed 2026-11-19 after stages 1–2.

## Findings that determine the migration

- `ecs/EntityStore.*` owns its own EnTT registry, string-ID index and allocation counters.
  It cannot remain an independently owned authoritative registry alongside `SimWorld`.
- `ecs/Components.h` associates ships and orders with civilisations. `Stockpile`
  is keyed only by item; it has no venue or company boundary.
- `market/Marketplace.*` accepts a caller-supplied civilisation in an `OrderRequest` and
  a caller-supplied tick in `AdvanceTick`. Cancellation has no actor argument. Posting can
  match and settle immediately, and `ExecuteOrder` walks the book.
  `LoadSimulation` copies development simulation state into the ECS representation.
- `progression/ProgressionManager.*` combines immutable definitions with one mutable
  tier/resource pool. That pool must not serve every company or share a civilisation wallet.
- `ManifestConsole` (`source/tools/tui`) owns local simulation/progression state. Its library
  and tests are built only when `MANIFEST_BUILD_TUI` is on.
- Jettison's `CommandQueue` applies queued commands in insertion order at the beginning
  of `SimWorld::Step`, then runs systems and advances the clock. It does not provide
  actor authentication, request deduplication or durable acknowledgement.
- The engine's generic archives support reflected objects, sequences and maps keyed by a string
  or a registered enumeration, written in key order. They have no set or Manifest `Decimal`
  codec. Existing components must not be registered wholesale with an assumption that they
  are serialisable.

## Authoritative state model

Use the stage 1 host's single `SimWorld`. All gameplay mutations operate on that world's
registry or its owned, saved/hashed services. Immutable definitions come from stage 2's
validated content set. Hosted commands never mutate the development `Simulation` and
then copy its state into the world.

| State | Ownership and identity |
| --- | --- |
| Company | A distinct entity with stable company ID; one company per player principal initially. Membership belongs to world state; credentials do not. |
| Planet | Physical identity, environment and sites; parent of multiple economic regions, without another authoritative population or pooled treasury. |
| Region | Stable regional ID/planet link; population, household funds, needs/condition, public accounts, local development and policies. |
| Government/control | Explicit regional authority held/exercised by eligible organisations and principals. Corporate/bloc links cannot duplicate population or merge sibling accounts. |
| Civilisation | Existing authored cultural/technology affiliation; distinct from physical planet, region, public authority and company. |
| Treasury | Separate company, regional public and household accounts with explicit owners. Account identity includes owner kind, so equal text IDs cannot alias. |
| Ship | Explicit company ownership; hull, capacity and physical location. Civilisation affiliation is a separate field if needed for technology or permissions. |
| Venue inventory | One balance per owner kind/ID, venue ID and item ID. Company stock at two venues is two balances. |
| Cargo | One balance per ship/item; the ship's owner controls it. Cargo is not venue stock. |
| Order | Explicit account owner and venue, item, side, quantity, Decimal limit and lifecycle; a reservation names that order. |
| Progression | Immutable shared skill definitions plus explicitly scoped mutable tiers/exclusive choices: regional projects/infrastructure, company capabilities or declared shared civilisation technology. |
| Identity allocation | Saved world service with checked, non-reusing counters; rebuilt lookup indexes are derived, counters are not. |
| Request history | Saved world service containing actor request progress, pending/result records and deterministic retention state. |

<a id="home-region-allocation"></a>Allocating a player a home region creates a new `Polity`
organisation for that player and sets the region's `governorId` to it; the region is the first
ungoverned `playerHome` region in the world's region order: authored order in a scenario,
generation order on a server, whose supply of free regions is the
[roadmap's](roadmap.md#region-supply). The player's trading company stays a separate
organisation ([D-075](decision-ledger.md#d-075)).

Prefer separate reflected inventory/cargo balance records or components to opaque
maps hidden inside public components. If a service retains maps internally, serialise
explicit sorted record vectors through engine archives. Rebuild derived indexes after
load and reject duplicate keys. Persist every value needed to reproduce future behaviour.

Store Decimal prices in saved records and command payloads as their signed 64-bit raw
microcredit value, using checked conversion at the boundary. Do not serialise their
`ToDouble()` representation. Quantities and physical values remain double, money balances
remain checked whole-credit integers. Preserve authored IDs and YAML keys; changing
runtime ownership fields is not permission to rename content identities.

<a id="registry-owner"></a>Settled in A02 (2026-11-19): hosted state lives directly on the engine
world. The `world` module registers its components on the session's `SimWorld` and adds one world
service, `Manifest::World::Identities`, which owns the saved ID counters and the derived lookup from
a kind and ID to its entity; the world owns the service for its lifetime, and every load rebuilds
the lookup from the loaded entities, refusing a duplicate. `EntityStore` is not adapted: it stays
the owning store of development and tests, and the market and effects code it serves move onto the
world's components when their hosted forms are built, starting in Stage 4. It never enters the
hosted authority graph.

## Inventory, reservations and capacity

Keep one accounting convention: reservations remove units/credits from available
balances and hold them in an order escrow record. Report available and reserved amounts
separately; total holdings are their sum. Cancellation refunds the remaining escrow
to the same owner and venue. Stage 4 settlement consumes escrow exactly once and
credits purchased goods to the buyer's inventory at that venue.

Loading moves available company inventory at the ship's docked venue into that ship's
cargo; unloading performs the reverse. Neither operation changes ownership. Reserved
goods cannot load, and cargo cannot back a venue sell order. Reject nonfinite/nonpositive
quantities, wrong location, insufficient available stock, overflow and capacity excess
before making any writes. Preflight both source and destination arithmetic. Deterministic
clamping of arithmetic residue, if needed, must have one documented rule and tests;
never use a tolerance to permit a material overdraft.

Use the existing content units and packing semantics for capacity. Settled in A02: an item's
occupied volume is its quantity over `ItemNode::unitsPerCubicMetre`, which already folds in the
unit, density and packing, against the hull's `CargoVolume` in cubic metres. The one owner is a
function beside `ItemNode` in `config`, added with A04's cargo commands and tested for each
supported unit; nothing computes it yet. Do not treat every item unit as one cubic metre. Derived occupied capacity is rebuilt from authoritative cargo and
validated definitions; cargo and a second mutable capacity counter must not drift apart.

Docked location comes from authoritative state, not a client flag. Jettison already
exposes `Dock::Docking`, its `Docked` stage, station/pad links and `Docks` services.
Use these where the host has installed docking. Stage 3 may initialise a ship in a
validated docked scenario state without implementing travel, but it must not expose
a player command that sets an arbitrary docked venue. Stage 4 owns departure/arrival.

## Command boundary and ordering

Separate transport/local caller context from the reflected command delivered to the
world. A trusted local harness supplies actor context for now; no network authentication
is claimed. The host resolves principal, permissions and company membership rather
than accepting a claimed owner as authority. Administrative commands have explicit
capabilities and cannot use player payload fields to bypass ownership checks.

Proposed envelope: world identity, actor identity, actor request sequence, command
kind/version, canonical payload, server-assigned execution step and global sequence.
Use separate reflected payload types for operations. Client input supplies neither
execution step nor global order. Bound payload sizes and queue occupancy at admission.

One host-owned admission path serialises requests and assigns increasing global
sequence numbers. Requests admitted before the step boundary enter that next step in
sequence order; requests arriving after it wait for the following step. Never let an
unordered container or parallel handler completion determine application order.
The engine clock's value before `Step()` advances is the execution-step label.

At execution, resolve targets again and recheck current membership, ownership, docking,
resources and operation preconditions. Earlier commands in the same step may consume
funds, change capacity or remove a target. Admission cannot promise execution success.
Use expected revisions only for commands that need stale-state rejection; do not require
a client to guess a future server tick. Define revision increments alongside those commands.

Expected invalid actions return structured reason codes, not exceptions. Example codes:
`NotAuthorised`, `UnknownTarget`, `WrongVenue`, `NotDocked`, `InsufficientFunds`,
`InsufficientStock`, `CapacityExceeded`, `InvalidQuantity`, `InvalidPrice`,
`StaleRevision`, `RequestConflict`, `RequestTooOld`, `QueueFull`, `UnsupportedCommand`.
Keep user-facing text in the frontend. Internal invariant failures stop normal processing;
they must not be reported as a harmless rejection after partially mutating a world.

## Retry and result contract

Proposed request identity is `(world ID, actor ID, monotonically increasing request
sequence)`. Each actor submits contiguous sequences starting from the advertised next
value. Reject gaps without consuming a sequence. Different actors may progress
independently. The host advertises progress on reconnect; changing a connection does
not reset the sequence. Counter exhaustion fails explicitly rather than wrapping.

- An identical pending retry returns the existing queued receipt, with no second enqueue.
- An identical completed retry within retention returns its recorded result.
- Reusing a retained request ID with a different canonical payload is `RequestConflict`.
- Once a result is pruned, any sequence below the retained range is `RequestTooOld`;
  it is never treated as a new request. Preserve a high-water mark independently of results.
- Malformed/unauthorised admission and `QueueFull` consume no request sequence.
  Admitted commands consume their sequence even if execution rejects their action.
- Retain a fixed bounded count of completed results per actor, proposed 256, plus bounded
  pending requests. Prune deterministically by sequence, never by wall time. Treat the
  configured limit as part of the command-state contract.

Return a queued receipt after admission and a final success/rejection after execution.
Neither response is a durable acknowledgement in stage 3. Stage 5 will journal accepted
ordering before acknowledging durable commitment. Preserve canonical payload/version,
assigned ordering, pending requests, high-water marks and retained outcomes in the save
design now; test in-memory save/load continuity without claiming crash recovery.

The retry ledger must be reconstructed by replay as well as normal admission. Define
an internal replay application path that consumes recorded envelopes without allocating
new sequence numbers or authenticating a live connection. Handle pending requests in
snapshots so replay cannot enqueue them twice. Reject inconsistent restored ledgers/queues.
This is an integration seam requiring tests, not a capability supplied by CommandQueue.

<a id="admission-a03"></a>Settled in A03 (2026-11-19), in the `command` module. A request is one
reflected envelope, `Manifest::Command::Request`: the actor's kind and ID, its sequence, the global
order admission assigned, the command's name and version, and its payload as the binary archive's
canonical bytes, which an identical retry is compared on. `Requests::Admit` is the one admission
path; it queues the envelope on the world's `CommandQueue`, whose saved queue holds the pending
work. An actor is a `Principal` that must exist in the world, or the `Administrator` the trusted
harness makes, and each command names the one kind that may send it. The limits are 256 retained
results and 32 pending requests per actor, 1,024 queued commands and a 4,096-byte payload. Besides
the plan's codes, admission refuses `MalformedPayload`, `PayloadTooLarge`, `SequenceGap` and
`SequenceExhausted`, consuming no sequence. A replayed envelope advances its actor's ledger where
it runs, and one already completed is not run twice. The world identity in the envelope waits for
a host that runs more than one world.

<a id="provisioning-a04"></a>Settled in A04 (2026-11-19), in the `company` module. Three commands, each
version 1. `Manifest.Company.Provision`, the administrator's, takes a principal's ID and the names
it shows, and makes the `Principal`, a `Company` issued `Company<n>` with its `CompanyAccount` at
`company.startingCredits` ([D-077](decision-ledger.md#d-077)), a `Polity` governing the first
ungoverned home ([D-075](decision-ledger.md#d-075)), and one `Ship` issued `Ship<n>` of the
scenario's starter hull, docked at the region's home venue: the first public, operational station
with life support that serves it and has a pad the starter fits, which the content set records
(`ContentSet::homeVenues`) by the check C09 made. A principal the world has is `AlreadyProvisioned`,
so a retry under a new sequence mints nothing; with no home free it is `NoHomeRegion`. A ship's dock
is its `Ship::dockedAt`, a venue's ID, which only provisioning sets: the engine's `Dock::Docking`
needs physics and a station body, which W05 brings. `Manifest.Company.Load` and `Unload`, a player's,
move a quantity between the company's `Stock` (a sorted `Holding` per venue and item, its available
and reserved quantities) and the ship's `Cargo` (a sorted `Lot` per item), naming the venue the
sender believes the ship is at. Every check runs before any write, with the plan's codes plus
`InvalidQuantity` for a quantity whose sum would not be finite. Occupied volume is
`Core::OccupiedVolume` over the hold against the hull's `CargoVolume`, with no tolerance, so a load
that fills the hold exactly can be refused by its last bit. A load refuses a save whose stock or cargo
is out of order, held twice, empty or not finite. Inventory comes from fixtures alone; nothing a
player sends makes it. Life-support connections at the home station are not counted (W06).

## Stage 3 executable scope

Deliver real company provisioning and cargo load/unload commands against the validated
scenario, plus structured queries. Company creation is a privileged operation, assigns
one ship and a bounded configured initial treasury, and is retry-safe. Tests may provision
inventory explicitly through controlled fixture/bootstrap setup; players cannot mint it.

Define place/cancel order, upgrade and travel command contracts for stage 4, but register
them only when their full semantics exist. Unsupported operations fail explicitly. Do
not expose current immediate-settlement methods as a temporary player API or report
success for an order that cannot yet match. This keeps stage 3's meaningful acceptance
slice at ownership, fixed-step cargo operations and command reliability.

Order/cancellation authority rules for stage 4: the actor must own the order account;
an absent or inaccessible order returns a result that reveals no private owner information.
Retries use the request ledger, not the current helper's permissive unknown-order no-op.
Production accounts controlled by civilisations use the same reservation/settlement rules
under server authority. Venue fee income belongs to its configured account, not the player.

Stage 4 must also separate company upgrade payment from the current generic resource pool;
an approved credit purchase must not silently spend civilisation resources. Preserve
existing development progression behaviour until its hosted equivalent is explicit.

## Queries and tooling

Provide value snapshots carrying completed step/revision, with stable ordering and
actor-filtered company balances, venue inventory, ships/cargo and command results.
Market summaries obey the [fleet-observation contract](market-information-plan.md): exact
current data only at stations with an observing fleet member docked, last-known reports
elsewhere and one-time starting reports. Another company's private inventory and treasury
remain private. Apply filtering before replication; return no mutable registry references
or component pointers to clients.

<a id="queries-a05"></a>Settled in A05 (2026-11-19), in the `query` module. `Query::Observe` takes the
world, its identities and an actor, read-only, and copies out a `Snapshot`: the clock's step, then the
actor's company (its ID, name, credits and holdings, and each ship's hull, dock and cargo), sorted by
ID, with nothing in it pointing back into the world. A player sees only the company their principal
runs, none before provisioning, and the `Administrator` every company. It is called between steps, so
it reads completed state only. A command's result is `Requests::Outcome`, already the sender's alone.
The observation boundary: the snapshot holds no market and no other company's stock or credits;
market rows stay out until W07 adds the fleet-observation filter, and a later market view goes in the
snapshot behind that filter, not beside it. Regions, their accounts and the player's polity are not
yet in it.

Split command/query services and their tests from terminal presentation. FTXUI remains
optional. Preserve the development console's established workflows and tests; distinguish
its local sandbox from hosted commands until each workflow has an authoritative equivalent.
Do not silently start a second simulation behind a hosted frontend.

## Delivery and acceptance

1. Add reflected hosted state, distinct owner identities, world-owned counters and
   explicit serialisation records. Prove one-world ownership and index rebuilding.
2. Add the trusted admission API, bounded sequencing/retry ledger and structured results.
   Wire recorded envelopes through the engine fixed-step queue.
3. Implement company provisioning and atomic cargo commands with shared rule owners.
   Add immutable filtered queries and a nonterminal command harness.
4. Decouple tests/services from FTXUI and preserve development-tool coverage.
5. Record actual validation and update architecture/roadmap; hand stage 4 the order,
   upgrade and travel contracts plus the unresolved integration details below.

Required checks for the implementer:

- Company/civilisation balances stay distinct, including colliding textual IDs; one
  player cannot operate another's ship or inventory, or mint resources through provisioning.
- Two regions on the same planet retain independent public/household funds, population,
  condition and project permissions. Shared civilisation, corporation, government or bloc
  membership grants only explicit authority and never duplicates accounts or people.
  Player provisioning allocates one regional control right, not a whole physical planet.
- No action takes effect before the assigned step. Conflicting commands execute in
  assigned order; second-command validation sees first-command effects.
- Rejected actions preserve business state, reservations and identity allocation. Clock
  advancement and recording a rejection are expected changes; do not compare whole-world
  hashes as if processing an invalid command were identical to doing nothing.
- Valid load/unload conserves quantities between the specific venue and ship; wrong-venue,
  reserved-stock, capacity, nonfinite and overflow cases make no business-state writes.
- Identical retries execute once; conflicting payloads, gaps, stale requests and queue
  bounds have stable outcomes; pruning cannot enable re-execution.
- Save/load and replay preserve state, counters, pending work and retry outcomes with
  deterministic hashes across Debug/Release/sanitised builds. Authentication credentials
  and frontend state are absent from snapshots.
- Queries enforce access restrictions and cannot mutate state; command coverage runs
  with `MANIFEST_BUILD_TUI=OFF`. Existing relevant suites remain green.

Run required repository gates in the locked development environment with each machine's
concurrency (D-051), local clang-tidy opt-in. OpenAI planning performs none of that
implementation work.

Before coding cargo operations, settle the concrete registry-adapter choice, the item
capacity conversion owner and the minimum docking bootstrap against the stage 1–2 code.
Do not defer authority or numeric invariants to those choices. Network transport,
authentication, durable journals, full market integration and travel remain later stages.
