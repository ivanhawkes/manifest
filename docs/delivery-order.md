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

## Status

Stages 1 and 2 are done: each finished row is marked **Done** with its date and a link to what it
built, and how each task went is in [history](history.md). A07 is done (2026-11-19): **Stage 3 is closed**, its checks each held by a gated test
([delivered](stage-3-authority-plan.md#delivered-a07-2026-11-19)). Its cross-review's findings are
fixed on pin `bc4facb` ([history](history.md#stage-3-cross-review--2026-11-19)). N01 is done
(2026-11-20): the [network contract](gameplay-and-server-plan.md#network-contract-n01-2026-11-20)
selects ENet over UDP, bearer tokens and plaintext until encryption, and names the Jettison gaps
N02 provisions. N02 is done (2026-11-20): the [dedicated server](history.md#n02-the-dedicated-server-and-remote-tui--2026-11-20)
steps the world unattended, admits remote requests and sends filtered snapshots on Jettison's `net`
(pin `65846bb`), and the TUI's `connect` and `disconnect` play on it. N03 is done (2026-11-20):
[several clients, reconnects and bounds](history.md#n03-several-clients-reconnects-and-bounds--2026-11-20)
on the same pin, with no engine edit. W02 is done (2026-11-20): whole-IBC orders, a broker at each
venue, order life and matching bounds, a 1/60 s step and one travel command
([D-083](decision-ledger.md#d-083) to [D-086](decision-ledger.md#d-086)), with
[the step and its transaction boundaries](stage-4-trading-plan.md#the-step-in-order); it names
one Jettison gap for W05, an order between `dock` and `lanes`. W03 is done (2026-11-20): the
[hosted book](stage-4-trading-plan.md#delivered-w03-2026-11-20) settles orders on owner and venue
accounts with each venue's broker, on the same pin with no engine edit, and goods are counted in
totes, an eighth of a cubic metre ([D-087](decision-ledger.md#d-087)). W05 is done (2026-11-20): a
ship [travels](stage-4-trading-plan.md#delivered-w05-2026-11-20) from one venue's pad to another's
through Jettison's departure, cruise and docking, on star systems and drives authored as the engine's
records ([D-088](decision-ledger.md#d-088)), on pin `377bd8d`, with two engine provisions. The user
selected new onboarding during it ([D-089](decision-ledger.md#d-089)), and after it a cruise limited
by acceleration ([D-090](decision-ledger.md#d-090)). W17 is done (2026-11-20): the Manifest mode
[cruises by acceleration](stage-4-trading-plan.md#delivered-w17-2026-11-20), manned or unmanned by a
hull's occupants, Astralis on its drives' power law, on pin `54fb871` with Jettison's cruise profile.
W16 is done (2026-11-20): a new player
[starts at the scenario's starter venue](stage-4-trading-plan.md#delivered-w16-2026-11-20), a neutral
public spaceport, with no home region or polity ([D-089](decision-ledger.md#d-089)), on the same pin
with no engine edit. W15 is done (2026-11-20): the
[vertical slice](stage-4-trading-plan.md#delivered-w15-2026-11-20) is played through the console,
furs bought at Ore, flown to Brine in 4.49 days and sold there, with the console's trade, travel and
timed step and a player's journeys and orders in the snapshot, on the same pin with no engine edit.
W04 is done (2026-11-20): the [regional economy](stage-4-trading-plan.md#delivered-w04-2026-11-20),
each region's employer making its goods in batches whose wages it reserves and pays, selling them to
its broker, and its households buying their essentials and then their optional goods through the
book, their shortages moving health and happiness and so the work rate, on the same pin with no
engine edit; every region trades the essentials ([D-092](decision-ledger.md#d-092)). W06 is done
(2026-11-20): [station life support](stage-4-trading-plan.md#delivered-w06-2026-11-20), a ship
connected as it docks while its venue has a connection free and drawing its own oxygen and cartridges
off it, refilled and replaced from its own hold ([D-094](decision-ledger.md#d-094)), a new player
spawning connected, on the same pin with no engine edit. W07 is done (2026-11-20):
[live and stale observations](stage-4-trading-plan.md#delivered-w07-2026-11-20), each player's reports
of the venues their fleet is docked at refreshed as each step ends and frozen as the last ship leaves,
with each market's sales over the hour, and starting reports of the rest of their system and the
systems directly connected to it ([D-095](decision-ledger.md#d-095)), on the same pin with no engine
edit. W08 is done (2026-11-20): [company-funded cargo upgrades](stage-4-trading-plan.md#delivered-w08-2026-11-20),
a tier of cargo holds bought with the company's own credits for its docked ship, once, its hold
computed from its hull and its systems, and a hull's authored mass ([D-096](decision-ledger.md#d-096)),
on the same pin with no engine edit. L26 is done (2026-11-20):
[the console's long step](stage-4-trading-plan.md#delivered-l26-2026-11-20) is taken a slice at a
time and stoppable, and a rails body placed once a step in Jettison halved a cruising step, on pin
`9e0274f`. W09 is done (2026-11-20): [the slice's whole fixture](stage-4-trading-plan.md#delivered-w09-2026-11-20),
one command stream from purchase to upgrade with production, life support and observations, its credits
and goods accounted for every step, saved in flight, replayed, and the same from the pack, on the same
pin with no engine edit. L27 is done (2026-11-20): [a fast-forward over quiet steps](stage-4-trading-plan.md#delivered-l27-2026-11-20),
each system saying when it next has to run and the console and server passing over the steps
between to the same bits, Ore to Brine 2.37 s, on pin `892b7b3`. Next: W10.

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
| 12 / H07 | Close Stage 1: commit/publish the validated engine, land the compatible Manifest adapter/import requirements with its pin/environment update, verify fetched consumption and the complete acceptance matrix. **Done 2026-11-18, pin `e53681e`.** | H06 |

Do not require the future host target in H01 before its SDK exists. H06 may use the
explicit local override; H07 must prove the committed fetch. Add the Stage 1 dependency
closure/registration checks from T8 alongside the host and sample extraction.

## Priority 2: Stage 2 validated content

Canonical scope: [Stage 2](stage-2-content-plan.md). Recheck its older API inventory
against the delivered Stage 1 SDK before implementation.

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 13 / C01 | Finalise schema mappings, defaults/null handling, ordered IDs, units, scenario frames and distinct planet/region/government/company relationships. Inventory remaining authoring decisions. **Done 2026-11-18**, [schema contract](stage-2-content-plan.md#schema-contract-c01-2026-11-18). | H07 |
| 14 / C02 | Add reflected engine-readable definitions, generated record packaging and equivalence checks; define the canonical manifest/fingerprint builder and preserve exact price text and IDs/selections. **Done 2026-11-18**, [session handoff](history.md#c02-definitions-through-the-engine-registry--2026-11-18). | C01 |
| 15 / C03 | Add atomic aggregate loading, shared semantic validation, engine layering/migration wiring and failure rollback; compute content identity before publication, migrate callers and retire custom readers after their last caller moves. **Done 2026-11-18**, [session handoff](history.md#c03-one-validated-content-set--2026-11-18). | C02 |
| 16 / C04 | T4 and item follow-ups: resolve required physical/storage fields and author tuning, grains, medical supplies, LPG, Life Support and hull definitions through the shared pipeline. Keep unselected values explicit and obtain decisions where required. **Done 2026-11-19**, [item authoring](stage-2-content-plan.md#item-authoring-c04-2026-11-19), [tuning](stage-2-content-plan.md#tuning-t4-c04-2026-11-19). | C03 |
| 17 / C05 | Author the separate trading and multi-region governance fixtures, including valid starting access/service capacity and required project/production references. **Done 2026-11-19**, [scenarios](stage-2-content-plan.md#scenario-and-frames). | C04 |
| 17b / C09 | Share Jettison's ship and station definitions ([plan](small-trader-and-docking-plan.md#shared-ship-and-station-definitions-c09-done-2026-11-19)): pad classes, a dock hull for each hull with the starter a skiff, venues naming station layouts, and the engine's fit rule without a world. **Done 2026-11-19**, [plan](small-trader-and-docking-plan.md#shared-ship-and-station-definitions-c09-done-2026-11-19). | C05 |
| 18 / C06 | Add the validator CLI through the same loader; verify host agreement, explicit overlays, contextual provenance and failure behavior. **Done 2026-11-19**, [options](stage-2-content-plan.md#versions-layering-and-migration). | C03, C09 |
| 19 / C07 | Verify directory/pack content-manifest identity and add the validation build graph: validator → staged directory validation → pack → pack validation. **Done 2026-11-19**, [identity](stage-2-content-plan.md#content-identity). | C06 |
| 20 / C08 | Close Stage 2: verify directory/pack identity, layering/migration, late-failure rollback and semantic rejection; run final gates and record the delivered schema/content contract. **Done 2026-11-19**, [delivered](stage-2-content-plan.md#delivered-c08-2026-11-19). | C07 |

No task may infer physical cargo volume from a commodity's trading unit. Resolve
required authoring choices before their definitions/fixtures are accepted. D-050's
modular tank detail remains deferred; it does not block C04. Compact-berth runtime
integration is W01/W05, not a requirement to complete the neutral host.

## Priority 3: Stage 3 authority

Canonical scope: [Stage 3](stage-3-authority-plan.md).

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 21 / A01 | Jettison boundary prerequisite: resolve neutral frame/partition and save-metadata seams needed for hosted state, with compatible fixtures; commit and consume any SDK provision. **Done in Jettison 2026-11-18, in pin `fafa758`**; C01 rechecks the fit (see status). | C08 |
| 22 / A02 | First refresh the [Stage 3 plan](stage-3-authority-plan.md)'s 2026-11-15 inventory against the Stage 1–2 code, and obtain the user's choice of how a player takes a `playerHome` region at allocation ([open](stage-2-content-plan.md#authoring-decisions-still-open)). Add reflected one-world state, distinct accounts/control identities, counters and serialisation; settle the registry-adapter and capacity-conversion owners, then prove index rebuilding. Commands and queries live in a game module, not `tools/tui`, so A06 moves the console onto them rather than rewriting them. Add the TUI-off build to the gate. **Done 2026-11-19**, [history](history.md#a02-one-hosted-world--2026-11-19). | A01 |
| 23 / A03 | Add trusted admission, fixed-step ordering, bounded retry/history state and structured command results. **Done 2026-11-19**, [history](history.md#a03-admission-and-the-request-ledger--2026-11-19). | A02 |
| 24 / A04 | Implement retry-safe provisioning and atomic cargo load/unload using validated docked bootstrap state, real account ownership and capacity checks. **Done 2026-11-19**, [history](history.md#a04-provisioning-and-cargo--2026-11-19). | A03 |
| 25 / A05 | Add immutable actor-filtered queries and result snapshots; define the observation boundary before exposing market views. **Done 2026-11-19**, [history](history.md#a05-queries-and-the-observation-boundary--2026-11-19). | A04 |
| 26 / A06 | Separate command/query services and tests from FTXUI; preserve the development console without a second hosted simulation. **Done 2026-11-19**, [history](history.md#a06-the-console-apart-from-ftxui--2026-11-19). | A05 |
| 27 / A07 | Close Stage 3: prove authorisation, conservation, rejection atomicity, sequencing/retries and in-memory save/replay with TUI disabled; hand over full order/travel/upgrade contracts. **Done 2026-11-19**, [delivered](stage-3-authority-plan.md#delivered-a07-2026-11-19). | A06 |

Provisioning may use a validated seeded dock/service state at this stage. It must not
offer arbitrary player teleport/dock commands or claim the later movement/service
runtime is implemented. Do not expose unfinished order/travel commands as successful.

## Priority 4: remote connection foundation

This inserts the required [network/TUI slice](roadmap.md#networked-idle-game-and-remote-clients)
after minimal authority. It does not wait for Stage 5 or promise durable responses.

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 28 / N01 | Select the bounded transport/authentication/protocol contract and map missing reusable SDK facilities; keep principal identity, game authorisation and admin rights distinct. **Done 2026-11-20**, [network contract](gameplay-and-server-plan.md#network-contract-n01-2026-11-20). | A07 |
| 29 / N02 | Provision demonstrated SDK gaps, then add the headless host loop and real TUI connect/disconnect through the command/query boundary. **Done 2026-11-20**, [history](history.md#n02-the-dedicated-server-and-remote-tui--2026-11-20). | N01 |
| 30 / N03 | Prove initial multi-client/reconnect behavior, filtered snapshots, bounded queues and world progress with zero clients. Only advertise operations already implemented. **Done 2026-11-20**, [history](history.md#n03-several-clients-reconnects-and-bounds--2026-11-20). | N02 |

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
| 32 / W02 | Resolve outstanding settlement, transport, service and cadence choices; document the actual scheduler graph and transaction boundaries before integration. **Done 2026-11-20**, [the step](stage-4-trading-plan.md#the-step-in-order) and [D-083](decision-ledger.md#d-083) to [D-086](decision-ledger.md#d-086). | W01, C08, A07 |
| 33 / W03 | Adapt marketplace reservations, matching, partial settlement, cancellation, expiry and fees to owner/venue accounts with checked atomic writes. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w03-2026-11-20), goods in totes ([D-087](decision-ledger.md#d-087)). | W02 |
| 34 / W05 | Integrate real departure, cruise, arrival, docking and cargo eligibility; prove starter/compact berth and capacity behavior where selected. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w05-2026-11-20), the star systems and drives ([D-088](decision-ledger.md#d-088)). | W01, W03, A04 |
| 34a / W17 | Cruise by acceleration ([D-090](decision-ledger.md#d-090)): Jettison provisions a `CruiseDrive` acceleration profile (a boost for a time split across both ends, a sustained rate between) with its time and placement, a drive without one cruising as now; the game mode chooses its cruise model, the Manifest mode authoring a manned and an unmanned profile, given by a hull's occupants, and Astralis keeping its time-compressed drives. Prove local trips at 2 g, a long one switching to 1 g after two hours, an unmanned one at 12 g, Astralis's cruise unchanged, and a save mid-cruise. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w17-2026-11-20), on pin `54fb871`. | W05 |
| 34b / W16 | Onboarding by [D-089](decision-ledger.md#d-089): provisioning gives a company, the starting credits and the starter ship parked at a neutral public spaceport the scenario names, with no home region or polity; retire `playerHome` allocation, `NoHomeRegion` and the home-venue rule, and move W13's home regions to founding by acquisition. Founding a homeworld is its own later task under the acquisition rules. **Done 2026-11-20**, [history](history.md#w16-onboarding-at-a-neutral-spaceport--2026-11-20). | W05 |
| 35 / W15 | Vertical slice: in the trading fixture, one company ship buys at one venue, departs, cruises, docks and sells at the other through the public commands, on W03's settlement and W05's travel; no production, life support, observations or upgrades yet. Played through the console, it shows the loop end to end before W04 and W06–W08 build around it; W09 later proves the complete fixture. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w15-2026-11-20). | W03, W05 |
| 36 / W04 | Integrate local production, finite brokers/demand, regional payroll/tax, essential purchases and health/happiness in the selected fixed-step order; eliminate automatic inter-venue stock transfers. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w04-2026-11-20), every region trading the essentials ([D-092](decision-ledger.md#d-092)). | W03 |
| 37 / W06 | Implement working station life-support connections, onboard usage and explicit resupply under the resolved physical/accounting rules; preserve one-time starter grants. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w06-2026-11-20), on the user's choices ([D-094](decision-ledger.md#d-094)). | W04, W05 |
| 38 / W07 | Implement live/stale observations, spawn reports for every directly connected station market, report age and actor filtering across queries and trader/script facts. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w07-2026-11-20); trader and script facts read the same reports when W10 adds them. | W03, W05, A05 |
| 39 / W08 | Implement company-funded cargo upgrades without sharing regional/civilisation funds; prove retry-safe payment/capacity changes. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w08-2026-11-20), on the user's choices ([D-096](decision-ledger.md#d-096)). | W03, A04 |
| 40 / W09 | Prove the complete public-command buy → load → travel → dock → unload → sell → upgrade fixture, including directory/pack and cross-build replay. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-w09-2026-11-20). | W04, W05, W06, W07, W08 |
| 41 / W10 | Add enabled standing trade policies, autopilot and NPC jobs through the same physical commands and permitted market facts; prove operation without connected clients. | W09, N03 |
| 41a / W18 | Transport and holding charges at their real values. W10 charges them nominally (a 10-credit docking fee as a `Travel` starts, a 2-credit berth fee each simulation hour docked, [D-097](decision-ledger.md#d-097)) and its jobs attribute them to a lot through `Policy::Charges`: select the berth fee's amount and basis (it includes connected life support, D-047), the docking fee's, arrears' consequences, and the cost of life support used in transit, attributed the same way; storage fees (L16) and tolls (L10, L16) join that attribution when they land. Prove an estimate and a floor with each charge. | W10 |
| 42 / W11 | Resolve the clean public-project/tender lifecycle's remaining rounding, deadline, default and regional delivery choices; define saved jobs, rights and reservations. | W10 |
| 43 / W12 | Implement competitive procurement, partial delivery/payment, performance security and a material-gated regional project through physical fulfilment. | W11 |
| 44 / W13 | Prove the multi-region idle governance scenario: independent accounts/condition, AI neighbours, a homeworld region per participant founded by acquisition ([D-089](decision-ledger.md#d-089)), development queues, protected late joining and reconnect continuity. | W12, W10 |
| 45 / W14 | Close Stage 4: verify trading, market fog, automation and regional development together, in-memory continuation and final repository gates; record the actual schedule and state contracts. | W09, W13 |

W01 is technically independent of N01–N03; its later placement is the default priority,
not a dependency on transport. Likewise, W05, W07 and W08 can be developed separately
once their listed prerequisites pass. W15, the vertical slice (the user's, 2026-11-19), needs
only W03 and W05, so W04 follows it. The final loop must integrate all of them.
Commercial private-port booking/policy changes follow later; the first public project
uses an accessible public destination and cannot depend on unfinished private ports.

## Priority 6: Stage 5 recovery and persistent release

Canonical scope: [Stage 5](stage-5-recovery-plan.md). Network, regional development,
observations, automation and accepted obligations already delivered must be covered.

| Order / ID | Task and completion boundary | Must complete first |
| --- | --- | --- |
| 46 / R01 | Define the final versioned Manifest save/compatibility section and exact content/state inventory; cover request outcomes, observations, regional rights, cadence and all active jobs. | W14 |
| 47 / R02 | Prove the required durable I/O, directory creation and world-lock contract; provision any missing reusable primitive in Jettison and consume its committed pin. | R01 |
| 48 / R03 | Implement ordered immutable step journals/catalogue and acknowledge durable acceptance only after the tested commit boundary, including autonomous empty steps. | R02 |
| 49 / R04 | Implement boundary snapshots, retention and startup replay into a candidate world; prove crash/fault behavior, compatibility rejection and retry preservation. | R03 |
| 50 / R05 | Implement bounded shutdown and verified backup/restore with storage/retention limits and interrupted-publication tests. | R04 |
| 51 / R06 | Select and document the supported runtime distribution contract, dependencies and writable-data locations. | R05 |
| 52 / R07 | Build and test the relocatable read-only distribution outside both checkouts/development environment; retain graphics-free consumer coverage. | R06 |
| 53 / R08 | Prove persistent multi-client reconnect/retry and zero-client recovery across trading, region control, projects, observations and automation. | R04, N03, W14 |
| 54 / R09 | Close Stage 5: consolidate fault/restart, backup, relocation, cross-build replay and final gate evidence; record measured I/O, recovery and storage limits. | R05, R07, R08 |

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
| 55 / L01 | Deployment hardening, capacity profiling and unattended soak; assess the supported persistent service before public hosting. | R09; select operational platform, security and measured release limits. |
| 56 / L02 | Simple corruption/bribery after clean tenders, with preserved eligibility, separate expenses and recovery. | W12, R09; settle remaining scoring/cost choices. |
| 57 / L03 | Eligible AI regional-control acquisition and portfolio policies. | W13, R09; define pricing/transfer rules and prove player/home-region protection. |
| 58 / L04 | Private-port bookings and owner access/priority policy; Jettison admission/queue provision before Manifest policy. | W05, R09; settle service windows/failure terms and prove grandfathered commitments. |
| 59 / L05 | Corporate port construction and infrastructure/supply leverage on a static map. | L04, W12; resolve funded site, deadline/salvage and local delivery rules before enabling construction. |
| 60 / L06 | Resource exploitation and company-controlled colonies with independent regional grants/concession expiry. | L05, W13; resolve extraction, operating and colonial-government rules. |
| 61 / L07 | Modular oxygen banks and larger-hull scaling under D-050. | W14, R09; D-050's re-entry threshold is stages 1–4, with tank/storage/refill rules resolved before implementation. |
| 62 / L08 | Prove static multi-system navigation and existing-journey recovery before live expansion. | W05, R09. |
| 63 / L09 | Resolve expedition funding/equipment, commissioning, protected return and operating-supply rules; provision reusable ungated arrival/topology gaps. | L08; use the [stargate capability map](stargate-expansion-plan.md#capability-map-and-implementer-evidence). |
| 64 / L10 | Deliver a fixed gate pair to a pre-existing unconnected destination; validate once-only activation, ownership, permits, outages and shutdown-aware progress. | L09; commit/pin the proven SDK provision before Manifest integration. |
| 65 / L11 | Add incremental rival connections/endpoints in existing systems without breaking incumbent journeys or accepted return rights. | L10; prove incremental registration/publication and recovery before consuming it. |
| 66 / L12 | Deterministic generated frontier/topology publication during live play. | L11, L06; prove static exploitation first and atomic expansion/recovery without moving established systems. |
| 67 / L13 | Physical settlement/terraforming scenario in the generated frontier. | L12; select the environmental/settlement contract, without assuming terraforming always precedes habitation. |
| 68 / L14 | Broader government/media influence and political modelling. | Proven regional/corporate leverage in L03–L06; select a bounded next mechanic before implementation. |
| 69 / L15 | Graphical client and station cultural/corporate branding/customisation. | Stable shared N02/A05 interfaces and proven trading/recovery; station policy/ownership views use L04–L05 when needed. |
| 70 / L16 | Moongates ([D-091](decision-ledger.md#d-091)): fee-charging gates that connect places round one planet or moon, as stargates do between systems; provision the reusable local transit in Jettison before Manifest's fees and access. | W05, W17; settle the fee, the places each connects and its transit time first. |
| 70 / L16 | Consignment storage at a market's warehouse ([plan](economy-and-development-plan.md#consignment-storage-selected-2026-11-19), [D-072](decision-ledger.md#d-072)): goods unloaded and listed under their owner's title until bought or withdrawn, within the market's and the ship's space caps, at a storage fee by the space taken. | W09; settle the plan's open fee, cap and collection choices first. |
| 71 / L18 | Freight contracts: a trader posts cargo to carry between venues and ship owners bid to carry it ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19), [D-074](decision-ledger.md#d-074)). | W09; settle posting, bidding, delivery deadlines and failed delivery first. |
| 72 / L19 | Warehouse receipts: consigned goods carry a tradable title, sold where they lie and collected later ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | L16. |
| 73 / L20 | Paid price reports: fresher market information sold over the players' stale observations ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | W07; settle who sells reports, their price and their freshness. |
| 74 / L21 | Forward contracts: a bilateral promise to deliver a quantity at a venue by a tick at a price ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | W09, L16; settle collateral and default first. |
| 75 / L22 | Loans against stored goods, at interest, the goods kept on default ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | L16; settle the lender, interest, limits and default. |
| 76 / L23 | Cargo insurance policies, with claims paid on cargo lost to piracy ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | W09 and a cargo-loss mechanic; settle premiums, cover and claims. |
| 77 / L24 | Auctions for goods left with unpaid storage fees, seized cargo and salvage ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | L16; settle the auction form and what is sold. |
| 78 / L25 | Escrow and letters of credit for players trading with each other directly ([plan](economy-and-development-plan.md#further-market-services-selected-2026-11-19)). | N03 and direct player trading; settle the terms first. |
| 80 / L26 | The console's long step ([W15](stage-4-trading-plan.md#delivered-w15-2026-11-20), the user's, 2026-11-20): `step 5d` blocks the console and the TUI until it ends, with no progress and no way to stop it, and Ore to Brine on the manned profile costs 9 min 35 s of a release build (about 25 µs a step). Step beside the frontend, showing progress and stoppable, and profile where a step's time goes; a reusable cost found in Jettison's systems is provisioned there. | Before the next task that plays a long journey through the console, at the latest W09's fixture. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-l26-2026-11-20). |
| 81 / L27 | A fast-forward over quiet steps ([L26](stage-4-trading-plan.md#delivered-l26-2026-11-20), the user's, 2026-11-20): a cruising step is 13 µs, nearly all of it Jettison's movement and under 5% Manifest's systems, so running Manifest's own less often saves little. Jettison provisions the world going straight to the next step any system must run at while nothing but motion by formula is under way, to the same bits as stepping there ([Jettison's roadmap](../../jettison/docs/roadmap.md#first-what-saves-and-the-simulations-interface-hold)); Manifest's systems then say when they next must run (a batch due, an order lapsing, a journey ending), and its console and server use it. | Decided before W10, whose traders and NPC jobs add work to every step. **Done 2026-11-20**, [delivered](stage-4-trading-plan.md#delivered-l27-2026-11-20). |
| 82 / L28 | Docked fleets and the fast-forward (the user's, 2026-11-21, booked straight after W10): `manifest.observe` refreshes every docked fleet's venues each step, so no step is passed over while any ship is docked, and W10's NPC and blocked jobs wait docked. Examine how a live report can change only when its market does (its book, its sales window's edge, a docking or a departure), with observation's quiet naming that step and its skip keeping the reports to the bits stepping gives; implement it, or record why not and what it costs. | W10 |
| 79 / L17 | Stretch goal: options and futures on a market's goods ([plan](economy-and-development-plan.md#options-and-futures-stretch-goal), [D-073](decision-ledger.md#d-073)). | L16, L21, R09; select settlement, margin and default rules before scheduling. |

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
