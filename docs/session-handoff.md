# Session handoff

## Next, on resuming — 2026-11-24 (R02 done, then R03)

R02 is done: [durable I/O, directories and the world lock](stage-5-recovery-plan.md#durable-io-directories-and-the-world-lock-r02-2026-11-24),
on Jettison's durable directories and file lock, pin `ea66afe`. `Host::OpenWorldDirectory` makes and
holds a world's directory, and nothing opens one yet. Next is R03, the immutable step journals and the
catalogue: it gives `--serve` its world directory, makes the world's identity, compiles in the game's
and the SDK's revisions, has `Command::Requests` list its commands, and injects I/O faults at its own
seam ([what R01 left it](stage-5-recovery-plan.md#what-it-leaves-each-task)). A remote view may be
192 KiB now (`Remote::kMaxMessageBytes`); past about 256 KiB an unreliable message does not arrive,
and a view wants deltas ([history](history.md#r02-durable-io-directories-and-the-world-lock--2026-11-24)).
W22, the drives past the torch, waits for the first larger hull. Jettison's station traffic lanes
were measured and parked (`fixture_traffic`, 2026-11-22), to be taken up when it fails. A commit is
gated in the second checkout, `build/gate-tree`, with `--tidy`, editing alone beside it
([gating beside the work](build-and-gates.md#gating-beside-the-work)). The development fixture takes
about 50 s in Debug and six minutes under ASan, most of the gate's sanitised stage.

What each task found and left, and how each was closed, is in [history](history.md), newest first.
