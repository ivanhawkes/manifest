# Session handoff

## Next, on resuming — 2026-11-24 (R01 done, then R02)

R01 is done, a planning task: [the save and compatibility contract](stage-5-recovery-plan.md#save-and-compatibility-contract-r01-2026-11-24)
defines the `Manifest.Save` section, what a build compares before it loads a save
([D-111](decision-ledger.md#d-111), mine, the user's to review) and the inventory of what a hosted
world saves, with no code and no engine edit. Next is R02, the durable I/O, directory creation and
world-lock contract, the first Stage 5 task likely to provision in Jettison; a message longer than a
packet is booked for Jettison's next change and rides with it
([history](history.md#stage-4-cross-review--2026-11-23)). W22, the drives past the torch, waits for
the first larger hull. Jettison's station traffic lanes were measured and parked (`fixture_traffic`,
2026-11-22), to be taken up when it fails. The SDK is pinned to `14fc23a`. A commit is gated in the
second checkout, `build/gate-tree`, with `--tidy`, editing alone beside it
([gating beside the work](build-and-gates.md#gating-beside-the-work)). The development fixture takes
about 50 s in Debug and six minutes under ASan, most of the gate's sanitised stage.

What each task found and left, and how each was closed, is in [history](history.md), newest first.
