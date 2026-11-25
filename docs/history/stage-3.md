# Stage 3: authority and commands (2026-11-19)

Task records as they were written at the time, newest first. Moved from `docs/history.md` on
2026-11-25; the [history index](../history.md) lists every record.

## Stage 3 cross-review — 2026-11-19

Codex reviewed `9de7cb0..97f6e52` in seven rounds (`build/reviews/stage-3`), the last approving. Round 1's three findings:

- **F1:** a load or unload checked only that each side's sum was finite, so 1 t loaded from 2^54 t
  left the venue at 2^54 t, and unloaded onto it vanished. Each side is now refused as
  `InvalidQuantity` when rounding would change it by more than one part in 2^24 of the quantity,
  the plan's one residue rule; `company` holds both cases and a 0.1 t + 0.2 t move that only rounds.
- **F2:** a save loaded whose queue lacked a pending request (its retry answered queued for ever) or
  held two out of admission's order (run out of order). The engine's services could not see the
  queue, a Jettison gap: pin `bc4facb` gives `WorldService::Load` the queued commands, and
  `Requests` refuses a queue that is not its pending requests in order (`command`).
- **F3:** A06 made `Jettison::Devtools` required, which an SDK without presentation lacks, and
  `clang-notui` kept presentation on, so missed it. The console is built where the SDK has the
  engine's developer console, and `clang-notui` builds against an SDK without presentation.

Round 2 resolved F1 and F3, left F2 open on one actor's pending requests saved out of admission's
order, and found two more: a pending payload its command cannot read loaded and was lost when the
step took it off the ledger (F4), and capacity summed the lot's and the load's volumes apart, so
two loads could fill the starter's 20 m^3 hold to 20.000000000000004 m^3 (F5). Fixed after it:
`Requests::Load` holds each actor's requests to increasing order and refuses an unreadable
pending payload (`command`), and a load is measured over the lot it would store (`company`). The
user asked for a third round. It resolved F2 and F4, left F5 open on a new item's lot summed in another
order than the hold stores it, and found two more: a retry after a restart that registers another
version of its command was refused rather than answered from the ledger (F6), and a save was not held
to the payload and queue limits (F7). Fixed after it: the user selected whole IBCs for cargo
([D-078](../decision-ledger.md#d-078)), so a move counts IBCs and capacity is an integer count, and the
venue takes or gives the IBCs times `unitsPerCubicMetre` under F1's rule; `Admit` answers a retained
sequence before asking the commands; `Requests::Load` refuses a payload over 4,096 bytes and a queue
over 1,024 commands. The `company` suite's pinned replay hash moves with `Lot` and `Move`, now IBC
counts, and its fixture's quantities: `0x6754d1546a115533` to `0xe885f21ff9a48a55`.

A fourth round, at the user's request, resolved F5 to F7 and found one more: a hull's `CargoVolume` had
no upper bound, so content could pass 2^64 and its cast to an IBC count was undefined behaviour (F8).
The user then set the bounds and took warehouse holdings into IBCs too (D-078, 2026-11-20): a hold
carries at most 11,520,000 IBCs, the heavy destroyer's envelope, and a warehouse at most a hundred
of those; content and the commands each refuse a hull over it or not whole (`content`, `company`), a
save refuses a holding or lot over its limit (`world`), and an unload past the warehouse's is
`CapacityExceeded`. With every count an integer, round 1's residue rule is gone. The pinned hash moves
again with `Holding`, now IBC counts: `0x63a4858a587bffa5`.

A fifth round resolved F8 and found one more: a save was held to the hold limit lot by lot, so 11,520,000
IBCs of copper and one of tin, or 21 IBCs in the 20-IBC starter, loaded (F9). Fixed after it: a
cargo's lots are bounded together (`world`), and a ship's cargo against its hull's hold, which
`Company::RegisterCommands` gives `Identities` (`company`). Round 6 left F9 open on hulls with no
`CargoVolume`, whose empty map was taken for none given; a hull the map does not name now carries
nothing (`company`). Round 7 approved with no finding open.

## A07: Stage 3 closed — 2026-11-19

**Stage 3 is closed** ([delivered](stage-3-authority-plan.md#delivered-a07-2026-11-19)), each of its
checks held by a gated test. The company tests check what a player sees through `Query::Observe`,
reading the registry only for the principal, the polity and region and the identity counters. Four
cases are new: requests in one step run in admission order (two loads for one holding, two
provisionings for the one home left); a retried provisioning and load run once, and order, travel and
upgrade commands are refused as unsupported until Stage 4 registers them; two regions of one planet
keep their own accounts; and a save with a request pending and a replay from the start reach the
hosted world's hash, views and outcomes, pinned to one hash in every gate build (`company`, 8 cases).
Stage 4 is handed its order, travel and upgrade contracts. No Jettison gap: Stage 3 built on pin
`30ea3f3` throughout.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` at 24 workers passed 37 of 37 in
`clang-debug`, `clang-release`, `clang-asan` and `clang-notui`, the pinned hash holding in each, and
tidy clean.

## A06: the console apart from FTXUI — 2026-11-19

The console library and its tests moved from `tools/tui` to `tools/console`, so they build and run
with the TUI off; `manifest_tui` is only the FTXUI frontend ([what was settled](stage-3-authority-plan.md#console-a06)).
The console's existing commands are its sandbox, labelled so in `status` and the TUI. New hosted
commands drive at most one hosted world, which `host <scenario>` starts through `Host::Start`:
`provision`, `cargo load|unload`, `observe` and `step`. They go only through the world's admission
and `Query::Observe`, which a frontend reaches by `Host::HostedServices`. The sandbox's tick and the
hosted step move only their own clocks; a failed `host` keeps the world there was, and a successful
one replaces it with its queued requests (`console`, 10 cases). `Jettison::Devtools` is now required
whether the TUI is built or not, and the SDK-import check's expected list says so. No Jettison gap:
the session's `GetGame()` reaches Manifest's services.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 37 of 37 in `clang-debug`,
`clang-release`, `clang-asan` and `clang-notui`, and tidy clean.

## A05: queries and the observation boundary — 2026-11-19

The new `query` module copies what an actor may see into a value snapshot at the completed step
([what was settled](stage-3-authority-plan.md#queries-a05)): a player's own company, its credits,
stock, ships and cargo, sorted by ID; every company for the administrator; nothing of a market or
another company. Observing changes no state, and a snapshot is unchanged by later steps (`query`,
2 cases). The company tests still read the registry for their checks. No Jettison gap: a const
`SimWorld` gives the registry and the clock's step.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 37 of 37 in `clang-debug`,
`clang-release` and `clang-asan`, 36 of 36 in `clang-notui`, and tidy clean.

## A04: provisioning and cargo — 2026-11-19

The user selected a new company's starting credits, 25,000 ([D-077](../decision-ledger.md#d-077)). The
new `company` module registers three commands on A03's admission path
([what was settled](stage-3-authority-plan.md#provisioning-a04)): the administrator provisions a
player (principal, company and account, a `Polity` governing the first free home, the starter docked
at the region's home venue), and a player loads and unloads goods between their company's stock at
that venue and the ship's hold. Each checks everything before it writes: a second provisioning of
one principal is `AlreadyProvisioned`, and none mints; wrong venue, undocked, reserved stock,
capacity, nonfinite, nonpositive and another's ship write nothing. The volume conversion,
`Core::OccupiedVolume`, sits beside `ItemNode`; the content set records each home's venue by C09's
check. A load refuses broken stock or cargo (`company`, 6 cases; `world`, 7; `content`). No Jettison
gap: the docked bootstrap is Manifest's `Ship::dockedAt`, the engine's docking waiting for W05's
physics.

Evidence, on Lythir: `./scripts/ci.sh --werror --asan --tidy` passed 36 of 36 in `clang-debug`,
`clang-release` and `clang-asan`, 35 of 35 in `clang-notui`, and tidy clean.

## A03: admission and the request ledger — 2026-11-19

The new `command` module is the one way a request reaches the hosted world
([what was settled](stage-3-authority-plan.md#admission-a03)). Admission checks the actor, the
command, its version and payload and the sequence before it assigns a global order and queues the
envelope on the world; a refusal consumes no sequence. Requests run at the next step in admission
order, the second of two conflicting ones seeing the first's effects. Identical retries answer
from the pending or completed record, a reused sequence with another payload is a conflict, and a
pruned one is too old, never run again. Save and load keep the pending work, progress and results,
and a replay of the recorded envelopes rebuilds the world and the ledger to the same hash without
admission (`command`, 9 cases). No command is registered yet; A04 adds provisioning and cargo. No
Jettison gap: the engine's queue is saved and its applied commands are recorded with their step.

## A02: one hosted world — 2026-11-19

The user chose how a player takes a home region ([D-075](../decision-ledger.md#d-075)) and how a
server keeps free regions in supply ([D-076](../decision-ledger.md#d-076)), and renamed `EcsWorld` to
`EntityStore`. Stage 3's inventory was refreshed: the engine's archives now write string- and
enumeration-keyed maps, which the plan had listed as missing. The new `world` module puts the hosted
state on the session's `SimWorld` ([registry owner](stage-3-authority-plan.md#registry-owner)):
organisations, regions and their public and household accounts are seeded from the scenario, a
kind is part of every identity so equal text never aliases, and `Identities` saves its counters and
rebuilds its lookup on load, refusing a duplicate (`world`, 6 cases; `host`). The capacity
conversion's owner is settled for A04. `clang-notui` is a gate stage. No Jettison gap.
