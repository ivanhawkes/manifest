# Session handoff

## Next, on resuming — 2026-11-24 (R03 and R04 done, then R05)

R03 and R04 are done: [the journal and its snapshots](stage-5-recovery-plan.md#the-journal-and-its-snapshots-r03-r04-2026-11-24),
on pin `ea66afe` with no engine edit. The user decided a crash may lose up to 30 s of play
([D-113](decision-ledger.md#d-113)), so `manifest --serve --world <directory>` steps and answers at
once, `Host::Journal` writes a batch of what ran every 30 s and the world's save every hour, a start
recovers from the newest snapshot and the batches after it, and a clean stop loses nothing. What
each design would write in a day, and why this one, is under
[write volume and wear](stage-5-recovery-plan.md#write-volume-and-wear). Next is R05, shutdown, backup
and restore and the storage limits ([what R04 left](stage-5-recovery-plan.md#what-it-leaves)). A
world is its build's (D-111), and a build is its trees (`cmake/Revision.cmake`), so any edit under
`source` or `cmake` makes a world of the build before unopenable: delete it. A remote view may be
192 KiB (`Remote::kMaxMessageBytes`); past about 256 KiB an unreliable message does not arrive, and
a view wants deltas ([history](history.md#r02-durable-io-directories-and-the-world-lock--2026-11-24)).
W22, the drives past the torch, waits for the first larger hull. Jettison's station traffic lanes
were measured and parked (`fixture_traffic`, 2026-11-22), to be taken up when it fails. A commit is
gated in the second checkout, `build/gate-tree`, with `--tidy`, editing alone beside it
([gating beside the work](build-and-gates.md#gating-beside-the-work)). The development fixture takes
about 50 s in Debug and six minutes under ASan, most of the gate's sanitised stage.

What each task found and left, and how each was closed, is in [history](history.md), newest first.
