# Stage 5: persistence, recovery and a relocatable release

Subsequent requirements: [planetary/corporate planning](planetary-development-plan.md)
adds control rights, separate public budgets, development reservations/queues and idle
automation to authoritative recovery as they land. Later acquisition and generated frontier
topology need the same guarantees. A remote TUI slice can precede this stage; durable
production acknowledgements still require the recovery evidence below.

Apply the [regional correction](regional-economy-and-governance-plan.md): persist physical
planet/region relationships, regional population/household/public accounts, needs/condition,
government/control and organisation links, local development and employer-region tax carry.
Replay and migration must conserve totals without copying an old planet economy into every
region. Recovery cases must include independently governed regions sharing a planet/station,
not just economies on different planets.

Planning handoff for Claude, prepared 2026-11-15. Depends on the completed
[local trading loop](stage-4-trading-plan.md) and retains the
[gameplay/server persistence contract](gameplay-and-server-plan.md).
This is a proposed design, not an implementation or durability claim.

Inspection used Manifest `feature-base` at `2716380` and Jettison `feature-base`
at `826c4bec125b6631a6ba8ee822c90463316c3511`. Refresh both after stages 1–4.
No implementation, builds, recovery experiments or gates were performed for this plan.

## Existing facilities and required additions

Reuse Jettison's `SimWorld::Save/Load`, component/service migrations, binary archives,
`SaveGame` sections, VFS writes and `Core::WriteFileDurably`. The last writes a sibling
partial file, fsyncs it, renames it and fsyncs its parent directory. It is a whole-file
replacement API, not an append journal. No public journal coordinator was found in the
inspected simulation/core interfaces.

`SaveInfo` contains engine version, step and mod versions, but these do not enforce
Manifest's exact content/native-code compatibility. `ModDifferences` reports differences;
Manifest must decide whether startup is allowed. `SimWorld::Load` validates before
replacing state and invokes service commit callbacks, but that is not a transaction
across the entire host and its content.

Add a Manifest recovery coordinator around these facilities. Keep reusable durable I/O
extensions in Jettison only where a concrete gap requires them. Game journal records,
compatibility policy and acknowledgement semantics belong to Manifest.

## State and compatibility contract

The proposal; [R01](#save-and-compatibility-contract-r01-2026-11-24) defines the section and the
inventory.

Every authoritative value belongs in registered world state or a world service: account
balances, venue inventory, cargo, reservations, orders, fills needed for decisions,
company/civilisation progression, allocation counters, command sequencing/retry state,
regional population and household/public funds, condition, control, projects and payroll jurisdiction,
production cadence, RNG and in-flight engine/Manifest travel state. Rebuild lookup
indexes and other derived caches. UI state, credentials and wall-clock timestamps are
not authoritative simulation state.

Use `SaveGame` for snapshot payloads. Add a versioned Manifest section containing world
identity, application/game revision, SDK revision, command/schema versions, fixed-step
configuration, the exact stage 2 canonical content manifest and its algorithm/version,
and the journal boundary represented by this snapshot. Save headers, section schemas,
components and commands evolve independently through named engine migration chains.

Initially require the recorded compatible build/schema combination and exact content
manifest. A 64-bit fingerprint or `mod.yaml` release string alone is insufficient.
Directory and pack representations may differ while their effective manifests match.
Refuse mismatched content before installing a world; never silently reprice, replace
definitions or drop unknown state. No live content reload is supported.

Migration is an offline operation against a copy, with a verified backup and explicit
supported source/target versions. Migrate outstanding journal commands as well as
snapshot state when their formats change. Do not promise downgrade support or permit
an old binary to mutate a migrated world. Test migration with fixtures when real
versions exist; test-format migration coverage does not establish production support.

## Save and compatibility contract (R01), 2026-11-24

What a Manifest save holds and what a build checks before it loads one, for R02 to R09 to build;
where the proposal above differs, this holds. Read from Manifest `feature-base` at `3f97171` on pin
`14fc23a`, the inventory taken from a hosted trading world's own save: 50 component storages and 3
services, 461,719 bytes at step 0. A planning task: no code, and nothing below is enforced yet.

### The file

A snapshot is one Jettison save game (`Sim::SaveGame`, `Jettison.Save` 2): its `SaveInfo`, the
world's save (`SimWorld::Save`, `Jettison.World` 5) and one section of Manifest's, `Manifest.Save`.

- **`SaveInfo`** is for a list and a report, never the check: the engine's version, the clock's step
  and time, when it was written by the wall clock (the caller's, no part of the state), the packages
  mounted with their `mod.yaml` releases, and two details, `scenario` and `content`, the
  fingerprint. Packages that differ are reported (`Sim::ModDifferences`) and not refused: a release
  string does not change a rule, and the manifest below holds what does.
- **The world's save** names every component storage with its version first, then holds the clock's
  step and time, the generator's state, the queued commands, the entities, each storage and each
  service. `SimWorld::Load` refuses by name, leaving the world as it was, a component at a version its
  chain does not reach, a component or service the world does not register, a service with no state
  and a queued command the world does not apply. The section repeats none of that.
- **`Manifest.Save`**, version 1, is a reflected record in the binary archive's layout:

| Field | Holds | Compared with |
| --- | --- | --- |
| `world` | The world's identity: 128 bits from the operating system as the world is created, in hex. Never from the simulation's generator, and no part of the state or its hash. | The recovery catalogue's and each batch's (R03, R04). |
| `game` | The Manifest build: its version and its source's revision. | The running build's. |
| `sdk` | The Jettison revision the build was made on. | The running build's. |
| `fixedStep` | The step in seconds, which the world's save does not hold: a save made at 1/60 s loads today into a world stepping at any other. | The hosted world's, to the bit. |
| `commands` | Each command registered on `Command::Requests`, its name and version, in name order: 22 today ([Stage 4's handoff](stage-4-trading-plan.md#handoff-to-stage-5)). | The running registrations. |
| `content` | The canonical content manifest's bytes (`Content::ContentIdentity::manifest`, `Manifest.ContentManifest` 1), which carry their own format and version. | The loaded content's, byte for byte. |
| `batch`, `batchId` | The last journal batch this snapshot has applied: its sequence and the identity R03's chain gives it, 0 and empty for none. | The catalogue's chain (R04). |

The global command order needs no field: it is `RequestLedger::order`, in the world's save. Neither
does a schema version: each storage's is in the world's save, and the world's and the save game's
are their formats'.

### Compatibility

A build loads a save only when `game`, `sdk`, `fixedStep`, `commands` and `content` each equal its
own ([D-111](decision-ledger.md#d-111)), checked from the section before the world's save is read,
so a refusal installs nothing. The refusal names the field and both values: for `commands` the
first that differs, for `content` both fingerprints and the first definition type and record that
differ.

The build is compared exactly because nothing else tells two builds apart. Formats change in place
until player testing ([D-109](decision-ledger.md#d-109)), so equal versions do not mean equal
layouts or equal rules, and a journal replayed under other rules gives other outcomes than the ones
it acknowledged. So a world does not outlive the build that hosts it until D-109 ends. From then a
build names the earlier builds whose saves a migration brings up, offline and against a copy, as
[above](#state-and-compatibility-contract).

Two builds whose simulation could differ never share an identity. Today the executable knows only
the project's version, 0.0.1, and the SDK pin is a CMake variable: R03 selects how both revisions
are compiled in, a build from changed sources included.

### The state inventory

Every authoritative value is in a registered component or a world service; registration order is the
save's. Manifest's 32 components (`World::RegisterWorld`, all at version 1), by the entity that
carries them:

| On | Components | Holds |
| --- | --- | --- |
| Every hosted entity | `Identity` | Its kind and ID, never changed. |
| A principal | `Principal`, `Reports` | The player or NPC; their market reports by venue, each with its source (docked or starting), the step it was observed at and its window. |
| A company | `Company`, `Stock`, `Paid` | Its principal; its goods by venue and item, available and reserved; the last price it paid for each item. |
| An organisation | `Organisation` | Its kind and the principal who directs it, empty for the AI's. |
| A region | `Region`, `Knowledge`, `Facilities`, `Development` | Its planet, civilisation and governor, population, health and happiness; its skills and the recipes unlocked there; its facilities; its governor's standing development policy. |
| An account | `Account` | Whole credits, one entity for each kind and owner: company, public, household, employer, broker, venue, a tender's reserved budget and a contract's bond. |
| A ship | `Ship`, `Journey`, `Cargo`, `LifeSupport`, `Fuel`, `Refuel`, `Systems`, `Arrears`, `Policy`, `Job` | Its company, hull and venue; its journey's leg; its lots and opened totes; its reserves and tanks; the systems bought for it; what it owes its venue; its standing policy and the job working it. |
| A broker, an employer, a household | `Broker`, `Stock`, `Production`, `Pantry` | A broker's operator; each one's goods; an employer's batches, their progress and wages paid, and its tax carry; a household's opened totes, unmet needs and pressure. |
| A market | `Sales`, `Fees`, `Pricing` | The window's fills and each item's last sale; the posted charges; the fee level. |
| An order | `Order` | Its terms, what remains, its escrow, its expiry step and the request that placed it. |
| A project, a tender, a bid | `Project`, `Stock`, `Tender`, `Bid` | A project's phase, work done, place in its region's queue and delivered inputs; a tender's terms, steps, award and deliveries; a bid's total, revision and the step and order it was accepted at. |

The 18 that `Travel::Register` attaches after them are a ship in space: `Sim.Transform`,
`Sim.InFrame` (2) and `Sim.ControlledBy`; `Physics.Body` (2), `.Velocity`, `.OnRails`,
`.LocalFrame`, `.Handling` and `.FlightControls`; `Ai.Steered` and `Ai.Destination` (2);
`Dock.Vehicle`, `.Platform` (2), `.Berth` (3) and `.Docking` (2); and Manifest's `Space.OnLane`,
`Space.CruiseDrive` and `Space.Cruising`. A journey's leg is Manifest's; where the ship is within it
is these.

The three services, in the order they are added: `Manifest.Identities`, the count of IDs each
issued kind has handed out, so none is reused; `Manifest.Requests`, the ledger below; and `physics`,
Jolt's bodies and contacts.

What the delivery order names, each where it is saved:

| Required | Saved as |
| --- | --- |
| Request outcomes | `Manifest.Requests`: the last global order, and for each actor the next sequence, the floor, up to 256 completed requests with their bytes, step and result, and up to 32 pending. A pruned sequence stays below the floor, so it is too old after a restart as before it. The limits (`command/Request.h`) and `Command::Result`'s values are the build's. |
| Observations | `Reports` on the principal and `Sales` on the market. A starting report is a report as any other, its source `Starting`, so a load gives none again. |
| Regional rights | `Region::governorId` and `Organisation::principalId`; `Development`, `Knowledge`, `Facilities`; each `Project`'s phase and queue place; `Tender` and `Bid`; the public, tender and bond accounts. |
| Cadence | The clock's step, and steps held in state: an order's expiry, a tender's closes, a job's retry, arrears' start, a sale's step, a batch's progress. Hourly and daily turns fall on the clock's count. No system keeps a timer of its own: each one's `Quiet` is computed from the state. The step's length is the section's `fixedStep`. |
| Active jobs | `Policy` and `Job` on the ship (phase, lot, costs, retry step, tender), its `Journey` and the engine's components above. An NPC trader is a principal, a company and a ship as a player's are; a region's AI issuer and a governor's policy act from the state each step. |

Manifest queues only `Command::Request` on the world (`Requests::Admit`); every engine command its
systems send is applied at once. So a snapshot taken after a step, admission closed, has an empty
queue and no pending request in any ledger, and R04 refuses one that has either.

Not saved, and rebuilt from the content at start-up, which is why the content is compared exactly:
each module's rules and lookup tables, the pad classes, dock hulls, station layouts and star systems
defined in the engine's services, the spatial index, and `Identities`' lookup and the limits it
holds a load to. Not authoritative, and in no save: the token file and `Remote::Server`'s sessions
and unsent replies (a client that lost one retries its sequence), the pause and time compression,
the event queue, the world's recording, which the journal replaces, the console's and TUI's own
state, and every wall-clock time.

### The content inventory

The manifest holds, for each of the 15 definition types in `Content::DefinitionTypes`' order
(`Manifest.Age`, `.Category`, `.Item`, `.Skill`, `.Hull`, `.Offer`, `Dock.BerthClassDefinition`,
`Dock.VehicleDefinition`, `Dock.PlatformLayout`, `Space.CruiseDrive`, `Space.StarSystem`,
`Manifest.NamePool`, `.GameMode`, `.Tuning`, `.Scenario`), its format's version and every loaded
record as layered, an overlay's effect included and whether the scenario uses it or not, then the
selected scenario's name. Paths, mount names, packaging, times and `mod.yaml` are not in it. Every
value a rule reads from data is in a definition; what a rule reads from code is the build's, under
`game` and `sdk`.

### What it leaves each task

- **R02.** A hosted session mounts its content read-only and refuses a writable mount
  (`Jettison::Host::Options`), so `SaveGame`'s write through the VFS's user mount is not the host's
  path: a snapshot's bytes (`SaveGame::Write`) go through `Core::WriteFileDurably` under
  `Platform::ResolvePaths("manifest")`, with the directories and the lock [below](#snapshot-boundary-and-retention).
- **R03.** Makes the world's identity; compiles in `game` and `sdk`; has `Command::Requests` list
  its commands, which it keeps private today. A batch carries these fields; whether it repeats the
  manifest or names it through the catalogue is R03's.
- **R04.** Writes, reads and checks the section. No load compares content today: the fixtures compare
  manifests in the tests themselves. Refuses a snapshot with a queued or pending request. Adds the
  gated test that pins this inventory to the names the world's save lists.
- **Jettison.** R01 names no gap. A world cannot list what it registers except through its save's
  first bytes; R04's inventory test may ask for that, with the engine's next change.

A mechanic that adds authoritative state adds its row here in its own delivery.

## Commit model: durable step batches

Use one writer per world, enforced with a process-held lock in its writable data root.
Retain stage 3's single ordered admission path. Initially use immutable journal files
and durable replacement of a small recovery catalogue, avoiding an unproven append
protocol. This is deliberately a correctness-first baseline whose I/O cost must be measured.

A journal batch contains its format/world/content identity, sequence number, previous
batch identity, execution step, ordered accepted command envelopes and bounded lengths
plus an integrity checksum. Serialise payloads with Jettison archives. Include empty
steps: production, travel and other autonomous systems still change state without commands.

For each step:

1. Close admission for step `s`. Validate and freeze its command order; later arrivals
   remain provisional ingress for the next step. No command executes yet.
2. Write an immutable batch file durably, then durably replace the recovery catalogue
   to reference that batch as the new committed head. Do not overwrite a committed batch.
3. Only after both operations succeed, acknowledge commands as durably accepted for
   execution. This promises their ordering survives a crash, not that their actions succeed.
4. Apply the recorded envelopes and execute exactly one engine step using the stage 4
   schedule. Return final success/rejection outcomes; replay must reconstruct those
   same outcomes from the durable batch and preceding state.

A committed batch is an obligation to execute its step. A crash after commit but before
execution is recovered by completing that step. This may include an acknowledged command
whose result was never delivered. Uncommitted provisional ingress is not promised to
survive. A lost response is resolved by retrying the same request identity.

The catalogue records the active and previous snapshots and the committed journal head.
Batch files written before a failed/interrupted catalogue publication are uncommitted
orphans: do not execute them or report their commands durable. Recovery reads the valid
catalogue rather than guessing commitment from the highest filename. Frame/checksum
every record, reject length/count overflow and validate the consecutive sequence chain.

Do not equate an I/O error with rollback. Jettison can report failure after rename if
directory fsync fails; a new catalogue may already be visible. On any uncertain commit,
stop admission and advancement, retain files and report an indeterminate outcome. Do not
return a definitive command rejection or reuse sequence numbers until recovery resolves
the catalogue. A retry then returns the recorded outcome or safely submits uncommitted work.

Keep the initial protocol synchronous at the step boundary. Measure fsync latency, step
throughput and backlog before optimising. If it cannot sustain the chosen cadence, revise
batching as an explicit protocol change with equivalent crash tests; never omit fsync or
acknowledge early to reach a target. Storage backpressure slows simulation without
advancing by wall-clock catch-up. Bound ingress and disk use, and expose the failure reason.

## Snapshot boundary and retention

Capture snapshots only after a complete step, before installing the next batch into
the engine command queue. The snapshot clock is the next step to execute. Record the
last applied batch and global command sequence alongside it; never infer inclusion from
a filename or a wall-clock time.

At this boundary no durably accepted external command remains unapplied. Provisional
ingress belongs outside the snapshot and has received no durable acknowledgement. This
narrows stage 3's general pending-command support for production snapshots and removes
ambiguity about which queued envelopes journal replay should restore. A loaded snapshot
with unexpected pending external commands fails validation rather than double-enqueuing.

Write a new uniquely named snapshot through the engine durable-write API, read/validate
it and then publish it in the recovery catalogue, retaining the previous snapshot.
The bytes captured at the boundary are immutable. Initially pause admission/stepping
during capture/publication; any later background writing must preserve that same cut.

Retain all committed journal batches after the previous retained snapshot through the
current committed head. That allows recovery if the latest snapshot is unreadable.
Garbage-collect only files no retained recovery point or backup needs, after successful
catalogue publication. A crash during cleanup must leave every referenced file intact.
Never truncate journals before the replacement snapshot and catalogue are durable.

The current file writer creates missing directories but only explicitly fsyncs the
immediate parent after replacement. The implementer must establish and test durable
creation of the world-directory hierarchy before the first acknowledged commit; do
not assume that nested newly created directory entries survive a power failure. Ensure
data-directory permissions follow the engine path policy and all writes stay on the
intended supported filesystem. Single-writer locking also prevents partial-file clashes.

## Startup and recovery

1. Resolve content and writable user data separately, acquire the world lock and read
   the catalogue. Validate referenced file identities, lengths, checksums and bounds.
2. Build a fresh host with matching validated definitions, types, services and systems.
   Verify Manifest compatibility before loading snapshot state.
3. Load the active snapshot into that candidate. If it is corrupt, try the retained
   previous snapshot only when the complete committed journal chain to the same head
   is available. Unsupported compatibility is not a reason to silently load older data.
4. Replay exactly the batches after the snapshot boundary through the committed head,
   respecting their step/global order and reconstructing retry history/results. Derived
   native commands run from state as before; do not invoke live policy sources twice.
5. Verify counters, escrow, ownership, indexes and travel state; publish readiness only
   after recovery succeeds. Resume at the next simulation step, without elapsed-downtime
   advancement. Wall time may label logs but never chooses replay steps.

Missing/corrupt committed batches, a broken chain or invalid catalogue fail startup with
an actionable diagnostic. Do not skip a record, initialise an empty world over it or
silently discard possibly acknowledged work. Automatic fallback is allowed for a bad
snapshot with an intact recovery chain; storage loss needs explicit backup restoration.

Recovery must reconstruct stage 3's high-water marks and retained outcomes, including
rejected executions. A result aged out before the crash remains too old after restart;
it cannot execute again. Restore next company/order/ship/request/global sequence counters
without reusing IDs. After a fatal runtime invariant failure, recover from known committed
history in a fresh candidate rather than continuing a partially mutated world.

## Shutdown and backups

Graceful shutdown stops admission, commits/executes the already selected batch, completes
its result publication and captures a final snapshot. Provisional requests that were never
selected receive a non-durable stopped response. If storage fails, exit with an error and
leave the last committed recovery point intact; do not claim a successful final save.
Keep signal handling minimal and perform I/O on the host control path.

For the first backup implementation pause at a consistent boundary, capture the catalogue,
referenced snapshots/journals and matching content/build manifest, and copy them as a
verified set. Do not copy a changing directory while assuming it is consistent. Keep
backups outside the active world directory; document their retention separately from
local snapshot retention. A backup can lose work after its captured head; report that
recovery point honestly. Process-crash guarantees assume intact storage.

Restore to a separate empty data directory, validate the backup and run full recovery
before publishing it as the active world. Keep the original untouched until verification.
Test a restored backup with open orders, escrow, an in-flight ship and retry history.

## Relocatable release

Stage a Manifest executable, validator, required engine resources, Manifest content pack,
runtime libraries as needed, licences and a build/content manifest. Do not ship the source
checkout, yq, CMake or the development shell as runtime requirements.

Use `Platform::ExecutablePath` for default installed content discovery relative to the
executable and `Platform::ResolvePaths("manifest")` for separate writable application data.
Retain explicit `--content` and user-data overrides. A read-only install must be runnable;
saves/logs must not go beside the executable. No source/build absolute paths may be required.
The Linux `/proc/self/exe` dependency must be part of the documented platform requirements.

Inspect dynamic dependencies and runtime search paths. A successful run in the same Nix
development shell does not prove redistribution: account for Nix store references and
choose either a documented Nix runtime closure or a tested standalone Linux bundle.
Record the chosen platform contract and test it outside both checkouts and the development
environment. Do not promise universal Linux portability from one local run.

Test with no renderer, display or terminal. The boundary assessment adds a CPU-only
configure requirement to stage 1; retain that coverage through release packaging and
report any regression rather than claiming graphics independence from runtime alone. Network service
deployment, authentication and public-hosting capacity remain later work.

## Delivery and required evidence

Implementer delivery order: versioned compatibility/save section; durable journal and
catalogue; boundary snapshots/recovery; shutdown/backup/restore; relocatable distribution.
Each slice has fault tests before the next expands it.

| Test boundary | Required outcome |
| --- | --- |
| Batch write before catalogue commit | No durable acknowledgement; orphan batch never executes on restart. |
| Catalogue commit before execution | Acknowledged ordering executes once during recovery, with the original assigned step. |
| Execution before result delivery | Retry returns the recovered outcome without repeating payment, cargo transfer or upgrade. |
| Snapshot write/publication interruption | Existing catalogue still names a usable snapshot/journal chain; incomplete files are ignored. |
| Corrupt latest snapshot | Previous snapshot plus retained committed batches reaches the same head/state. |
| Corrupt/missing committed journal or catalogue | Startup fails explicitly; no silent reset or acknowledged-command loss. |
| Disk full, short write, fsync/rename error | No premature durable response; uncertain commits stop processing and are resolved on restart. |
| Content/schema mismatch | Rejected before candidate world publication, with the incompatible identity/version named. |
| Retry pruning and restart | Old IDs stay non-reusable; retained IDs return the same payload/result. |
| Backup restoration | Verified set recovers into a separate directory at its declared committed head. |
| Release relocation | Directory/pack content and writable-data overrides work outside checkouts, with declared runtime dependencies only. |

Use deterministic I/O fault injection and separate-process crash tests around the actual
commit boundaries. Killing a process alone does not demonstrate power-loss durability;
verify fsync ordering and scope claims to the tested storage contract. Compare recovered
state to uninterrupted execution of exactly the same committed batches, including empty
steps, partial fills, expiry, travel and upgrades. Debug/Release/sanitised results must agree.

Run the relevant existing suites and both repositories' required gates for touched code,
with each machine's concurrency (D-051), local clang-tidy opt-in. Record measured snapshot
size, journal
growth, fsync latency and recovery duration without inventing capacity claims. Define
bounded storage/retention limits and test reaching them before marking the stage complete.

## Completion and next milestone

Stage 5 completes when acknowledged commands survive the tested crash boundaries, retries
cannot duplicate actions after recovery, the full local loop resumes without downtime
catch-up, a backup restores successfully and the declared release distribution relocates.
No checks in this document have yet been executed.

The remote-client milestone now begins after minimal authority, as described in the
[development-stage amendments](development-stages.md). After this recovery stage, harden
transport, identity, snapshots, reconnect, bounded queues and four-client operation for a
persistent deployment. Use implementation evidence; these plans are not working software.
