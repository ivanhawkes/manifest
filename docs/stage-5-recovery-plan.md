# Stage 5: persistence, recovery and a relocatable release

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

Every authoritative value belongs in registered world state or a world service: account
balances, venue inventory, cargo, reservations, orders, fills needed for decisions,
company/civilisation progression, allocation counters, command sequencing/retry state,
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
checkout, yq, CMake, the development shell or `lazygit` as runtime requirements.

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
with two compile/test jobs and no local clang-tidy. Record measured snapshot size, journal
growth, fsync latency and recovery duration without inventing capacity claims. Define
bounded storage/retention limits and test reaching them before marking the stage complete.

## Completion and next milestone

Stage 5 completes when acknowledged commands survive the tested crash boundaries, retries
cannot duplicate actions after recovery, the full local loop resumes without downtime
catch-up, a backup restores successfully and the declared release distribution relocates.
No checks in this document have yet been executed.

The next milestone is authenticated multiplayer: transport, identity, filtered snapshots,
reconnect, bounded queues and four-client testing. Begin that design from implementation
evidence for stages 1–5, not from a claim that these plans are already working software.
