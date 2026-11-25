# Notes moved out of the plans and references

Material taken out of the plan and reference documents on 2026-11-25, when they were cut to open
work and the feature documents took the built behaviour: superseded designs, evidence blocks and
measurement tables, kept as they were written.

## From build-and-gates

Written 2026-11-25 in the documentation restructure. Each item is copied unchanged from the source
named, where it stood before the rewrite. Links in them are relative to `docs/` as they were (so
targets moved into `docs/history/` today, such as `stage-5-recovery-plan.md`, now sit beside the
fragment's destination). Records in `docs/history/` and `docs/delivery-order.md` link three of
these headings as `build-and-gates.md#t5-selected-environment-parity-plan`,
`#t7-selected-editor-example-plan` and `#t10-selected-gate-and-promotion-policy`: repoint them to
wherever these land (stage-1.md, beside M01, M02, M04 and M05, fits).

### 1. T5, T7 and T10 plans (from docs/build-and-gates.md; done in M04, M05, M01 and M02, 2026-11-18)

### T5 selected environment-parity plan

[D-054](../decision-ledger.md#d-054) resolves the approach to the open D-006 deviation.
Desktop-only tools stay out of the shared shell: `lazygit` was removed from both repositories'
`devenv.nix` on 2026-11-18 and is installed on the desktop instead.

Implemented in M04 (2026-11-18): the pin is `fafa758`, and the three files match it byte for
byte. `scripts/check-env-parity.sh [repository]` reads the pin, fetches only that commit (from the
pin's `GIT_REPOSITORY`, or a given clone, whose committed objects alone are used) and diffs each
file against it; it never reads `JETTISON_SOURCE_DIR`. `scripts/test-env-parity.sh` runs it on
copies against a throwaway repository: a match, each file differing or missing, a file absent at
the pin, no pin, two pins, an abbreviated pin, an unknown pin, an engine's uncommitted edit and a
`JETTISON_SOURCE_DIR` at matching files. CI runs both straight after checkout, before Nix and
compilation. The handoff records the evidence. The plan as selected follows.

Review every intervening Jettison commit before selecting the new SDK revision; a newer sibling
checkout alone is not a reason to move the pin. Then update Manifest's immutable pin and copy all
three environment files from that exact committed revision together. Preserve the
byte-identity rule without a Manifest-only package exception.

Add a parity check to Manifest's CI before compilation. Its revision authority is
`MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`, not a second hard-coded SHA,
a moving branch or the current sibling checkout. Compare all three Manifest files against
the committed blobs at that revision. The checker may reuse an available engine clone that
contains the exact revision or obtain it independently; a local SDK override or uncommitted
engine edits must not substitute a different baseline. Missing files, an unresolved or
ambiguous pin, unavailable reference content and any byte difference fail with a useful
file/revision diagnostic rather than silently skipping the check. Keep the lock file intact
unless the selected SDK change itself requires updating it.

Implementer ownership: Jettison's shared environment and its verification first; Manifest's
SDK pin, matching environment files, parity tooling, CI wiring and workflow documentation
second. Keep README and AGENTS guidance aligned with the final verified state. This is T5's
parity deliverable, also referenced by T8; implement one check rather than separate copies.

Acceptance evidence includes a passing comparison of all three files against the selected
commit and rejection of a controlled difference in each file separately, including missing
reference content. Show that local overrides cannot bypass the comparison. Record the engine
verification and run Manifest's full gate from a clean checkout using its pinned fetch with no sibling-engine override. Retain
the replay expectations and record exact revisions, stages and logs under D-007. Apply T10's
selected CI order when its implementation has landed and use D-051's host-specific concurrency.
Environment parity alone does not establish Stage 1's CPU-only consumer or gameplay integration.

#### T10 selected gate and promotion policy

[D-052](../decision-ledger.md#d-052) and [D-053](../decision-ledger.md#d-053) select the following
implementation contract. The gate order (D-052) is implemented in `scripts/ci.sh` (M01,
2026-11-18); the triggers and promotion (D-053) in the workflow and `scripts/promote.sh` (M02,
2026-11-18), described [above](../build-and-gates.md#promoting-to-main).

Run formatting, Debug build/CTest, Release build/CTest, ASan/UBSan build/CTest, then
clang-tidy in that order for the full gate. Keep `--warnings-as-errors=*` for tidy; do not
introduce a warning baseline or suppress checks. Preserve the existing optional local flags:
`--skip-release` omits Release, `--asan` includes sanitizers and `--tidy` includes tidy.
Keep ordinary failure propagation: a tidy failure must fail the gate, after the requested
build/test stages have completed. This does not require continuing after an earlier build or
test failure, and a shortened local pass remains partial evidence.

`feature-base` is the integration branch. Its push runs the full gate on the resulting
commit. `main` receives only fast-forward promotions to an exact commit already tested on
`feature-base`; it receives no independent changes or promotion merge commits. A promotion
must verify a successful, completed full gate for that exact SHA under the applicable workflow.
An ancestor's result, a PR merge candidate's result, a partial gate, or a missing, running,
failed or cancelled run is insufficient. If the update cannot fast-forward, stop and resolve
the divergence through integration and verification rather than rewriting either branch.

Avoid the second full gate triggered solely by promoting the same SHA to `main`. Retain
PR candidate verification for changes targeting `feature-base`; ordinary feature-branch pushes
need not also run a full gate when the PR already provides that check. The integrated commit
still needs its own `feature-base` result. Explicit manual diagnostic runs remain available.
This policy removes duplicate promotion work; it does not equate distinct PR and integrated
commits or cancel one branch's required run when another branch advances.

Implementer scope: `scripts/ci.sh`, `.github/workflows/ci.yml`, the promotion procedure and
their workflow documentation. Before changing triggers, inspect repository rules and required
checks, and confirm that the verified integration result can authorize promotion without
weakening protection or inventing a passing result for skipped work. Any required repository
administration must be identified explicitly. `scripts/tidy.py` needs no warning-policy change.
No SDK or gameplay changes are required. Concurrency follows [D-051](../decision-ledger.md#d-051).

Acceptance evidence must include a controlled tidy failure after successful requested
build/test stages, with a nonzero gate result and logs showing those stages ran first; a
passing full gate on the final implementation; and promotion checks covering exact-SHA success,
unverified changes and divergence. Confirm that a normal verified fast-forward to `main`
does not start another full gate, while a new integration commit and an explicit manual run
still do. Record actual stages, logs and exact-commit CI evidence under D-007 before closing T10.

#### T7 selected editor-example plan

[D-055](../decision-ledger.md#d-055) selects repair of the tracked
`.vscode/launch.json.example` and `.vscode/tasks.json.example`, with setup instructions here.
Delivered 2026-11-18 (M05): the setup is [below](../build-and-gates.md#editor-setup), the evidence in the
[session handoff](stage-1.md#m05-the-editor-examples--2026-11-18).

Provide two launch entries using these Debug executables, confirmed against CMake targets and
the existing generated CTest commands:

| Launch | Executable relative to the workspace |
| --- | --- |
| Simulation unit tests | `build/clang-debug/source/game/sim/manifest_sim_tests` |
| Development TUI | `build/clang-debug/bin/manifest_tui` |

Remove the legacy launch arguments; the simulation suite requires no YAML path arguments.
The TUI uses its configured data paths and normal startup interface. Make configure, build
and test tasks explicitly use the `clang-debug` presets, with configure before build and
build before tests or debugger launch. Task labels and `preLaunchTask` references must agree.
Preserve Ninja-compatible targets and build dependencies for generated content; do not fall
back to the pre-rename top-level build tree or source-tree generated YAML.

Document copying the examples to ignored personal files, required editor/debugger extensions
and launching the editor/tasks in the development shell. Validate the chosen debugger adapter's
configuration fields rather than mixing adapters. Compilation follows D-051: the laptop stays
within its limit and the tasks respect overrides appropriate to Lythir. Preserve existing
personal editor files when updating the tracked templates.

Acceptance: copy the examples in a fresh checkout, configure and build through the tasks,
run the Debug CTest suites, and launch both entries through the debugger. Reach a source
breakpoint in each and continue the simulation suite to success and the TUI to normal exit.
Record the debugger/extension used and observed results. Inspecting JSON or finding an old
binary alone does not establish working editor integration. No SDK, gameplay or test-source
change is needed for this repair; this check is separate from the full gate's evidence.

### 2. Superseded verification note (from docs/build-and-gates.md, "Verification evidence")

Before the rewrite, the section also required each gate to be recorded in the session handoff with
"the commit, the presets and suites that passed, the three state hashes (generation, 100-tick, raw
price) and the log path". The rewrite records gates in the task's history record instead, and no
longer names the three hashes. Its paragraph on the evidence of 2026-11-16:

The newest local evidence is the
[T1/T9 verification record](planning-2026-11-15.md#t1t9-local-verification--2026-11-16), including
the tested tree, full command, presets, suites, replay hashes and log path. Both tidy findings
identified in the [historical CI analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16)
are fixed. This does not establish engine integration or gameplay acceptance.

### 3. Proposed checks of 2026-11-16 (from docs/build-and-gates.md, handoff T8)

The devenv parity check it proposes was built in M04 (`scripts/check-env-parity.sh`); the link
checker and the boundary checks are still open and stay listed in build-and-gates.md.

Proposed checks, not present (handoff T8): local Markdown link and heading-anchor integrity across
`docs/`, `README.md` and `AGENTS.md` — an ad-hoc scan at the 2026-11-16 assessment covered 31
documents and roughly 300 local targets, found two stale heading anchors in `docs/coding-standards.md`
pointing at Jettison headings, and those links now use `../../jettison/docs/…`; the
checker lives in the analysis appendix until an implementer commits it to `scripts/` and CI. Also
proposed: a `devenv.nix`, `devenv.yaml` and `devenv.lock` parity check against the pinned engine
revision, and the CMake dependency-closure
boundary checks in the [boundary assessment](engine-game-boundary-assessment.md#enforcement-to-implement-later).

### 4. The Work 1-5 planning table (from docs/agent-workflow.md, "Roadmap planning"; all five done)

### Roadmap planning

Before planning an integration, consult [development stages](development-stages.md),
[gameplay requirements](gameplay-and-server-plan.md) and the
[engine/game boundary assessment](engine-game-boundary-assessment.md). Record a small
capability map: required Manifest mechanic, existing public engine API, demonstrated gap,
owner, and acceptance evidence. Jettison implementers must inspect Manifest's current
requirements and provision reusable missing capabilities in engine code; Manifest
implementers supply the game's rules, schemas and content. Consult the selected SDK's
`docs/game-integration.md` when available. Keep proposed APIs distinct from implemented ones.
The planning-only policy above applies to this cross-repository work too.

| Work | Planning output |
| --- | --- |
| 1. Host boundary and script capabilities | State ownership, host/script contracts, capability limits and acceptance criteria |
| 2. Decimal market prices | Precision, overflow and rounding rules, affected-field inventory and migration plan |
| 3. VFS, layered definitions and schema migration | Loader inventory, layering and migration semantics, compatibility and validation plan |
| 4. Fixed-step commands, saves, replay and hashing | Ordering and state contracts, save format decisions, replay and determinism checks |
| 5. Scripts, staging and an engine-hosted scenario | Capability/ABI design, packaging requirements and scenario acceptance criteria |

A handoff should name the agreed design, affected files or modules, constraints, acceptance
criteria, required checks and unresolved questions. The implementer applies changes, reviews
integration and runs the relevant checks and final gate. Build concurrency follows
[D-051](../decision-ledger.md#d-051); local clang-tidy remains opt-in as described in
[build and gates](../build-and-gates.md).

### 5. The session handoff as it stood on 2026-11-25 (from docs/session-handoff.md)

Replaced by a short "where things stand" page. Its facts are in docs/history/stage-5.md and the
feature documents; some figures appear only here (a flush holds the server's loop about 11 ms, a
snapshot about 60; the development fixture about 50 s in Debug and six minutes under ASan).

#### Next, on resuming — 2026-11-25 (R09 done, Stage 5 closed)

R09 is done: **Stage 5 is closed** ([the close](stage-5-recovery-plan.md#stage-5-closed-r09-2026-11-25)),
on pin `13c1bec`. The `host` suite pins the state inventory to the names a world's save lists
(`SimWorld::ComponentNames`, `ServiceNames`). A world opened under other content is refused naming the
first definition that differs, and a pruned request sequence is held too old across a load. The
bundle passed on Lythir and in Ubuntu 24.04 and Debian 13. The Release figures are recorded: a flush
holds the server's loop about 11 ms, a snapshot about 60. Saves are to leave the step's thread
([D-115](../decision-ledger.md#d-115)), not yet built. The later workstreams (L01 onwards) wait until the
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
[D-113](../decision-ledger.md#d-113)): a crash loses up to 30 s. A world is its build's (D-111), and a build is its trees
(`cmake/Revision.cmake`), so any edit under `source` or `cmake` makes a world or backup of the build
before unopenable: delete it. A remote view may be 192 KiB (`Remote::kMaxMessageBytes`); past about
256 KiB an unreliable message does not arrive, and a view wants deltas
([history](stage-5.md#r02-durable-io-directories-and-the-world-lock--2026-11-24)). W22, the drives
past the torch, waits for the first larger hull. The later workstreams (L01 onwards) wait until the
user has played the first loop for a day or two (2026-11-24). Jettison's station traffic lanes were
measured and parked (`fixture_traffic`, 2026-11-22), to be taken up when it fails. A commit is gated
in the second checkout, `build/gate-tree`, with `--tidy`, editing alone beside it
([gating beside the work](../build-and-gates.md#gating-beside-the-work)). The development fixture takes
about 50 s in Debug and six minutes under ASan, most of the gate's sanitised stage.

What each task found and left, and how each was closed, is in [history](../history.md), newest first.


### 6. The 2026-11-16 identifier incident (from AGENTS.md, "Verification and CI")

Cut to the rule and a link; the full record is history/project-analysis-2026-11-16.md, "CI failure
analysis". The paragraph as it was:

- **Tool output in this project has emitted identifiers that do not exist.** The 2026-11-16 analysis
  saw two CI run records (43806887285, 43806869597, "created 2026-11-16T04:21Z") and a push rejection
  (`9ea337b..7c0a307`, "Docs: record the running-world economy decisions") that resolve to nothing on
  GitHub: the records 404, `gh api` reports _No commit found for SHA_ for both SHAs, no forced push
  event exists, and `FETCH_HEAD` plus the remote commit list show linear history to `531295e`. Nothing
  was lost. Confirm any externally-reported run or commit identifier against a second source (`gh api`,
  `git ls-remote`, `FETCH_HEAD`, the remote's commit list) before reporting it or writing it into docs;
  see the data-quality note in
  [CI failure analysis](project-analysis-2026-11-16.md#ci-failure-analysis--2026-11-16).

## From economy-and-development-plan

Source: `docs/economy-and-development-plan.md` at manifest HEAD `6842e8b` (before the 2026-11-25
rewrite; the full text is in git). Each item is a record that left the plan; none is current contract.

- **2026-11-15, preamble ("Product planning, 2026-11-15").** The plan began as a requirements-only
  assessment ("no code, content, SDK pin or development environment was changed"), then took the
  regional correction (population, households, condition and budgets per economic region of a
  planet; concessions and port opportunities per region) from regional-economy-and-governance-plan.md.
- **2026-11-15 to 2026-11-21, "Confirmed rules".** A summary list restating every selected rule
  (LPG, instant credits, physical chain, brokers, households, condition, wage tax, monetary rule,
  payroll, Bronze, core/frontier, the 72 h concession). The built rules now live in economy.md,
  trading.md, pricing.md and governance.md; the open ones in this plan and
  planetary-development-plan.md. Its LPG item was done in Stage 2 (`ItemLPG`, data/trade-items/fuels.yaml).
- **Basic medical supplies and staple grain handoffs (selected; implementation handoff).** Content
  handoffs for `ItemBasicMedicalSupplies` (now data/trade-items/medical.yaml) and the four grains
  (data/trade-items/grains.yaml). Done in Stage 2. The plan's "kilograms" and `misc.yaml` were
  superseded: the grains trade by the tonne (D-064).
- **Aggregate households; health, happiness and production; production-batch payroll; wage-tax
  withholding; initial monetary rule (all "selected").** The design text of W04 and W19 (done
  2026-11-20 and 2026-11-22). The basket first had three needs and W19 widened it to five on
  2026-11-21 (legumes for staple food, salt, soap). Built behaviour: economy.md.
- **Planetary tech and the Bronze metallurgy example (selected).** The W12 design (done
  2026-11-23). The plan's 90 kg copper + 10 kg tin and 9 kg + 1 kg batch became 9 + 1 totes
  (D-087, D-101). Built: governance.md; recipe production stays open in this plan.
- **Each section's "Required implementer evidence ... no fixture, content or test was changed or
  executed".** Pre-implementation evidence lists for W04, W09, W12, W14; the checks now run as gated
  tests in those tasks.
- **"Capability map, delivery and evidence".** A table of each requirement's pre-W03 evidence
  (civilisation-based owner fields, `Quotes`/`QuoteFor` without an observation gate, the
  development `PlanTrade`/`AdvanceTick`), "Suggested slices" (owner/location accounting,
  extraction job, finite broker, idle trade, tech project, core/frontier, rights) and an
  "Acceptance" list. Most slices were delivered in Stage 4; extraction, the frontier and rights
  remain (this plan's physical chain; planetary-development-plan.md L05, L06, L12). The acceptance
  list's starter life-support figures (30 kg oxygen, one 30-occupant-day cartridge, the 20 m3 hold)
  belong to W06 (stations.md, companies.md).
- **"Core and frontier", "Real-time exclusive exploitation", "Corporate power".** Moved to
  planetary-development-plan.md on 2026-11-25 (core/frontier and the concession, L06); the
  corporate-power circulation rules stay here as "Long-run circulation".

## From planetary-development-plan

Source: docs/planetary-development-plan.md as committed (a planning assessment recorded
2026-11-15, against Manifest `feature-base` 8f02d54 and Jettison `feature-base` 0564f19, SDK pin
f850920). Moved out on 2026-11-25 when the plan was rewritten to hold open work only. These
sections are records of that assessment: the capability map predates the space extraction (its
engine paths for Galaxy, StarSystem, Routes, Lanes and Gates are now Manifest's
`source/game/space`, and `Dock::Station`/`RequestPad` are now `Dock::Platform`/`RequestBerth`);
delivery amendments 1 to 4 were delivered (Stages 1 to 5); 5 and 6 are the queue's L03 to L06 and
L08 to L13. The home-region model in them was superseded by D-089 and D-110.

### Capability map and demonstrated gaps

| Required mechanic | Existing public provision/evidence | Missing work and owner | Acceptance evidence |
| --- | --- | --- | --- |
| Governed idle world | Jettison `SimWorld::Step`, `WorldService`, commands, save/load/hash; Manifest world/effects components | Manifest control rights, independent budgets, saved project/policy state; Jettison neutral CPU host/build provision | AI and player projects progress with zero clients; replay and restore agree. |
| Scoped development | Manifest `ProgressionManager`, `CivilisationEffectEngine`, habitat/service skill definitions | Manifest separates physical planet, region, organisation and immutable definitions from per-owner/per-region tiers, projects and effect scope | Multiple regions on one planet retain distinct population, public/household funds, condition and control. A local project affects only its target; rejected spending leaves all treasuries unchanged. |
| Public tenders and corruption | Jettison fixed-step commands, saved/hashed world services and archives; no Manifest tender/bribery system was demonstrated | Manifest competitive bid/award/fulfilment lifecycle, government corruption trait and abstract influence expense after clean tendering | Honest and corrupt award cases, invalid bids, deadline boundaries, separate funds, exactly-once costs/payment and replay/recovery. |
| Purchase of AI control | Jettison command handlers, serialised state and deterministic execution | Manifest offer eligibility, asset/liability scope, atomic transfer and request ledger | Race, unauthorised buyer, changed offer, retry and restart tests. |
| Company power over institutions | Manifest companies/markets are planned; no government/media influence system was demonstrated | Manifest corporate holdings, contracts, leverage and rival response rules | A defined economic dependency permits a specific influence action; effects and limits survive replay. |
| Company-controlled stations | Jettison `Docks`, `RequestPad` and compatibility-aware insertion queues; no admission/priority hooks were found | Jettison reusable admission/refusal and queue-ranking seams, with startup `Park` bypass scoped; Manifest construction, corporate ownership, fees and contracts | Independent docking-policy consumer plus Manifest build-once, fee-once, deterministic priority, equal NPC/player refusal and saved-policy/queue tests. |
| Resource exploitation and puppet colonies | Manifest production, stockpiles, body effects and authored habitat skills; no complete colonial government or extraction loop was demonstrated | Manifest regional site eligibility, resource/project/colony schemas, company-control links and public procurement; reuse/provision public spatial/docking facilities | Eligible regions on one planet retain independent initial port opportunities and resource grants. Reject duplicate funded claims and unsolicited builds in other players' regions; allow competing frontier ports at expiry without transferring government control. |
| Remote TUI/GUI play | Jettison capture/shown-component snapshots and controller primitives | Jettison reusable transport/session/encoding provision; Manifest authentication policy, commands, filtering and frontend views | Connect/disconnect, private-view filtering, stale revision and retry tests without FTXUI/rendering. |
| More physical systems | Jettison `Galaxy`/`StarSystem` resolved YAML data, `PhysicsWorld::DefineStarSystem`, `LaneTravel::DefineStarSystem`/`SetGalaxy`, `PlotRoute` | Manifest frontier generation/policy; demonstrate whether existing composition suffices, then provision minimal reusable topology publication/identity gaps in Jettison | Independent two-to-three-system consumer plus Manifest expansion without broken transit. |
| Stargate delivery and controlled access | Galaxy positions, lane-based routing/transit, `Gates::SetTollRule`, commands/services/archives | Manifest funded unmanned FTL expeditions, running-time arrival and gate-owner policy; Jettison reusable ungated-transit, policy-context or topology provision where existing APIs do not suffice; see the stargate capability map | An initially unconnected destination opens only after delivered/operational gate, with zero-client progress, shutdown/recovery correctness and owner-controlled access/fees. |
| Persistent expanded world | Jettison `SimWorld::Save/Load/StateHash`, game save sections and reflected `OnLane` | Physics system definitions are not saved/hashed; Manifest must persist generated world data through SDK archives and bind its identity into recovery; generic missing archive/topology facilities belong in Jettison | Restart outside the source tree reproduces expanded topology, state hash and active journeys. |

Source inventory: Manifest [Simulation.h](../../source/game/sim/include/manifest/sim/Simulation.h),
[Components.h](../../source/game/ecs/include/manifest/ecs/Components.h),
[ProgressionManager.h](../../source/game/progression/include/manifest/progression/ProgressionManager.h),
[CivilisationEffects.cpp](../../source/game/effects/src/CivilisationEffects.cpp) and
[habitat definitions](../../data/skill-tree/habitats.yaml). Engine evidence is in
`source/engine/celestial/include/jettison/celestial/{Galaxy,StarSystem}.h`,
`source/engine/ai/include/jettison/ai/{Routes,Lanes}.h`,
`source/engine/dock/include/jettison/dock/Docks.h`, `source/engine/dock/src/Docks.cpp`,
`source/engine/physics/include/jettison/physics/PhysicsWorld.h`,
`source/engine/physics/src/PhysicsWorld.cpp` and
`source/engine/sim/include/jettison/sim/{SimWorld,Snapshot,SaveGame}.h` in the selected SDK.

The inspected engine has registration and route-building pieces, not a demonstrated live
expansion protocol or public procedural galaxy generator. Name-derived system IDs coexist
with positional route indices; collision/rename rules need review. Do not present these
gaps as solved, require Manifest to use private app headers, or create a second custom
loader/physics scheduler. The implementer commits reusable engine provision before updating
Manifest's immutable pin and matching development environment.

### Delivery amendments and acceptance

Retain existing stage numbers as foundational work packages, with these planning amendments:

1. Host/content/authority: retain CPU-only composition and shared loaders. Resolve planet,
   company and public-budget identity before freezing authoritative schemas. Keep the small
   trading fixture; add governance definitions without implementing an entire political model.
2. Network/idle slice: move the TUI to real remote commands and snapshots after minimal
   authority exists. Prove server progress without clients, physical auto-trading, NPCs and
   reconnect continuity. The old optional administration-only TUI is superseded.
3. Regional development: multiple economies on one planet, one controlled home region per
   participant, AI neighbours, bounded
   saved development queues, meaningful local effects and separate public/company finances.
   Public procurement uses competitive tenders; add the simple corruption/bribery slice
   after the clean tender lifecycle works.
4. Persistent release: extend stage 5's journal/recovery contracts to control rights,
   project reservations, automation, government budgets and request outcomes. No production
   durability claim before its fault/restart evidence exists.
5. Corporate growth: eligible AI regional control purchases and per-region portfolio policies, then
   infrastructure/essential-supply/access-contract leverage and company-built stations with
   owner fees, shipping priority and rival denial, then physical resource exploitation and
   company-controlled colonies at eligible planets.
   Preserve player-region acquisition protection and late-join capacity; do not invent a win threshold.
6. Frontier: static multi-system navigation first; then deterministic, durable expansion,
   claims and one physical settlement/terraforming scenario. This does not block the first
   governed-planet idle loop. Broader political/media simulation follows proven mechanics.

Required scenario: multiple players receive distinct regions on one planet; AI neighbours run; one player
starts development and trading and disconnects; work progresses; a late joiner receives a
valid reserved region; reconnecting shows the same control and separate budgets. A later
scenario purchases an eligible AI region while refusing a player-region purchase, allowing
independent regional port opportunities on the same planet. A final
frontier scenario expands with an in-flight ship, restarts, supplies a colony physically and
continues from identical topology/project state. Each scenario checks unauthorised requests,
duplicate requests, bounded histories/queues, inventory accounting and deterministic replay.

The implementer should run long simulated workloads for short and extended server-life
profiles, measuring economic concentration, entrant viability, project/route costs, memory,
save growth and recovery time. Fast stepping tests determinism and game pacing but do not
prove month-long real-time availability. Include a real unattended soak separately. Profile
before changing route selection/indexes or adding simulation parallelism.

Use relevant presets and both repositories' required gates for touched implementation, with
each machine's concurrency (D-051), local clang-tidy opt-in for this cross-repository work.
Validation is limited to source/API inspection and documentation whitespace/link checks; no builds,
economy runs, benchmarks or recovery tests were performed for this assessment.

### Next planning cycle

The [contract/booking handoff](procurement-and-station-contracts-plan.md) proposes the first
clean tender lifecycle and station commitment boundary. The [economy/development plan](../economy-and-development-plan.md)
records instant credits, physical commodities, brokers/handling, material/facility-gated tech
and core/frontier generation. Partial payment, booked terms and temporary running-time
concessions followed by rival ports are confirmed. Supplier-funded performance security is
selected with a starting 10% bond and proportional forfeiture. Funded construction-site
reservation, non-refundable mobilisation, a fixed completion deadline and reopening on
abandonment/expiry are also selected. Frontier exclusivity covers eligible resources in one
named region, starting at operational port plus concession activation, with a disclosed
provisional 72-hour server-running term. Neighbours retain independent eligibility and timers;
government control stays separate. Regional construction and shared service scope are selected;
owners may serve multiple regions on the same planet under their offered terms, with explicit
physical delivery, independent economies/rights and protected bookings. Detailed transport
remains open. Starting regions require continuing public access to basic docking, trading and
regional delivery, with an operational starter-compatible station and physical delivery route
at allocation. Require an available working life-support connection and spawn connected;
reject missing or exhausted service capacity, preserving the connection through retry/recovery
without duplicate spawn/resupply. Fees/capacity/funds still need balancing. Cultural styling, corporate branding and distinct customisable
station appearances are recorded for later presentation design. The first
[material/facility tech example](../economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
is Bronze metallurgy, using local copper, imported tin and an operational workshop. Detailed
settlement, construction amounts/deadline/salvage and long-running demand/public-income
circulation remain design work; selected quantities, throughput and the exclusivity term
still need economic/pacing validation.
Keep no-win open-ended play, company identity, player-region acquisition protection and late-join
capacity as constraints. Server pacing, detailed influence rights, acquisition pricing and
expansion triggers remain open choices.

## From sandbox

Source: docs/model.md, "Units, prices and storage", lines 575-642, as of 2026-11-25. The item
prices were set from these quotes on 2026-11-19; the current prices are the `basePrice` fields in
data/trade-items/*.yaml, whose comments also name their sources. Kept as a record.

### The 2026-11-19 price quotes (recorded 2026-11-19, from docs/model.md)

Prices are the market figure for the traded form of the good, not for a purer or rarer
form of it: uranium is the spot price of `U3O8` concentrate, the pharmaceuticals are bulk
active-ingredient prices per kilogram, and the alloys are the LME value of the metal in
them. Where the market quotes a unit the game does not measure, the price is converted:
the pound to the kilogram, the bushel and the hundredweight to the tonne, and the yuan and
the Australian dollar to the US dollar at the week's rate (6.68 and 0.678).

The quotes of 2026-11-19 (the latest each source published, and its date where earlier):

| Goods | Price | Source |
| --- | --- | --- |
| Copper, tin, aluminium, zinc, nickel, lead | 13226, 46650, 3061, 3776, 15790, 1840 a tonne | LME cash settlement, 17 November |
| Brass, bronze | 9918, 16568 a tonne | LME value of 65:35 copper and zinc, and of the 9:1 copper and tin of `RecipeBronze` |
| Iron | 458 a tonne | Brazilian pig iron FOB, November |
| Cobalt | 33525 a tonne | indicative, 13 November |
| Antimony | 17600 a tonne | Fastmarkets, Rotterdam, 4 November |
| Lithium, neodymium | 90, 138 a kilogram | Chinese metal benchmarks, October and 4 November |
| Molybdenum, vanadium | 76.5, 26.6 a kilogram | LME Molybdenum (Platts) 34.69 a pound; ferrovanadium 80%, Europe, 7 October |
| Uranium | 198.7 a kilogram | U3O8 spot, 90.12 a pound, 18 November |
| Gold, silver, platinum, palladium | 3725, 45.03, 1371, 949 an ounce | spot, 18 November |
| Rhodium, ruthenium | 8550, 1900 an ounce | dealer bids, 17 and 18 November |
| Wheat, oats | 209, 229 a tonne | CBOT, 5.70 and 3.33 a bushel, 18 and 5 November |
| Barley | 244 a tonne | Australian feed barley FOB, A$360, 11 November |
| Rice | 494 a tonne | Thai 5% broken, 18 November |
| Cocoa seeds | 5145 a tonne | ICE cocoa, 18 November |
| LPG | 720 a tonne | Saudi Aramco propane contract price, November |
| Oxygen | 0.26 a kilogram | US bulk liquid oxygen, 263 a tonne, mid-2026 |
| Ammonia, sulfur | 585, 733 a tonne | CFR Far East and CFR China, 12 November |
| Potash | 365 a tonne | granular MOP CFR Brazil, 10 November |
| Caustic soda | 840 a tonne | 98% flakes FOB Mersin, 1 November |
| Sulfuric acid | 170 a tonne | Q3 2026 assessments |
| Ethylene | 1100 a tonne | CFR Northeast Asia |
| Yellow phosphorus, natron | 4261, 151 a tonne | Chinese spot, 16 and 5 November (natron as dense soda ash) |
| Graphite | 323 a tonne | Chinese flake, -194 mesh, 4 September |
| Crude oil | 92.18 a barrel | ICE Brent, 18 November |
| Coal, coke | 145, 334 a tonne | Newcastle thermal, 17 November; Chinese metallurgical coke FOB, October |
| Steel | 845 a tonne | Western European hot-rolled band, SteelBenchmarker, 11 November |
| Titanium | 6.1 a kilogram | Chinese sponge, 13 October |
| Copper, tin and iron ores | 3446, 25190, 90.7 a tonne | iron ore 62% Fe, 18 November; the concentrates at the payable LME value of their 27% copper and 60% tin |
| Timber | 224 a cubic metre | CME lumber, 527.50 per thousand board feet, 17 November |
| Livestock | 4845 a tonne | CME live cattle, 219.77 a hundredweight, 18 November |
| Cotton | 1653 a tonne | ICE cotton, 74.97 cents a pound, 18 November |
| Rubber | 2492 a tonne | SGX TSR20, 249.2 cents a kilogram, 17 November |
| Plastics | 1269 a tonne | HDPE CFR Southeast Asia, October |
| Urea, diammonium phosphate | 535, 803 a tonne | Egyptian granular FOB, November; DAP FOB US Gulf, October |
| Natural gas | 27.88 an MMBtu | JKM, the North Asian LNG cargo price, 18 November |
| Sugar, coffee | 398, 6086 a tonne | ICE raw sugar No. 11, 18.04 cents a pound; ICE arabica, 276.05 cents a pound, 18 November |
| Spices | 7350 a tonne | Vietnamese black pepper export quote, November |
| Tea | 3.0 a kilogram | Mombasa auction, November |
| Wool | 11.68 a kilogram | AWEX Eastern Market Indicator, A$17.22 clean, 11 November |
| Preserved meat and fish | 7114 a tonne | Argentine frozen beef export average, August |
| Copper wire | 14500 a tonne | LME copper with a rod and fabrication premium |
| Batteries, solar panels | 108, 148 each | BNEF pack average of 108 a kilowatt-hour; modules at 0.37 a watt |

The goods of [D-066](../decision-ledger.md#d-066) with no market quote carry estimates of their
wholesale value: bricks 0.6, cloth 8 a square metre, tools 15, machinery 30000, lenses 150,
gunpowder 25 a kilogram, paper 950 a tonne, radios 400, silicon wafers 120, AI accelerators
25000, small arms 600, medicinal herbs 12 a kilogram, liquid hydrogen 6 a kilogram, helium-3
18.6 million a kilogram (2500 a litre of gas) and a habitat module 50 million; and for
[D-068](../decision-ledger.md#d-068) diesel 900 a tonne, tobacco 5 a kilogram, clay 150 a tonne, vaccines
10 a dose, mining drills 50000, robots 50000, satellites 5 million, reactor fuel assemblies 1.5
million, water ice 1000 a tonne, heavy weapons 250000, media 20, and passengers, prisoners and
tourists 1000, 2000 and 3000 a person.

Every other good has no market quote, and keeps its first-draft price converted to its
unit: a kilolitre of a heaped good was the cube's heap, so the tonne price is the kilolitre
price over `density x packFactor`, and a kilolitre of a liquid was the liquid, so the price
is over `density` alone.

## From space-plan

- 2026-11-15, docs/stargate-expansion-plan.md (intro): the stargate plan was written as planning
  requirements only. Its inspection used Manifest `feature-base` at `066b5cc` with the planning
  edits of the day, the pinned Jettison SDK `f85092027eda49c29001f7589608df1f0b8827fb`, and
  Jettison's integration guidance at `0564f19`. No implementation, assets, configuration, SDK change
  or build/gate run took place. Its capability map named Jettison SDK types (`Celestial::Galaxy`,
  `Ai::PlotRoute`, `Ai::EnterLane`, `Ai::OnLane`, `Ai::Gates::SetTollRule`, `Celestial::Resolve`,
  `Gates::DefineStarSystem`, `MakeGates`) that moved to Manifest's `Manifest::Space` in the space
  extraction of 2026-11-23; docs/space-plan.md carries the map under the new names.
- 2026-11-15/16, same file: a "regional correction" paragraph (after the regional economy plan) said
  a government expedition names its regional public account apart from a corporate sponsor and the
  physical planet and system IDs, and that region control grants no sibling treasury or whole-planet
  sovereignty. docs/space-plan.md keeps the rule (a government spends its region's public funds) and
  links governance for the rest.
