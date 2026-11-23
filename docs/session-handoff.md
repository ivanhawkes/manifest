# Session handoff

## Next, on resuming — 2026-11-24 (R03 done, then R04)

R03 is done: [the step journals and the catalogue](stage-5-recovery-plan.md#step-journals-and-the-catalogue-r03-2026-11-24),
on pin `ea66afe` with no engine edit ([D-112](decision-ledger.md#d-112)). `manifest --serve` takes
`--world <directory>`: `Host::Journal` commits each run of steps before the server takes it,
`Remote::Server::Acknowledge` sends a queued receipt only then, and a start replays the committed
batches from the world's first step. Next is R04, boundary snapshots, retention and the replay from
a snapshot into a candidate world ([what R03 left it](stage-5-recovery-plan.md#what-it-leaves)):
nothing removes a batch yet, and an idle server writes 16 GB of blocks a day. Open for the user:
whether an idle server commits a second's steps ahead rather than every 21 ms. A world is its
build's (D-111), and a build is its trees (`cmake/Revision.cmake`), so any edit under `source` or
`cmake` makes a world of the build before unopenable: delete it. A remote view may be 192 KiB
(`Remote::kMaxMessageBytes`); past about 256 KiB an unreliable message does not arrive, and a view
wants deltas ([history](history.md#r02-durable-io-directories-and-the-world-lock--2026-11-24)).
W22, the drives past the torch, waits for the first larger hull. Jettison's station traffic lanes
were measured and parked (`fixture_traffic`, 2026-11-22), to be taken up when it fails. A commit is
gated in the second checkout, `build/gate-tree`, with `--tidy`, editing alone beside it
([gating beside the work](build-and-gates.md#gating-beside-the-work)). The development fixture takes
about 50 s in Debug and six minutes under ASan, most of the gate's sanitised stage.

What each task found and left, and how each was closed, is in [history](history.md), newest first.
