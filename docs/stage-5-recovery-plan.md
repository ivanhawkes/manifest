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
| `batch`, `batchId` | The last journal batch this snapshot has applied: its sequence and the identity [the chain](#the-files) gives it. Held by the catalogue, not the snapshot ([the journal](#the-journal-and-its-snapshots-r03-r04-2026-11-24)). | The catalogue's chain. |

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

## Durable I/O, directories and the world lock (R02), 2026-11-24

What R03 to R05 write on, proved before they write. On Jettison `ea66afe`, which provisioned the two
primitives that were missing.

- **A file.** Everything under a world's directory is written with `Jettison::Core::WriteFileDurably`:
  to a partial file beside it, flushed, renamed over the name, and the directory flushed. A reader, a
  crash or a power cut finds the old file or the new, never part of either. It replaces what is
  there: a file that must not be overwritten, a committed batch, is looked for first by the world's
  one writer (R03). The token file is written the same way now, where it was renamed with no flush.
- **A failure is not a rollback.** A write that throws after its rename has left the new file in
  place, perhaps not yet on the disk; [the commit model](#commit-model-durable-step-batches) treats
  every failure as an uncertain commit.
- **A directory.** One made on the way to a file, or by `Core::CreateDirectoriesDurably`, is flushed
  into its parent as it is made, outermost first, so a new world's hierarchy survives with its first
  file. Before `ea66afe` only the file's own directory was flushed.
- **The world's directory and its lock.** `Manifest::Host::OpenWorldDirectory(root)` makes the root
  durably, private to its owner (0700) when it makes it and as it was when it was there, and holds
  `<root>/lock` with `Jettison::Core::FileLock` (the kernel's `flock`) until the `WorldDirectory` is
  destroyed. A second opener, in the same process or another, is refused with the root named. A
  holder that is killed leaves nothing to clear: the kernel lets go with the process. The lock is
  advisory and for a local file system; a world on NFS is not supported.
- **Where the root is** is the caller's: R03 gives `--serve` its world, and R06 selects the default
  under `Platform::ResolvePaths("manifest")`.

Evidence. Jettison's `core` suite makes a hierarchy three deep, refuses one through a file, holds a
lock against its own process and another, and takes it once that process is killed; under `strace`
each `mkdir` is followed by its parent's `fsync`, and a write is the partial file's `fsync`, the
`rename`, the directory's `fsync`. Manifest's `host` suite opens a world three directories deep,
finds it 0700, is refused a second opening by name, and opens it again once let go with its owner's
permissions kept.

Not proved here: that a disk keeps what `fsync` returned for, which no test on one machine shows; a
full disk and a failed `fsync` or `rename`, which R03 injects at its own seam, around its commit
boundaries, where [the evidence](#delivery-and-required-evidence) asks for them; and a server started
on the world, which waits for something to write (R03).

## The journal and its snapshots (R03, R04), 2026-11-24

What a server writes as it runs and reads when it starts, on Jettison `ea66afe` with no engine edit.
The user's decision ([D-113](decision-ledger.md#d-113)): a crash may lose up to 30 s of play, so
nothing is written before a step is taken and a receipt is not a durable acknowledgement. Where the
proposal below differs ([commit model](#commit-model-durable-step-batches) to
[startup and recovery](#startup-and-recovery)), this holds.

### What a crash loses

- **A crash or a power cut** loses what ran since the journal's last flush, at most the flush
  interval, 30 s unless `--flush-seconds` says otherwise: steps, and requests that were answered.
  Everyone's world goes back to the same step, so nothing is duplicated; a client holds no
  authoritative state, and its reconnection is told the sequence to send next.
- **A clean stop** (SIGINT, SIGTERM) loses nothing: it runs what was admitted and writes a snapshot
  ([R05](#a-clean-stop)).
- **A receipt** means the request was queued or ran, as before R03. A request lost with a crash has
  its sequence free again, and a retry of one that was kept is answered from the ledger, not run
  twice.

### The files

Each is under the world's directory, written whole with `WriteFileDurably`, and is followed by its
64-bit FNV-1a, low byte first. A file that is missing, cut short, longer than any of its kind or
changed since it was written is refused by name.

| File | Holds | Written |
| --- | --- | --- |
| `identity` | `Manifest.Host.Identity`: the world's identity and what [D-111](decision-ledger.md#d-111) compares, `game`, `sdk`, `fixedStep`, `commands` and `content`, as [R01](#the-file) defines them. | Once, as the world is made. |
| `journal/<sequence>` | `Manifest.Host.Batch`: the world, the batch's sequence from 1, the identity of the batch before it, the step it begins at, how many steps it covers, and each request that ran in them with the step it ran at, in the order they ran. | Once, at a flush. |
| `snapshots/<batch>` | The world's save (`SimWorld::Save`) as the journal's batch of that sequence left it. | Once, at a snapshot. |
| `catalogue` | `Manifest.Host.Catalogue`: the world, the head batch's sequence and identity, the step after its last, and the snapshots kept, each with its batch, that batch's identity, its own checksum and its step. | Replaced at every flush and snapshot. |

A batch's identity is its file's checksum, so the chain and the integrity check are one number: each
batch names the one before it, and the catalogue names the head. A batch and a snapshot name their
build and content through the world, whose `identity` file holds them once: the content manifest is
255 KB. A snapshot is not a `Sim::SaveGame` and has no `Manifest.Save` section, as R01 proposed: the
catalogue holds what that section would, and the identity file the rest. A file name is its
sequence in twenty digits.

### Flushing and snapshots

`Host::Journal::Advance` takes the world's steps for the server, noting the requests queued as it
does, which run at the start of the first.

- **A flush** is due when the steps since the last reach the flush interval, or the requests since
  it reach 4,096: the batch is written, then the catalogue naming it.
- **A snapshot** is due every snapshot interval, an hour unless `--snapshot-seconds` says otherwise:
  a flush, then the world's save, then the catalogue naming it. Taken between steps, so nothing is
  queued and no request is pending.
- **Two snapshots are kept**, with every batch since the older. Once the catalogue names two, older
  snapshots and batches are removed, a write's leavings among them; a world that has not yet two
  keeps every batch from its first.
- **A write that fails is uncertain, not rolled back.** The writer's exception leaves the server's
  loop and the process exits 1; a `Journal` that is kept takes nothing more. The next start reads the
  catalogue to find which it was. A batch or snapshot written and never named is ignored, and
  written over by the next of that sequence.

### Starting on a world

`--serve` takes `--world <directory>`, required. It holds the directory (R02), then recovers the
world before any client can reach it.

- **A directory holding neither an identity, a catalogue nor a journal is a new world.** Its
  identity is made, 128 bits from `std::random_device`, and written, then a catalogue with no head.
  One cut off between the two has no catalogue and is refused, not made again.
- **An existing world opens only for its own build and content** (D-111): the refusal names the
  first of game, SDK, fixed step, command and content that differs, with both values, the content's
  as fingerprints.
- **`Journal::Recover`** loads the newest snapshot. One that does not read is reported and the one
  before it tried; a world with fewer than two falls back to its own start, as `Host::Start` left
  it. With two and neither reading, the start fails. Then each batch after the snapshot is read,
  checked as the next in the chain, and its steps taken with each request queued at the step it
  ran at; `Requests::Execute` rebuilds the ledger. A batch missing, damaged or out of its chain fails
  the start by name: nothing is skipped.

### The build's identity

`game` is the project's version and a tree, `sdk` a tree: git's own hash of the checkout's
`source`, `cmake`, `CMakeLists.txt` and `CMakePresets.json` as they are on disk, tracked or not,
taken at every build (`cmake/Revision.cmake`) in an index and object store of the build tree's, so
the checkout's own are untouched. Uncommitted changes are another build; Debug, Release and
sanitised builds of one tree are the same build, as their replays must agree. Only one source file
is compiled again when a tree changes, and nothing when none has.

### Evidence

The `host` suite, on the trading scenario, with the writer replaced at the journal's seam:

| Case | Outcome held |
| --- | --- |
| Two flushes, of 601 and 3 steps, each with a request, then a request and five steps never flushed | A fresh world recovered has the flushed step and state hash and both requests' results and steps; the third request's sequence is free; a retry of the first is answered, not run. |
| A flush every 10 steps and a snapshot every 40, for 130 | Snapshots after batches 8 and 12 and batches 9 to 13 remain; a start has the same state hash from the newest, and from the older when the newest is damaged; with neither, it fails by name. |
| One snapshot, damaged | Recovered from the world's start, to the same state hash. |
| Batch write fails, half a partial file left | No batch file; head 0 on opening; the world starts without the request; the next flush succeeds. |
| Batch written, catalogue write fails | The orphan is on disk; head 0; not run; written over by the next flush, and a third world recovers that. |
| Catalogue in place, then the write reports failure | The flush threw and the journal took nothing more; on opening the head is 1 and its request is there, at its step. |
| A batch damaged, cut short, empty, another's or missing; a catalogue damaged, another world's or missing | Each refused by name, the world left at its first step. |
| Another build, SDK, fixed step, command or content | Refused, naming the field and both values. |

`cli_serve` flushes every second and snapshots every two: it kills the server outright (SIGKILL)
3.4 s in, starts it again on a flushed step, stops it with SIGINT and finds a third start at the
step it stopped at.

### Write volume and wear

Why the journal has this shape: the designs weighed on 2026-11-24, by what each writes in a day.

Measured in Debug on Lythir's NVMe (ext4), the trading scenario:

| Measured | Figure |
| --- | --- |
| A world with no player: 10 regions, 10 markets, 1,215 brokers' orders | 462 KB, about 46 KB a region |
| A player provisioned: a company, a ship and their market reports for 10 venues | 76 KB more |
| A resting order with its retained request | 624 bytes more |
| The same world after a day of steps | 568 KB: it does not grow unattended |
| Serialising a 550 KB save, with no write | 7.3 ms |
| A batch committed before every run of steps, as R03 first had it, no client | 47 commits a second, about 11 ms each, a 235-byte batch in a 4 KiB block |
| The journal as it is, 65 s with no client | Four 230-byte batches; a snapshot 475 KB |

Guessed from those, the size of a world's save:

| Players | Assumed | Save |
| --- | --- | --- |
| 1, today's scenario | As measured | 0.5 MB |
| 50, the most expected on a server (the user's) | Two regions a player, each player knowing 30 venues, with 5 ships, 20 orders and 256 retained requests | About 22 MB |
| 256 | The same | About 110 MB, between 50 and 250 |

Market reports are over half of it, at a guessed 7.5 KB a venue a player knows; a request in the
journal is guessed at 400 bytes, and a player at one request every 10 s.

Written in a day, in file blocks:

| Design | 1 player | 50 players | 256 players | Flushes | A crash loses |
| --- | --- | --- | --- | --- | --- |
| A batch committed before every run of steps (R03 as first built), with an hourly snapshot | 16 GB | 17 GB | 19 GB | About 188 a second | Nothing answered |
| The whole world saved every 10 s | 4.1 GB | 190 GB | 950 GB | 2 every 10 s | Up to 10 s |
| The whole world saved every 30 s | 1.4 GB | 63 GB | 317 GB | 2 every 30 s | Up to 30 s |
| **A batch flushed every 30 s, a snapshot every hour (chosen)** | 23 MB | 0.7 GB | 3.5 GB | 4 every 30 s, 4 more an hour | Up to 30 s |

Wear, as the years in which each would write the 600 TB a typical 1 TB consumer drive is rated for:

| Design | 1 player | 50 players | 256 players |
| --- | --- | --- | --- |
| A batch before every run of steps | About 100 | About 95 | About 85 |
| The whole world every 10 s | About 400 | 8.6 | 1.7 |
| The whole world every 30 s | Over 1,000 | 26 | 5.2 |
| **A batch every 30 s, a snapshot every hour** | Beyond any drive's life | Over 2,000 | About 470 |

Held on disk at once, with two snapshots kept: the whole-world designs hold twice the save; the
journals hold that and the batches since the older snapshot, up to two hours of them, which is 1.4 GB
for a batch before every run of steps and under 100 MB for a batch every 30 s.

What the figures say:

- **A batch before every step costs the same idle as busy.** It writes a 4 KiB block 47 times a
  second whatever is in it, and four million files a day. The blocks alone would not wear a drive
  out, but each file is also a directory entry, an inode and a filesystem-journal write, forced out
  by 188 flushes a second: guessed at two to four times the blocks, and not measured.
- **Saving the whole world often is the cheapest design for a small world and the dearest for a
  large one**, since it rewrites everything each time. It passes the journal's cost at a save of 6
  to 20 MB, some 15 to 50 players.
- **A batch flushed every 30 s pays for what happened, not for the time or the world's size**, and
  is the least at every size. Its snapshots are most of what it writes at scale; a longer snapshot
  interval trades them for a longer replay at a start.

Not measured: what reaches the drive beyond the file blocks, the serialising pause in a Release
build, and how the 76 KB a player splits between their reports and the rest.

### What it leaves

- **R05.** Done: [shutdown, backups and limits](#shutdown-backups-and-limits-r05-2026-11-24).
- **R09.** The gated test that pins [the state inventory](#the-state-inventory) to the names the
  world's save lists; a content refusal naming the first definition that differs; and the
  serialising pause of a large world's snapshot, 7.3 ms at 550 KB in Debug and unmeasured in Release.

## Shutdown, backups and limits (R05), 2026-11-24

How a server stops, what it backs up and what a world may store, on Jettison `ea66afe` with no
engine edit. Where the [shutdown and backups](#shutdown-and-backups) proposal differs, this holds.

### A clean stop

On SIGINT or SIGTERM the server admits nothing more, and `Host::Journal::Stop` runs the requests
it admitted since the last step in one more step, so none is left with a receipt and no answer; the
server then sends their answers and a snapshot is written, so the next start loads it and replays
nothing. The proposal's non-durable stopped answer is not needed: under
[D-113](decision-ledger.md#d-113) every admitted request runs in the next step, and the step at a
stop is that step. A stop costs one step and one save, 475 KB today. A write that fails exits 1
with no stop reported, and the catalogue still names the last recovery point.

### Backups

`--serve --backups <directory>` writes a backup with every snapshot: hourly, and at a stop. The
directory must be outside the world's, and is made private to its owner. A backup is a directory
`<world>.<batch>` holding the world's `identity`, the snapshot and a `catalogue` naming that
snapshot alone, its head the snapshot's batch, written in that order with `WriteFileDurably`. So
the set is consistent by construction: the bytes are the ones the world's own snapshot was written
from, between steps, and no changing directory is copied. Its recovery point is its snapshot's
step, up to an hour behind the world's head, and is the step a restore names.

- **One cut off** before its catalogue is not a backup: it is removed by the next.
- **A backup that fails** is logged as an error and costs nothing else: the world's own files are
  already whole, and the server goes on.
- **The newest 24** of the world's backups are kept, a day of hourly ones and every stop's; older
  ones go after a backup succeeds. Another world's in the same directory are left alone.

### Restoring

`manifest <options> --restore <backup> --world <directory>` restores into a directory holding
nothing but its lock, and refuses one that holds anything, a world included. It refuses a backup of
another build or other content, or whose catalogue is another world's, before writing anything.
It then copies the identity, every snapshot the catalogue names and every batch after the oldest,
each read and checked against its checksum, the catalogue last, and recovers the world as a start
does. Anything it wrote is removed again when any of that fails, the catalogue first. The backup is
only read. A restore cut off by a crash leaves an identity with no catalogue, which a server refuses
rather than make a new world over it; clear the directory and restore again. A stopped world's own
directory restores too, as its catalogue names more.

### What a world stores

| What | Bound |
| --- | --- |
| `identity` | Once; 255 KB today, at most 64 MiB read. |
| `snapshots/` | Two; 475 KB each today. A save larger than `Storage::snapshotBytes`, 4 GiB, is refused as it is written, since a start would refuse it as it is read: the server exits 1 and the world keeps its last recovery point. |
| `journal/` | The batches since the older snapshot: two hours, 240 at 30 s, more when 4,096 requests flush one early, each at most 32 MiB read. |
| `catalogue` | Once; 377 bytes today, at most 4 KiB read. |
| Backups | 24 of 730 KB today, 17.5 MB; at the 110 MiB guessed for 256 players, about 2.6 GiB. |

### Evidence

| Case | Outcome held |
| --- | --- |
| A request admitted, then a stop | It runs in one last step and is answered; the snapshot is of that step, and a start loads it with the batch removed. A second stop writes nothing. |
| A flush every 10 steps, a snapshot every 40, two backups kept | Backups after batches 8 and 12; the one after 16 cut off at its catalogue is none, and the world goes on; after 20, the one cut off and the oldest are gone. |
| A backup restored | Into an empty directory: the head, step and state hash, a retry answered from the ledger and the request sequence; served on and opened by a server again; the backup's bytes unchanged. Into a world, of another build, or damaged: refused, the directory left empty. |
| A save over the limit | Not written; the batches before it are, and a start reaches the same step. |
| Backups inside the world's directory, with a trailing separator or not | Refused before the world is made. |
| The trading fixture, backed up in flight | A bid resting with its escrow, the ship cruising and Ada's every outcome as before; restored and flown on through the journal, the loop ends at its pinned state hash. |

`cli_serve` stops the server with SIGINT, restores its newest backup into a new directory at the
step it stopped at, and is refused restoring it over the world. Measured in Debug: a backup is
730 KB, and restoring one takes 0.25 s, 0.11 s of it the start-up.

### What it leaves

- **R09.** A backup verified only as it is restored, not read back as it is written; the snapshot
  limit reached at 4 GiB rather than at a test's 1 KiB; and how long a stop of a large world takes.

## The runtime distribution contract (R06), 2026-11-24

What R07 stages and tests, and what a host must give it to run. The user chose a standalone bundle and
the TUI in it (2026-11-24, [D-114](decision-ledger.md#d-114)). Where the [relocatable
release](#relocatable-release) proposal differs, this holds. Read from Manifest `feature-base` at
`9dd0d81` on pin `ea66afe`. This is a planning task: no code changed, and nothing below is
enforced yet.

### What it is

A tarball, `manifest-<version>-<game tree>-linux-x86_64.tar.gz`, holding one directory:

| Path | Holds |
| --- | --- |
| `bin/` | `manifest` (the server, `--check-startup` and `--restore`), `manifest-validate`, `manifest-token` and `manifest_tui`. |
| `share/manifest/manifest.pack` | The content, packed as the build packs it. |
| `share/manifest/BUILD` | The build's version, its game and SDK trees ([D-111](decision-ledger.md#d-111)) and the content's fingerprint, the same values a save's `Manifest.Save` holds. |
| `share/doc/manifest/` | Manifest's `LICENSE` and the licence of every library linked into a binary, each from its source at its pinned version. |

R07 stages it from a clean commit, so its game tree names a commit. The directory may be anywhere
and read-only. Nothing is written to it, nor to the working directory.

### What a host must have

- **x86-64 Linux with glibc 2.39 or newer**, loaded through `/lib64/ld-linux-x86-64.so.2`: Ubuntu
  24.04 (2.39) and Debian 13 (2.41) onwards, the oldest the user chose to reach (2026-11-24). The
  binaries are built against nixpkgs' glibc, 2.44 today, and three of the symbols they import ask
  for more than 2.38 (`objdump -T`, the release build, 2026-11-24): `remainder` and `sqrtf` at
  2.43, from `libm`, and `__inet_pton_chk` at 2.42, which `_FORTIFY_SOURCE` puts in place of
  `inet_pton`. R07 binds each to its older version, and the check fails on any symbol above 2.39,
  so a nixpkgs bump that brings in another is caught and pinned too. If those pins become a hassle
  to keep, the release is built instead in an Ubuntu 24.04 container, or with `zig cc` targeting
  2.39 (the user's, 2026-11-24). The binaries need only glibc's own `libc`, `libm`,
  `libpthread`, `libdl` and `librt`.
- **On NixOS**, `programs.nix-ld`, which puts a loader at `/lib64` and the system's glibc behind it
  (2.44 on Lythir, 2026-11-24).
- **`/proc` mounted.** The default content is found from `/proc/self/exe`
  (`Jettison::Platform::ExecutablePath`). So a binary is started directly, never through a loader
  named on its command line (`ld.so bin/manifest`), which would make `/proc/self/exe` the loader.
- **A local file system** for the world, which `flock` holds
  ([R02](#durable-io-directories-and-the-world-lock-r02-2026-11-24)); NFS is not supported.
- **`HOME`**, or the XDG variables below, as absolute paths.
- **For the TUI, a terminal.** No display, GPU, Vulkan loader or window system, for any binary.

Not supported: other architectures, musl, older glibc, Windows and macOS, and the claim that any
Linux with a new enough glibc will do. Only the hosts R07 runs the bundle on are tested.

### How the binaries are linked

- `libstdc++` and `libgcc` are linked statically (`-static-libstdc++ -static-libgcc`); every other
  library already is, libsodium, ENet and FTXUI included.
- No `RUNPATH`, and the interpreter is `/lib64/ld-linux-x86-64.so.2`. Today both name the
  devenv shell's Nix store paths. R07 checks that no binary's interpreter, `NEEDED` or `RUNPATH`
  names `/nix/store` or any absolute path but the loader's.
- No `libvulkan.so.1`. The TUI asks for it today, through the console's `Jettison::Devtools`, whose
  `imgui` target compiles ImGui's GLFW and Vulkan backends and links both publicly. The TUI never
  draws. This is the one engine gap: Jettison provisions the split before R07 (below).

### Where it reads and writes

Content and writable data stay apart. Each default below is an option's, and the option still
overrides it.

| What | Default | Option |
| --- | --- | --- |
| Content | `<bin>/../share/manifest/manifest.pack`, from `ExecutablePath` | `--content`, `--overlay` |
| The world | `$XDG_DATA_HOME/manifest/world` | `--world` |
| Backups | `$XDG_DATA_HOME/manifest/backups`, written by default | `--backups` |
| The token file | `$XDG_CONFIG_HOME/manifest/tokens`, for `manifest` and `manifest-token` | `--tokens` |
| The TUI's transcript | `$XDG_STATE_HOME/manifest/tui-session.log` | none |
| The server's log | stderr, for the service manager to keep | none |

The XDG directories default under `HOME` as the specification has it
(`Jettison::Platform::ResolvePaths("manifest")`), and each is made 0700 when it is first written
to. `--scenario` stays required: which game to make is the operator's choice. Backups become a
default because a server run with none has only its world's directory to lose.

### What it leaves each task

- **Jettison, before R07.** ImGui's core in `imgui`, and its GLFW and Vulkan backends in a target
  that only `render` links, so a consumer of `Devtools` links no window system or Vulkan. The engine's
  acceptance: an executable linking `Devtools` alone has no `libvulkan` or `libglfw` among its
  `NEEDED` and starts with no Vulkan loader present. Manifest's: the staged `manifest_tui` passes
  the same check.
- **R07.** The defaults above: `--content` optional in `Host::ParseOptions`, the TUI taking the
  host's options rather than the compiled-in `MANIFEST_MOD_DIRECTORY`, and its transcript moved from
  `build/tui-session.log`. The build tree laid out as the bundle (`bin/`, `share/manifest/`) so a
  development run finds its content the same way. The link flags, the staging script, `BUILD` and
  the licences. The three glibc symbols bound to their versions at or below 2.39, and whatever
  linking `libstdc++` statically adds. The checks above. Then the bundle unpacked into a read-only directory outside both
  checkouts and run under `env -i` with only `HOME` and `PATH`, outside the devenv shell: a
  start, a server stopped and restored, a validation of the pack, a token issued, and the TUI
  connected; and the same in Ubuntu 24.04 and Debian 13 containers. The CPU-only configure (`clang-notui`) kept as it is.
- **R09.** The bundle's evidence in the stage's close, the glibc check among it.


## Commit model: durable step batches

The proposal of 2026-11-15, kept as it was made. Its commit before execution and its durable
acknowledgement were built, measured and replaced on the user's decision
([D-113](decision-ledger.md#d-113)); what a server does is
[above](#the-journal-and-its-snapshots-r03-r04-2026-11-24).

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

The world's hierarchy is made durably, private to its owner and held by one writer, which also
keeps partial files from clashing ([R02](#durable-io-directories-and-the-world-lock-r02-2026-11-24)).
All writes stay on that one local file system.

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

The proposal of 2026-11-15; what R05 built is [above](#shutdown-backups-and-limits-r05-2026-11-24).

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

The proposal of 2026-11-15; what R06 selected is [above](#the-runtime-distribution-contract-r06-2026-11-24).

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
