# Session handoff

## Next, on resuming — 2026-11-24 (R06 done, then the engine's ImGui split, then R07)

R06 is done: [the runtime distribution contract](stage-5-recovery-plan.md#the-runtime-distribution-contract-r06-2026-11-24),
a planning task with no code ([D-114](decision-ledger.md#d-114)). The user chose a standalone tarball for
x86-64 Linux with glibc 2.43 or newer, nix-ld on NixOS, with the TUI in it. Its one engine gap is
Jettison's `imgui` target, which carries ImGui's GLFW and Vulkan backends, so the TUI links
`libvulkan.so.1` through `Devtools`. Jettison splits that before R07 stages and tests the bundle.

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
