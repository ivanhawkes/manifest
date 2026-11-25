# Stage 5: persistence, recovery and the distribution (2026-11-24 to 11-25)

Task records as they were written at the time, newest first. Moved from `docs/history.md` on
2026-11-25; the [history index](../history.md) lists every record.

## Stage 5 cross-review — 2026-11-25

Codex reviewed `e1def0f..cdde5ab` and the stage's three engine provisions (`build/reviews/stage-5`).
Round 1's six findings: a clean stop's last replies were dropped, the engine's server closing every
connection at once as it was destroyed, so the stop now closes each session and polls until its
client has what it was sent, to two seconds (Jettison's `Server::Closing`); `ComponentNames` and
`ServiceNames` returned views a later registration could leave dangling (copies now); an actor's ID
was unbounded, so a batch of the most requests could outgrow what a start reads (256 bytes now, a
`static_assert` on the batch's bound and a check before it is written); a restore checked each file
as it copied it, and now checks every one first; a program writing only its tokens needed every XDG
variable or HOME (Jettison's `ResolvePath`, one directory at a time); and no test cut a snapshot's
write or its catalogue's off, which the `host` suite now does. Round 2 found each resolved and one
more: provisioning made a principal whose ID was past that bound, so it could never act, and now
refuses it as malformed. Round 3, aimed at what the first two had not examined, found F7 resolved and
two more: making a default place's directories chmodded an existing parent named through `new/..`,
now normalised first; and one long `Advance` flushed and snapshotted once at its end, now at each
step due on the way, as the journal suite's long advance against short ones shows. Round 4, the last
the user allowed, found F9 resolved and F8 open where the world's and the backups' directories are
made, the normalising fix wrong across a symlink (F10), and D-111 and build-and-gates stale (F11).
All three were fixed after it: every directory is made a component at a time, each made private
through the path it was made by, and the documents say what is built. Round 5, which the user added,
found them resolved and three more: a world within its backups could be removed by the backup named
as its directory is, and backups led into the world by a symlink after a directory made on the way
passed the check; both are refused, the check made again once the backups' directory is. And a
directory made and then made private stayed open to everyone when its parent's flush between the two
failed, so Jettison's `CreateDirectoriesDurably` makes it 0700 from the start (pin `459af99`).
Round 6 was stopped twice by OpenAI's filter before it wrote a review; its probes, kept in
`build/reviews/stage-5/round-6a-scratch`, found F12 to F14 refused as intended. A symlink planted in a
world's directory leads its writes outside, but planting one needs the owner's access to that private
directory already, so the user settled it as no threat and the review closed there.

## R09: Stage 5 closed — 2026-11-25

[The close](stage-5-recovery-plan.md#stage-5-closed-r09-2026-11-25), on pin `13c1bec`, whose one
provision is `SimWorld::ComponentNames` and `ServiceNames`: a world named its components only in
its save's first bytes, and its services nowhere but inside the save, so the inventory could not be
pinned to them from outside.

What it found. A content manifest has no count of its types; it relies on the build that wrote it,
which a world shares by D-111, so `FirstDifference` reads `DefinitionTypes`' count, and a record is
named by its file (`item-copper`), not its ID. A manifest damaged in memory, as the identity test makes
one, does not read, and the refusal then stands on the fingerprints as before. No test held a pruned
sequence too old after a load; the `command` suite's pruning case now loads the world and checks it.
The trading scenario holds four players, its starter venue's life support having four connections, so
the larger worlds R04 guessed at could not be built and measured. The figures are for zero, one and
four players. A save is written on the server's thread, a snapshot holding it about 60 ms; the user
decided saves leave that thread in future (D-115).

## R08: reconnect and recovery — 2026-11-24

[Reconnect and recovery](stage-5-recovery-plan.md#reconnect-and-recovery-r08-2026-11-24), on pin
`d9f565c` with no engine edit and no change outside the tests. The `remote` suite's `Served` now
serves a world through its journal when asked, crashing as it serves again, and the development
fixture steps through one.

What it found. Several sessions of one actor that retry after a restart must do so in her order:
the later sequence first is a `SequenceGap`, which consumes nothing, so the earlier then the later
both run. A `Completed` heard after the last flush is lost with the crash, and the welcome shows it,
its sequence at or below the one heard. `Served::Await` matched a reply by sequence alone, from any
client, which two actors' equal sequences confused; each client's replies are now kept apart. The
fixture's `ForwardTo` passed the journal by, so it now steps through one when given, as `Step`
did. A clean stop writes a snapshot that holds what a test set outside any request, which is how
the fixture's funds reach a recovery.

## R07: the distribution — 2026-11-24

[The distribution](stage-5-recovery-plan.md#the-distribution-r07-2026-11-24), on pin `d9f565c` with no
engine edit, staged from `0ac13fe`. The release preset now links the four binaries to run outside the
toolchain, every build tree is laid out as the bundle with the contract's defaults, and the tarball,
staged from a clean checkout, ran from a read-only directory under `env -i` on Lythir and in Ubuntu
24.04 and Debian 13 containers: a start, a validation, a token, a server served, the TUI connected, a
stop and a restore.

What it found. The clang wrapper's gcc does carry `libstdc++.a`, in its `lib/`, though
`clang++ -print-file-name` does not find it there; the wrapper's `-L` does. Nix's wrapper gives every
executable a `RUNPATH` into the store, which no link option takes back, so `patchelf` removes it
after the link; `--dynamic-linker` sets the interpreter. The static `libstdc++` names the loader
among its `NEEDED` and asks for nothing above glibc 2.38, so the three symbols R06 found are all
there is to bind, and `--wrap` binds them for the static libraries' calls as well as Manifest's,
where a `.symver` in each source would not reach them. The TUI connects in a pseudo-terminal from
`script` fed a line at a time, which is how `bundle_smoke` runs it on every build tree. MikkTSpace has
no licence file, only its header's comment. `--serve` without `--world` or `--tokens` no longer fails
its parse, so `cli_serve_no_tokens` and `cli_serve_no_world` gave way to the default-place tests.

## The engine's ImGui split, pin d9f565c — 2026-11-24

The engine gap R06 named, provisioned in Jettison before R07: its `imgui` target is ImGui's core
and its FreeType rasteriser, and `imgui_backends` the GLFW and Vulkan backends, which `render`
alone links ([Jettison's history](../../../jettison/docs/history.md#neutral-sdk-and-manifest-integration)).
Its `devtools_alone` test links `Devtools` alone and fails on a `libvulkan` or `libglfw` among its
`NEEDED` or mapped while it runs. Here, `manifest_tui` on pin `ea66afe` needed `libvulkan.so.1`;
on `d9f565c` it needs `libdl`, `libstdc++`, `libm`, `libgcc_s`, `libpthread` and `libc`, and ran
on a terminal with neither mapped. No Manifest code changed.

## R06: the runtime distribution contract — 2026-11-24

[The runtime distribution contract](stage-5-recovery-plan.md#the-runtime-distribution-contract-r06-2026-11-24),
a planning task on pin `ea66afe` ([D-114](../decision-ledger.md#d-114)). It selects a standalone
tarball: the four binaries, the packed content and the licences, for x86-64 Linux with glibc 2.39 or
newer (Ubuntu 24.04, Debian 13) and nix-ld on NixOS. Content is found from the executable, and the world, backups, tokens and
the TUI's transcript go under the XDG directories for `manifest`.

What it found. The release binaries' interpreter and `RUNPATH` name the devenv shell's store paths.
Built against nixpkgs' glibc they ask for 2.43, though only `remainder`, `sqrtf` and
`__inet_pton_chk` need more than 2.38, so R07 binds those three to older versions. A binary started through a loader
named on its command line would see `/proc/self/exe` as the loader, which rules out a bundled
`ld.so`. The TUI links `libvulkan.so.1` through the engine's `imgui` target without ever drawing,
and it reads its content from a source path compiled into it and writes its transcript under
`./build`. The first is Jettison's to provision; the rest is R07's.

## R05: shutdown, backups and restore — 2026-11-24

[Shutdown, backups and limits](stage-5-recovery-plan.md#shutdown-backups-and-limits-r05-2026-11-24),
on pin `ea66afe` with no engine edit. A stop now runs what it admitted in one last step, answers it
and writes a snapshot, so a request is never left with a receipt and no answer and a start replays
nothing; `--backups` writes the world's identity, snapshot and a one-snapshot catalogue beside it
with every snapshot, the newest 24 kept; `--restore` copies a backup into an empty directory and
recovers it before a server takes it; a save larger than a snapshot may be is refused as it is
written. The trading fixture is backed up in flight with a bid resting, restored and flown on
through the journal to its pinned state hash.

What it found. Taking a backup from the snapshot's bytes in memory, rather than copying the world's
directory, makes it consistent by construction and costs one more write an hour; its recovery point
is the snapshot's, up to an hour behind. The proposal's non-durable "stopped" answer was not needed
once D-113 ran every admitted request in the next step. A snapshot written larger than a start reads
would have been unreadable: the write now refuses it. The backup's read-back and a large world's stop
time go to R09.

## R04: the journal flushed every 30 s, and its snapshots — 2026-11-24

[The journal and its snapshots](stage-5-recovery-plan.md#the-journal-and-its-snapshots-r03-r04-2026-11-24),
on pin `ea66afe` with no engine edit, and the user's decision ([D-113](../decision-ledger.md#d-113)).
R03's journal, below, committed a batch before every run of steps so that no answered request could
be lost. Shown its cost, 16 GB of blocks and four million files a day with nobody playing, the user
asked whether a game needs that, and settled on losing up to 30 s in a crash. So the world steps and
answers at once; `Host::Journal::Advance` notes what ran and writes a batch every 30 s and the
world's save every hour, keeps two snapshots with the batches since the older, and removes the
rest; `Recover` loads the newest snapshot that reads and replays the batches after it; a clean stop
flushes. `Remote::Server` is as it was before R03: the held receipt went with the promise.

What it found. Saving the whole world every 30 s, the simplest design, was the first proposal, and
is the cheapest at today's 0.5 MB; measured per player and guessed up, a world of 256 players is
about 110 MB and would then write 317 GB a day, so the journal stayed and only its cadence changed
([write volume and wear](stage-5-recovery-plan.md#write-volume-and-wear)). A snapshot needs no
record of its own: it is the world's save under a checksum, and the catalogue holds what R01's
`Manifest.Save` section would have. R01's inventory pin and a content refusal's first differing
definition were not done, and go to R09.

## R03: the step journals and the catalogue — 2026-11-24

[The journal as first built](stage-5-recovery-plan.md#write-volume-and-wear), since replaced (below), on pin
`ea66afe` with no engine edit: the engine's durable write, FNV-1a and binary archive were enough.
`Host::Journal` commits a run of steps, a batch file then the catalogue that names it, before the
server takes them; `Remote::Server` holds a queued receipt until the host acknowledges, so the
receipt is the durable one; `manifest --serve` takes `--world`, makes the world there or replays its
committed batches from the first step, and exits on a commit that fails. A world's `identity` file
is D-111's fields, the build named by git's tree hash of each checkout's build inputs, taken at
every build (`cmake/Revision.cmake`), and `Command::Requests` lists its commands.

What it found. One batch a step, as proposed, would have queued batches behind a slow disk; a run of
steps to a batch needs no queue, and `FastForward` already ran a wait's requests at the start of its
first step. The checksum a file needs is also the identity the chain needs. Measured on Lythir, a
commit is about 11 ms and an idle server makes 47 a second, 16 GB of 4 KiB blocks a day, which
nothing removes until R04; whether an idle server should commit a second's steps ahead instead is
left to the user. The replay from the first step was not R03's by the delivery order, and is here
because a journal nothing has read is not evidence: R04 starts it from a snapshot.

## R02: durable I/O, directories and the world lock — 2026-11-24

[The contract](stage-5-recovery-plan.md#durable-io-directories-and-the-world-lock-r02-2026-11-24), on pin `ea66afe`. Jettison's `WriteFileDurably` flushed a file and its own
directory but not the directories it made on the way, and nothing held a directory to one process, so
Jettison provisioned `Core::CreateDirectoriesDurably` and `Core::FileLock`
([its history](../../../jettison/docs/history.md#neutral-sdk-and-manifest-integration)). Manifest's part is
small: `Host::OpenWorldDirectory` makes a world's root durably and 0700 and holds its lock, refusing a
second opener by name (the `host` suite); and the token file, which was renamed into place with no
flush, goes through `WriteFileDurably`. No server opens a world yet: R03 gives `--serve` the option
with the first thing to write. Fault injection is R03's too, at its own seam.

The message longer than a packet, booked from Stage 4's review for this engine change, needed no
splitting of the engine's: a message is an ENet packet, which ENet splits into datagrams and
reassembles, and the 64 KiB was a limit a game sets. `Remote::kMaxMessageBytes` is 192 KiB on both
ends, three times the trading scenario's view. Jettison measured where that stops on loopback: an
unreliable message arrived every time at 256 KiB and never at 384 KiB, its fragments sent at once into
the receiver's 256 KiB socket buffer, so past that a view wants deltas. The `remote` suite rests 24
bids on Ada's book in the trading scenario, her view 69,829 bytes, and it arrives; with the limit left
at the engine's default the server logs it as over the limit and the case fails.

## R01: the save and compatibility contract — 2026-11-24

A planning task, no code and no engine edit: [the contract](stage-5-recovery-plan.md#save-and-compatibility-contract-r01-2026-11-24) defines the `Manifest.Save`
section, exact compatibility ([D-111](../decision-ledger.md#d-111)) and the inventory. The inventory was
taken from a hosted trading world's own save, whose first bytes name its storages: 50 components, 32
of them Manifest's, and 3 services, 461,719 bytes at step 0. Three things the plans had assumed were
not so. No load compares content: Stage 4's handoff said a save was refused by its fingerprint, and
only the fixtures' own tests compare manifests. The world's save does not hold its step's length, so
a save made at 1/60 s loads into a world stepping at any other. And the executable knows neither its
own revision nor the SDK's. A hosted session refuses a writable mount, so a snapshot is written
through `Core::WriteFileDurably`, not the VFS's user mount, which is R02's to prove.
