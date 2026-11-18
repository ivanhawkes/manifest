# Prioritised delivery order

Execution order for the existing [five-stage plan](development-stages.md), its
maintenance handoffs and subsequent delivery amendments. Prepared 2026-11-16.
This is a scheduling index: linked plans retain the contracts and acceptance criteria.
It does not select unresolved gameplay values or mark implementation complete.

## How to use this order

Work from top to bottom. A dependency is complete only when its required checks and
evidence are recorded, not when a patch or stub exists. Each row names its direct
prerequisites; their prerequisites are inherited. Rows with independent prerequisites
may be developed concurrently, but this list supplies the default priority for one
implementer. The maintenance sequence is the agreed work order, except T7 (M05), which
does not gate Stage 1 (2026-11-18): editor repair is not a technical dependency of the
host API, so H01 follows M04 and M05 travels alongside Stage 1.

Jettison provisions reusable SDK gaps first. Before a dependent Manifest change lands,
commit the engine provision, make it fetchable and consume the compatible immutable
pin with matching environment files. A local override is development evidence only.
Checks accompany each slice; stage exit rows consolidate final evidence rather than
deferring all testing to the end. Follow [D-051](decision-ledger.md#d-051) for concurrency
and the [gate policy](build-and-gates.md).

Implementers may write useful stubs under [D-056](decision-ledger.md#d-056).
Planning-only restrictions on OpenAI implementation remain in force.

## Completed prerequisites

[T1/T9](session-handoff.md#t1t9-local-verification--2026-11-16) established the
model-library baseline and fixed the two tidy findings. Do not repeat them as open
tasks.

## Status, 2026-11-18

Done: M01–M04 and H01 in Manifest ([session handoff](session-handoff.md)); H02–H05, and the
Jettison rows A01 and W01 ahead of their place in this order, in Jettison
([history](../../jettison/docs/history.md#neutral-sdk-and-manifest-integration)). All of these are
in the SDK pin `fafa758`. Two consequences:

- H06 builds against the pin itself: the host it adapts (`Jettison::Host`) is already fetched,
  so the local override is optional, not required.
- H07's pin and environment update happened in M04. What H07 still has to do is the acceptance
  matrix, the fetched-consumption check, and settling the engine cross-review of H02–H05, A01
  and W01, which cannot start before 2026-11-21. If that review changes the host API, the pin
  moves again inside H07.

A01 and W01 were built against these plans before C01 settled the planet, region and company
identities. Before A02 binds hosted state, C01 confirms that a Jettison frame
(`Sim::InFrame`, a partition the game chooses) and a save's details (`SaveInfo::details`, the
game's own strings) still fit. A gap that turns up there is a new Jettison row, not a reopening
of A01. M05 is still open, and runs alongside Stage 1.

## Priority 0: maintenance before Stage 1

Canonical scope: [T10](build-and-gates.md#t10-selected-gate-and-promotion-policy),
[T5](build-and-gates.md#t5-selected-environment-parity-plan) and
[T7](build-and-gates.md#t7-selected-editor-example-plan).

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 01 / M01 | T10: move strict tidy after all requested build/test presets; prove controlled failure propagation. | T1/T9 |
| 02 / M02 | Finish T10: inspect repository protections, implement exact-SHA promotion and CI triggers, verify the final workflow and record evidence. | M01 |
| 03 / M03 | T5, both repositories: keep desktop-only tools out of the shared SDK environment (`lazygit` removed from both, 2026-11-18) and keep the three files identical. | M02 |
| 04 / M04 | Finish T5: update Manifest's pin/environment together; implement the pin-based parity check and its failure cases; verify pinned consumption. | M03 |
| 05 / M05 | T7: repair tracked editor examples and verify fresh-checkout tasks plus actual debugger launches. | M04 |

## Priority 1: Stage 1 host and bootstrap

Canonical scope and evidence: [Stage 1](stage-1-host-plan.md).

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 06 / H01 | Inventory the complete current SDK import contract and consumer-local packaging paths; reject partial SDKs and remove the moving-reference test fallback. | M04 |
| 07 / H02 | Jettison: implement the public host owner, lifecycle and reusable metadata primitives, with ordered startup, failure cleanup and lifetime tests. | H01 |
| 08 / H03 | Jettison: make presentation/editor/samples optional without losing CPU modules or the packer; prove a fresh configure without graphics discovery. | H02 |
| 09 / H04 | Jettison: prove the independent non-space CPU consumer, definitions, deterministic stepping, save/load and teardown. | H02, H03 |
| 10 / H05 | Jettison: extract explicit sample composition/content, adapt Jetsam and Flotsam, preserve registration/save behavior and exercise an actual trader route. | H04 |
| 11 / H06 | Manifest: add the host adapter and finite startup-check executable; test explicit directory/pack content, contextual rejection, unrelated working directory and TUI-independent teardown. | H05 |
| 12 / H07 | Close Stage 1: commit/publish the validated engine, land the compatible Manifest adapter/import requirements with its pin/environment update, verify fetched consumption and the complete acceptance matrix. | H06 |

Do not require the future host target in H01 before its SDK exists. H06 may use the
explicit local override; H07 must prove the committed fetch. Add the Stage 1 dependency
closure/registration checks from T8 alongside the host and sample extraction.

## Priority 2: Stage 2 validated content

Canonical scope: [Stage 2](stage-2-content-plan.md). Recheck its older API inventory
against the delivered Stage 1 SDK before implementation.

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 13 / C01 | Finalise schema mappings, defaults/null handling, ordered IDs, units, scenario frames and distinct planet/region/government/company relationships. Inventory remaining authoring decisions. | H07 |
| 14 / C02 | Add reflected engine-readable definitions, generated record packaging and equivalence checks; define the canonical manifest/fingerprint builder and preserve exact price text and IDs/selections. | C01 |
| 15 / C03 | Add atomic aggregate loading, shared semantic validation, engine layering/migration wiring and failure rollback; compute content identity before publication, migrate callers and retire custom readers after their last caller moves. | C02 |
| 16 / C04 | T4 and item follow-ups: resolve required physical/storage fields and author tuning, grains, medical supplies, LPG, Life Support and hull definitions through the shared pipeline. Keep unselected values explicit and obtain decisions where required. | C03 |
| 17 / C05 | Author the separate trading and multi-region governance fixtures, including valid starting access/service capacity and required project/production references. | C04 |
| 18 / C06 | Add the validator CLI through the same loader; verify host agreement, explicit overlays, contextual provenance and failure behavior. | C03, C05 |
| 19 / C07 | Verify directory/pack content-manifest identity and add the validation build graph: validator → staged directory validation → pack → pack validation. | C06 |
| 20 / C08 | Close Stage 2: verify directory/pack identity, layering/migration, late-failure rollback and semantic rejection; run final gates and record the delivered schema/content contract. | C07 |

No task may infer physical cargo volume from a commodity's trading unit. Resolve
required authoring choices before their definitions/fixtures are accepted. D-050's
modular tank detail remains deferred; it does not block C04. Compact-berth runtime
integration is W01/W05, not a requirement to complete the neutral host.

## Priority 3: Stage 3 authority

Canonical scope: [Stage 3](stage-3-authority-plan.md).

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 21 / A01 | Jettison boundary prerequisite: resolve neutral frame/partition and save-metadata seams needed for hosted state, with compatible fixtures; commit and consume any SDK provision. **Done in Jettison 2026-11-18, in pin `fafa758`**; C01 rechecks the fit (see status). | C08 |
| 22 / A02 | Add reflected one-world state, distinct accounts/control identities, counters and serialisation; settle the registry-adapter and capacity-conversion owners, then prove index rebuilding. | A01 |
| 23 / A03 | Add trusted admission, fixed-step ordering, bounded retry/history state and structured command results. | A02 |
| 24 / A04 | Implement retry-safe provisioning and atomic cargo load/unload using validated docked bootstrap state, real account ownership and capacity checks. | A03 |
| 25 / A05 | Add immutable actor-filtered queries and result snapshots; define the observation boundary before exposing market views. | A04 |
| 26 / A06 | Separate command/query services and tests from FTXUI; preserve the development console without a second hosted simulation. | A05 |
| 27 / A07 | Close Stage 3: prove authorisation, conservation, rejection atomicity, sequencing/retries and in-memory save/replay with TUI disabled; hand over full order/travel/upgrade contracts. | A06 |

Provisioning may use a validated seeded dock/service state at this stage. It must not
offer arbitrary player teleport/dock commands or claim the later movement/service
runtime is implemented. Do not expose unfinished order/travel commands as successful.

## Priority 4: remote connection foundation

This inserts the required [network/TUI slice](roadmap.md#networked-idle-game-and-remote-clients)
after minimal authority. It does not wait for Stage 5 or promise durable responses.

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 28 / N01 | Select the bounded transport/authentication/protocol contract and map missing reusable SDK facilities; keep principal identity, game authorisation and admin rights distinct. | A07 |
| 29 / N02 | Provision demonstrated SDK gaps, then add the headless host loop and real TUI connect/disconnect through the command/query boundary. | N01 |
| 30 / N03 | Prove initial multi-client/reconnect behavior, filtered snapshots, bounded queues and world progress with zero clients. Only advertise operations already implemented. | N02 |

N03 proves connection and unattended stepping. Physical unattended trade is W10;
durable reconnect/retry evidence is R08. Market rows must remain unavailable until
their observation filtering in W07 exists; a connection prototype cannot leak them.

## Priority 5: Stage 4 trading, automation and regional development

Canonical scope: [Stage 4](stage-4-trading-plan.md), with the
[regional](regional-economy-and-governance-plan.md) and
[development amendments](planetary-development-plan.md#delivery-amendments-and-acceptance).

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 31 / W01 | Jettison: split the required generic physics/AI/scene capabilities from optional space adapters; provision needed docking/berth seams and verify independent consumers, then commit/pin. **Done in Jettison 2026-11-18, in pin `fafa758`** (see status). | A07 |
| 32 / W02 | Resolve outstanding settlement, transport, service and cadence choices; document the actual scheduler graph and transaction boundaries before integration. | W01, C08, A07 |
| 33 / W03 | Adapt marketplace reservations, matching, partial settlement, cancellation, expiry and fees to owner/venue accounts with checked atomic writes. | W02 |
| 34 / W04 | Integrate local production, finite brokers/demand, regional payroll/tax, essential purchases and health/happiness in the selected fixed-step order; eliminate automatic inter-venue stock transfers. | W03 |
| 35 / W05 | Integrate real departure, cruise, arrival, docking and cargo eligibility; prove starter/compact berth and capacity behavior where selected. | W01, W03, A04 |
| 36 / W06 | Implement working station life-support connections, onboard usage and explicit resupply under the resolved physical/accounting rules; preserve one-time starter grants. | W04, W05 |
| 37 / W07 | Implement live/stale observations, spawn reports for every directly connected station market, report age and actor filtering across queries and trader/script facts. | W03, W05, A05 |
| 38 / W08 | Implement company-funded cargo upgrades without sharing regional/civilisation funds; prove retry-safe payment/capacity changes. | W03, A04 |
| 39 / W09 | Prove the complete public-command buy → load → travel → dock → unload → sell → upgrade fixture, including directory/pack and cross-build replay. | W04, W05, W06, W07, W08 |
| 40 / W10 | Add enabled standing trade policies, autopilot and NPC jobs through the same physical commands and permitted market facts; prove operation without connected clients. | W09, N03 |
| 41 / W11 | Resolve the clean public-project/tender lifecycle's remaining rounding, deadline, default and regional delivery choices; define saved jobs, rights and reservations. | W10 |
| 42 / W12 | Implement competitive procurement, partial delivery/payment, performance security and a material-gated regional project through physical fulfilment. | W11 |
| 43 / W13 | Prove the multi-region idle governance scenario: independent accounts/condition, AI neighbours, one home region per participant, development queues, protected late joining and reconnect continuity. | W12, W10 |
| 44 / W14 | Close Stage 4: verify trading, market fog, automation and regional development together, in-memory continuation and final repository gates; record the actual schedule and state contracts. | W09, W13 |

W01 is technically independent of N01–N03; its later placement is the default priority,
not a dependency on transport. Likewise, W05, W07 and W08 can be developed separately
once their listed prerequisites pass. The final loop must integrate all of them.
Commercial private-port booking/policy changes follow later; the first public project
uses an accessible public destination and cannot depend on unfinished private ports.

## Priority 6: Stage 5 recovery and persistent release

Canonical scope: [Stage 5](stage-5-recovery-plan.md). Network, regional development,
observations, automation and accepted obligations already delivered must be covered.

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 45 / R01 | Define the final versioned Manifest save/compatibility section and exact content/state inventory; cover request outcomes, observations, regional rights, cadence and all active jobs. | W14 |
| 46 / R02 | Prove the required durable I/O, directory creation and world-lock contract; provision any missing reusable primitive in Jettison and consume its committed pin. | R01 |
| 47 / R03 | Implement ordered immutable step journals/catalogue and acknowledge durable acceptance only after the tested commit boundary, including autonomous empty steps. | R02 |
| 48 / R04 | Implement boundary snapshots, retention and startup replay into a candidate world; prove crash/fault behavior, compatibility rejection and retry preservation. | R03 |
| 49 / R05 | Implement bounded shutdown and verified backup/restore with storage/retention limits and interrupted-publication tests. | R04 |
| 50 / R06 | Select and document the supported runtime distribution contract, dependencies and writable-data locations. | R05 |
| 51 / R07 | Build and test the relocatable read-only distribution outside both checkouts/development environment; retain graphics-free consumer coverage. | R06 |
| 52 / R08 | Prove persistent multi-client reconnect/retry and zero-client recovery across trading, region control, projects, observations and automation. | R04, N03, W14 |
| 53 / R09 | Close Stage 5: consolidate fault/restart, backup, relocation, cross-build replay and final gate evidence; record measured I/O, recovery and storage limits. | R05, R07, R08 |

Only after R09 should production deployment readiness be assessed. Authentication and
network development occur earlier; this resolves the older Stage 5 wording that
groups them with later hosting work. No public deployment or capacity promise follows
solely from completing a local loop or connection prototype.

## Later priorities and re-entry conditions

These are existing follow-on workstreams, not prerequisites for the first loop.
Their order is a planning priority; unresolved designs still need decisions before
implementation. Every new authoritative mechanic must extend R01–R08's recovery and
retry coverage in its own delivery, rather than treating Stage 5 as permanently done.

| Priority / ID | Workstream | Must complete first / entry condition |
| --- | --- | --- |
| 54 / L01 | Deployment hardening, capacity profiling and unattended soak; assess the supported persistent service before public hosting. | R09; select operational platform, security and measured release limits. |
| 55 / L02 | Simple corruption/bribery after clean tenders, with preserved eligibility, separate expenses and recovery. | W12, R09; settle remaining scoring/cost choices. |
| 56 / L03 | Eligible AI regional-control acquisition and portfolio policies. | W13, R09; define pricing/transfer rules and prove player/home-region protection. |
| 57 / L04 | Private-port bookings and owner access/priority policy; Jettison admission/queue provision before Manifest policy. | W05, R09; settle service windows/failure terms and prove grandfathered commitments. |
| 58 / L05 | Corporate port construction and infrastructure/supply leverage on a static map. | L04, W12; resolve funded site, deadline/salvage and local delivery rules before enabling construction. |
| 59 / L06 | Resource exploitation and company-controlled colonies with independent regional grants/concession expiry. | L05, W13; resolve extraction, operating and colonial-government rules. |
| 60 / L07 | Modular oxygen banks and larger-hull scaling under D-050. | W14, R09; D-050's re-entry threshold is stages 1–4, with tank/storage/refill rules resolved before implementation. |
| 61 / L08 | Prove static multi-system navigation and existing-journey recovery before live expansion. | W05, R09. |
| 62 / L09 | Resolve expedition funding/equipment, commissioning, protected return and operating-supply rules; provision reusable ungated arrival/topology gaps. | L08; use the [stargate capability map](stargate-expansion-plan.md#capability-map-and-implementer-evidence). |
| 63 / L10 | Deliver a fixed gate pair to a pre-existing unconnected destination; validate once-only activation, ownership, permits, outages and shutdown-aware progress. | L09; commit/pin the proven SDK provision before Manifest integration. |
| 64 / L11 | Add incremental rival connections/endpoints in existing systems without breaking incumbent journeys or accepted return rights. | L10; prove incremental registration/publication and recovery before consuming it. |
| 65 / L12 | Deterministic generated frontier/topology publication during live play. | L11, L06; prove static exploitation first and atomic expansion/recovery without moving established systems. |
| 66 / L13 | Physical settlement/terraforming scenario in the generated frontier. | L12; select the environmental/settlement contract, without assuming terraforming always precedes habitation. |
| 67 / L14 | Broader government/media influence and political modelling. | Proven regional/corporate leverage in L03–L06; select a bounded next mechanic before implementation. |
| 68 / L15 | Graphical client and station cultural/corporate branding/customisation. | Stable shared N02/A05 interfaces and proven trading/recovery; station policy/ownership views use L04–L05 when needed. |

L05 does not require buying the region in L03: existing eligibility permits construction
for eligible AI/unclaimed regions. L07 is deliberately scheduled after the persistent
release here; its canonical re-entry threshold remains W14. Presentation may progress
independently once its own prerequisites are met and must not block the native loop.

## Maintenance that travels with the tasks

- **T2/T3:** keep the decision ledger canonical and reduce duplicated prose when the
  affected plan is touched; record implementation status only with named evidence.
- **T4:** deliver authored tuning through C04 and the Stage 2 validation pipeline.
- **T6:** apply the scope funnel to every new requirement; deferred pillars do not
  displace H01–W14.
- **T8:** add cheap documentation checks with M02, reuse M04's one parity check, and
  add closure/header/registration checks with the relevant H02–H05, A01 and W01
  engine boundaries. Extend these checks as later targets and content land.

No additional baseline gate is scheduled solely for this ordering document. The
implementer records evidence for changed code at its slice/stage boundary, following
the repository's required gates and avoiding repeated unchanged runs.
