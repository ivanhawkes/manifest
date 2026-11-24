# Session handoff

## Next, on resuming — 2026-11-25 (R09 done, Stage 5 closed)

R09 is done: **Stage 5 is closed** ([the close](stage-5-recovery-plan.md#stage-5-closed-r09-2026-11-25)),
on pin `13c1bec`. The `host` suite pins the state inventory to the names a world's save lists
(`SimWorld::ComponentNames`, `ServiceNames`). A world opened under other content is refused naming the
first definition that differs, and a pruned request sequence is held too old across a load. The
bundle passed on Lythir and in Ubuntu 24.04 and Debian 13. The Release figures are recorded: a flush
holds the server's loop about 11 ms, a snapshot about 60. Saves are to leave the step's thread
([D-115](decision-ledger.md#d-115)), not yet built. The later workstreams (L01 onwards) wait until the
user has played the first loop for a day or two; W22 waits for the first larger hull.

R08 is done: [reconnect and recovery](stage-5-recovery-plan.md#reconnect-and-recovery-r08-2026-11-24),
on pin `d9f565c` with no engine edit, in tests alone. The `remote` suite crashes a journalled server
under three clients and reconnects them: what the journal kept is answered from the ledger, what it
lost is retried in each actor's order. The development fixture runs through a journal, a clean stop
and a crash, and recovers to the uninterrupted world's hash. The TUI still retries nothing. R09
closes Stage 5.

R07 is done: [the distribution](stage-5-recovery-plan.md#the-distribution-r07-2026-11-24), on pin
`d9f565c` with no engine edit. `clang-release` is the distribution build (`MANIFEST_DISTRIBUTION`):
its four binaries link `libstdc++` statically, load through `/lib64/ld-linux-x86-64.so.2` with no
`RUNPATH`, and ask for glibc 2.38 at most, the three newer symbols bound in
`source/apps/distribution/GlibcFloor.cpp`; its `distribution` test fails on anything newer than 2.39,
which a nixpkgs bump would bring and `GlibcFloor.cpp` then binds. So the release preset's tests need a
loader at `/lib64`, nix-ld on NixOS. Every build tree is laid out as the bundle, `bin/` and
`share/manifest/manifest.pack`: `--content` defaults to the pack beside the executable, and the
world, backups, tokens and the TUI's transcript to `~/.local/share`, `~/.config` and `~/.local/state`
under `manifest`. `scripts/stage-bundle.sh` stages the tarball from a clean checkout into
`build/bundle`, and `scripts/test-bundle.sh <tarball>` runs it here and in Ubuntu 24.04 and Debian 13
containers (Docker).

R05 is done: [shutdown, backups and limits](stage-5-recovery-plan.md#shutdown-backups-and-limits-r05-2026-11-24),
on pin `ea66afe` with no engine edit. `manifest --serve --world <directory> --backups <directory>`
stops on SIGINT or SIGTERM by running what it admitted in one last step and writing a snapshot, and
writes a backup with every snapshot, the newest 24 kept; `manifest --restore <backup> --world
<directory>` restores one into an empty directory and checks it by recovering it. The journal
itself is as R03 and R04 left it ([the journal and its snapshots](stage-5-recovery-plan.md#the-journal-and-its-snapshots-r03-r04-2026-11-24),
[D-113](decision-ledger.md#d-113)): a crash loses up to 30 s. A world is its build's (D-111), and a build is its trees
(`cmake/Revision.cmake`), so any edit under `source` or `cmake` makes a world or backup of the build
before unopenable: delete it. A remote view may be 192 KiB (`Remote::kMaxMessageBytes`); past about
256 KiB an unreliable message does not arrive, and a view wants deltas
([history](history.md#r02-durable-io-directories-and-the-world-lock--2026-11-24)). W22, the drives
past the torch, waits for the first larger hull. The later workstreams (L01 onwards) wait until the
user has played the first loop for a day or two (2026-11-24). Jettison's station traffic lanes were
measured and parked (`fixture_traffic`, 2026-11-22), to be taken up when it fails. A commit is gated
in the second checkout, `build/gate-tree`, with `--tidy`, editing alone beside it
([gating beside the work](build-and-gates.md#gating-beside-the-work)). The development fixture takes
about 50 s in Debug and six minutes under ASan, most of the gate's sanitised stage.

What each task found and left, and how each was closed, is in [history](history.md), newest first.
