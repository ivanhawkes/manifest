# Decision ledger

Append-only index of selected project decisions. One row per decision: a short title, the single
canonical statement, current status, owner and delivery stage. This document exists so a decision
is written once. Long-form plans own the reasoning and detail and are linked from here; they do not
restate the same rule in prose, and this ledger does not restate numeric values.

Statuses: `implemented` (code plus tests, with the verifying suite named), `selected` (user
decision awaiting implementation), `proposed` (planning proposal, not accepted), `open` (unresolved
question), `superseded` (replaced, with the superseding row named). Owners: **Manifest**,
**Jettison** or **both**. Stages refer to the
[five delivery stages](development-stages.md); `boundary` is the
[engine/game boundary assessment](engine-game-boundary-assessment.md) work, `later` is beyond the
current sequence.

New rows are appended with the next free ID; IDs are never reused or renumbered. When a decision
changes, add the new row, mark the old one `superseded` and link both ways.

## Model and engine baseline

| ID | Decision | Canonical | Status | Owner / stage |
| --- | --- | --- | --- | --- |
| D-001 | Market prices use an exact decimal representation; quantities, physics and effect stats keep floats/doubles and credit ledgers stay integers | [Decimal market prices](market-prices.md) | implemented (`types`, `market`, `sim` suites; replay hashes pinned) | Manifest, roadmap item 2 |
| D-002 | Determinism contract: `-ffp-contract=off`, the engine PCG32 generator, a specified Fisher-Yates shuffle, ordered maps and stable IDs, replay and state-hash tests across Debug, Release and sanitised builds | [Architecture](architecture.md#current-module-architecture) | implemented (`sim` suite) | Manifest, complete |
| D-003 | Jettison stays genre-neutral with sample gameplay isolated in test/sample targets; Manifest owns production rules and content; reusable engine gaps are provisioned in Jettison and consumed through a committed pin | [Boundary assessment](engine-game-boundary-assessment.md#ownership-rules-for-future-changes) | selected; Jettison's extraction implemented (H02–H05, A01, W01; dependency checks at configure), pin `fafa758` | both, boundary |
| D-004 | Authoritative rules live in linked native game modules; capability-limited Wasm supplies bounded policy inputs and observation only; there is no native C++ mod ABI | [Host and script boundary](host-script-boundary.md) | selected | Manifest, stages 1–3 |
| D-005 | A CPU-only consumer that configures and runs without presentation dependencies is a separation acceptance criterion | [Boundary assessment](engine-game-boundary-assessment.md#amendments-to-the-five-stage-plan) | implemented in Jettison (H03 `clang-cpu` preset, H04 `source/test/consumer`), pin `fafa758` | Jettison, boundary / stage 1 |
| D-006 | The pinned SDK revision, `devenv.nix`, `devenv.yaml` and `devenv.lock` stay identical to the selected engine checkout; shared dependency targets are reused, never refetched | `AGENTS.md`; [Build and gates](build-and-gates.md) | selected; met at pin `fafa758` and checked in CI ([D-054](#d-054), M04) | both, boundary |
| D-007 | Gate evidence is staged and named: clang-tidy runs in CI and stays opt-in locally, a recorded gate lists the stages and presets that ran, and a partial run is never reported as a gate | [Build and gates: verification evidence](build-and-gates.md#verification-evidence) | selected; local T1/T9 gate verified (`types`, `market`, `sim` and remaining suites in all presets); T10 implemented ([D-052](#d-052), [D-053](#d-053)) | Manifest, boundary |

## Product direction

| ID | Decision | Canonical | Status | Owner / stage |
| --- | --- | --- | --- | --- |
| D-010 | There is no win condition: players accumulate corporate wealth, power and control over trade, economies, governments and media; server lifespan and pacing are undecided | [Scope](scope.md) | selected | Manifest, later |
| D-011 | A CLI-launched dedicated server advances one authoritative world with zero connected players; economy, NPC AI, autopilot and enabled auto-trading run without clients; downtime pauses the world initially | [Roadmap: networked idle game](roadmap.md#networked-idle-game-and-remote-clients) | selected | both, after minimal authority |
| D-012 | The existing TUI becomes a required remote gameplay client with `connect`/`disconnect`; Manifest's GUI shares the client command/query layer; administrative permission stays separate from gameplay | [Scope](scope.md) | selected | Manifest, after minimal authority |
| D-013 | Manual TUI spaceport trading is a stretch goal; configuring and observing physical auto-trading is required | [Roadmap: networked idle game](roadmap.md#networked-idle-game-and-remote-clients) | selected | Manifest, stage 4 |
| D-014 | Multiple governed economic regions can exist on one physical planet, each with its own population, household pool, accounts, condition and projects; a player starts with one region's development and public budget alongside a separate trading company; AI governs the others | [Regional plan: selected scope](regional-economy-and-governance-plan.md#selected-scope) | selected | Manifest, stage 2 onward |
| D-015 | Player-governed regions are protected from AI acquisition; eligible AI neighbours on the same planet stay purchasable without a sibling governor's veto; purchase transfers public decision rights only, leaving treasury funds public and private owners intact | [Regional plan: control boundaries](regional-economy-and-governance-plan.md#control-construction-and-discovery-boundaries) | selected | Manifest, later |
| D-016 | A corporation may build an expensive orbital spaceport for its own, an eligible AI or an eligible unclaimed region, with one initial construction opportunity per eligible region; neighbouring ports do not block it and other players' governed regions are protected | [Planetary plan: company-built stations](planetary-development-plan.md#company-built-stations-and-control-of-shipping) | selected | Manifest, later |
| D-017 | Opening a new system requires a funded, automated, unmanned FTL stargate delivery whose duration is distance-based over server-running wall time; the operational gate's owner sets access and fees | [Stargate plan: confirmed direction](stargate-expansion-plan.md#confirmed-direction) | selected | Manifest, later |
| D-018 | Inner map worlds start at higher technology with depleted reserves; outer worlds are resource-rich and less developed; frontier expansion adds systems at the map edge without moving established ones | [Economy plan: established core and resource frontier](economy-and-development-plan.md#established-core-and-resource-frontier) | selected | Manifest, stage 2 content |

## Economy, households and condition

| ID | Decision | Canonical | Status | Owner / stage |
| --- | --- | --- | --- | --- |
| D-020 | Credits settle instantly between authorised accounts; commodities follow the full physical chain of extraction, local haul, station warehouse, ship, destination warehouse and local use | [Economy plan: credits, title and custody](economy-and-development-plan.md#credits-title-and-custody) | selected | Manifest, stage 4 |
| D-021 | Brokers trade real stock with their own funds; buy/sell spreads plus separate warehouse handling charges at both ends are the merchant income; production and extraction capacity cap output and scarcity against demand sets prices | [Economy plan: brokers and merchant income](economy-and-development-plan.md#brokers-and-merchant-income) | selected | Manifest, stage 4 |
| D-022 | Population counts people in aggregate; consumption rates are authored per 1,000 people per simulation day and scaled by population and elapsed interval, with fractional quantities retained; legacy abstract world units need an explicit conversion before any migration | [Economy plan: aggregate households](economy-and-development-plan.md#aggregate-households-wages-and-consumer-demand-selected) | selected | Manifest, stage 2 content |
| D-023 | The first household basket has three equally weighted needs: staple food satisfiable only by rice, wheat, oats or barley traded in kilograms and interchangeable by mass; drinking water; basic medical supplies. Bread stays tradable outside the basket and seeds keep their existing IDs | [Economy plan: staple grain commodities](economy-and-development-plan.md#staple-grain-commodities-selected-implementation-handoff) | selected (`ItemRice`, `ItemWheat`, `ItemOats`, `ItemBarley` and `ItemBasicMedicalSupplies` are proposed IDs only) | Manifest, stage 2 content |
| D-024 | Aggregate health and happiness are continuous 0–100 values per region, start at 100 in the first balance scenario, survive joins, acquisitions and governor changes, fall on essential shortages and recover when all essentials are met, with happiness moving faster than health | [Economy plan: population health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected) | selected | Manifest, stage 4 |
| D-025 | Shortage severity is the weighted average of required essentials' unmet fractions with authored positive weights, initially equal, with per-good fulfilment capped so surplus cannot offset a missing essential | [Economy plan: population health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected) | selected | Manifest, stage 4 |
| D-026 | Lower condition slows production work rate only: completed batches keep their declared inputs, outputs and gross wages, slower work slows wage accrual, changes apply to subsequent steps, and missing materials, funds or inactive facilities can still stop work entirely | [Economy plan: population health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected) | selected | Manifest, stage 4 |
| D-027 | Fixed-step economic ordering: production uses the previous step's condition and settles completed batches; at each due 60-second boundary regular payroll and tax precede household purchases and consumption, then condition updates for the next step; regular payments subtract prior settlement so a boundary completion cannot double-pay | [Economy plan: population health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected) | selected | Manifest, stage 4 |
| D-028 | Planet tech advancement requires resource exploitation, physically imported unavailable resources and built production facilities, not credits or knowledge prerequisites alone | [Economy plan: planetary tech](economy-and-development-plan.md#planetary-tech-and-production-facility-dependencies) | selected | Manifest, stage 4 |
| D-029 | First tech example is Bronze metallurgy in the separate governance fixture: an operational seeded local workshop plus delivered project inputs gate timed work; completion unlocks repeat production batches; interruption pauses work and completed knowledge survives facility loss | [Economy plan: first tech example](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected) | selected | Manifest, stage 2 content / stage 4 |
| D-030 | Frontier exploitation exclusivity runs on real elapsed server-running time, pausing during shutdown, starting when the first eligible frontier port becomes operational and its concession activates, disclosed before construction, and non-renewable through transfers, rebuilds or government change | [Economy plan: real-time exclusive exploitation](economy-and-development-plan.md#real-time-exclusive-exploitation) | selected | Manifest, later |

## Trading, contracts, ships and information

| ID | Decision | Canonical | Status | Owner / stage |
| --- | --- | --- | --- | --- |
| D-040 | Public contracts require competitive corporate bids including the governor's own company; a simple government corruption trait and abstract bribery may influence eligible bid rankings only after clean tendering and fulfilment work | [Planetary plan: public procurement](planetary-development-plan.md#public-procurement-corruption-and-abstract-bribery) | selected | Manifest, later |
| D-041 | Suppliers are paid for accepted partial deliveries as they arrive; station owners honour accepted bookings, with changed fees, priorities or exclusions applying to future requests only | [Contract plan: delivery, payment and project work](procurement-and-station-contracts-plan.md#delivery-payment-and-project-work) | selected | Manifest, later |
| <a id="d-042"></a>D-042 | Tender awards require a supplier-funded performance security held until close, forfeited proportionally for undelivered goods | [Contract plan: performance security](procurement-and-station-contracts-plan.md#performance-security-selected) | selected (numeric defaults proposed, not accepted) | Manifest, later |
| D-043 | Live station market data is available only to a fleet member docked there; elsewhere a trader has dated remembered reports; every station market in directly connected systems gets a spawn-time snapshot. The same boundary applies to UI queries, native auto-trader decisions and script facts | [Market information plan: selected information rules](market-information-plan.md#selected-information-rules) | selected | Manifest, stage 4 |
| D-044 | A compact single-occupant ship with a tiny long-journey cabin and four IBC units of cargo, plus a matching small berth, is distinct from the lone-trader starter's 20-IBC hold; the smaller-berth capability is a demonstrated Jettison gap: Jettison's pad classes are a closed engine enumeration (`dock/Definitions.h`), so the provision is pad classes and hull fit as definition data, letting Manifest author its compact berth with no engine edit per hull class (Jettison roadmap W01) | [Small-ship plan: selected ship roles](small-trader-and-docking-plan.md#selected-ship-roles-and-cargo-capacities) | selected; Jettison provision implemented (W01, `Dock::PadClassDefinition`, `dock` suite), pin `fafa758`; Manifest's berth pending | both, stages 2–4 |
| D-045 | The lone-trader starter is provisioned once with operating oxygen and one fresh installed scrubber cartridge in dedicated life-support space outside the trade hold; oxygen and scrubber capacity are separate constraints, consumed per occupant per simulation day, and trade goods and loose spares use cargo volume | [Small-ship plan: selected ship roles](small-trader-and-docking-plan.md#selected-ship-roles-and-cargo-capacities) | selected | Manifest, stages 2–4 |
| D-046 | A docked ship on working station life support pauses onboard oxygen and scrubber consumption while keeping its actual remaining supplies; departure or service loss resumes onboard use, and refills and cartridge replacements are separate operations that docking cannot reset | [Regional plan: control boundaries](regional-economy-and-governance-plan.md#control-construction-and-discovery-boundaries) | selected | Manifest, stage 4 |
| D-047 | Starting regions are allocated only with an operational starter-compatible public station, a physical regional delivery connection and an available working station life-support connection, and new players spawn connected | [Regional plan: selected scope](regional-economy-and-governance-plan.md#selected-scope) | selected | Manifest, stage 2 content |
| D-048 | Basic docking, trading and regional delivery access stay available despite corporate restrictions, retaining charges, finite capacity, goods and funds and private-port policies | [Regional plan: selected scope](regional-economy-and-governance-plan.md#selected-scope) | selected | Manifest, later |
| D-049 | LPG joins Fuels as a distinct kilogram-traded commodity with storage, production and consumption uses still open, and no ship-fuel role selected | [Stage 2 content plan](stage-2-content-plan.md) | selected | Manifest, stage 2 content |
| <a id="d-050"></a>D-050 | Modular oxygen banks refine D-045: independently refillable/replaceable tanks preserve continuous supply from remaining usable tanks, with proportionally greater storage on larger hulls | [Small-ship plan: modular oxygen storage](small-trader-and-docking-plan.md#modular-oxygen-storage) | selected; deferred, not implemented | Manifest, later; re-entry after stages 1–4 acceptance |

## Workflow and verification decisions

| ID | Decision | Canonical | Status | Owner / stage |
| --- | --- | --- | --- | --- |
| <a id="d-051"></a>D-051 | Build concurrency is machine-specific: the laptop's compilation limit does not constrain Lythir; existing environment overrides select concurrency for the available host | [Build and gates: machine-specific concurrency](build-and-gates.md#machine-specific-concurrency) | selected; uses existing environment overrides | Manifest, development workflow |
| <a id="d-052"></a>D-052 | The full gate completes requested build/test presets before strict clang-tidy; tidy remains fatal and no warning baseline is introduced | [Build and gates: T10 policy](build-and-gates.md#t10-selected-gate-and-promotion-policy) | implemented in `scripts/ci.sh` (M01, 2026-11-18) | Manifest, development workflow |
| <a id="d-053"></a>D-053 | `main` receives only exact-commit, verified fast-forward promotions from `feature-base`; promotion does not repeat the full gate, and distinct integration/PR commits retain their own verification | [Build and gates: T10 policy](build-and-gates.md#t10-selected-gate-and-promotion-policy) | implemented in `.github/workflows/ci.yml` and `scripts/promote.sh` (M02, 2026-11-18) | Manifest, development workflow |
| <a id="d-054"></a>D-054 | Keep desktop-only tools out of the shared SDK shell (`lazygit` removed from both repositories, 2026-11-18, superseding its retention), then update Manifest's pin and identical environment files together and enforce parity against the immutable pin in CI | [Build and gates: T5 plan](build-and-gates.md#t5-selected-environment-parity-plan) | implemented in `scripts/check-env-parity.sh` and CI, pin `fafa758` (M04, 2026-11-18); preserves D-006 | both, development workflow / boundary |
| <a id="d-055"></a>D-055 | Tracked editor examples use explicit Debug presets, the simulation suite and development TUI at their actual paths, development-shell tooling and host-appropriate concurrency; working debugger launches require direct verification | [Build and gates: T7 plan](build-and-gates.md#t7-selected-editor-example-plan) | selected; not implemented (T7) | Manifest, development workflow |
| <a id="d-056"></a>D-056 | Implementers may write stubs at their discretion when useful; unfinished behavior remains explicit and does not satisfy dependent-task acceptance | [Agent workflow: implementer stubs](agent-workflow.md#implementer-discretion-to-use-stubs) | selected; implementation permission | both, development workflow / all stages |
| <a id="d-057"></a>D-057 | Jettison provides low-level networking common to a wide range of networked games; Manifest owns its dedicated server and every game-specific mechanic, identity mapping, authorisation and visibility rule built on it | [Roadmap: networked idle game](roadmap.md#networked-idle-game-and-remote-clients) | selected (2026-11-18); N01 maps the exact split | both, before Stage 4 (N01–N03) |

## Numeric values

Numeric balance inputs are listed by name and canonical location only. They are
selected-for-implementation values under validation, and task T4 in the
[project analysis](project-analysis-2026-11-16.md) moves them into authored YAML, after which this
index points at the data keys instead of prose.

| Value | Canonical location |
| --- | --- |
| Household consumption rates per 1,000 people per simulation day, and the interval formula | [Economy plan: aggregate households](economy-and-development-plan.md#aggregate-households-wages-and-consumer-demand-selected) |
| Health and happiness decline and recovery rates, cadence and the production work-rate multiplier | [Economy plan: population health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected) |
| Bronze project inputs, batch ratio and wage basis | [Economy plan: first tech example](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected) |
| Frontier concession term and its start, disclosure and renewal rules | [Economy plan: real-time exclusive exploitation](economy-and-development-plan.md#real-time-exclusive-exploitation) |
| Performance-security rate, forfeiture and mobilisation funds | [Contract plan: performance security](procurement-and-station-contracts-plan.md#performance-security-selected) |
| Starter ship cargo capacities, life-support provisions and endurance | [Small-ship plan](small-trader-and-docking-plan.md) |
| Starter oxygen bank layout, per-tank capacity, aggregate capacity and initial fill | [D-050 canonical specification](small-trader-and-docking-plan.md#modular-oxygen-storage), pending T4 authoring |
| Item base prices, densities, unit types, pack factors | `data/trade-items/*.yaml`, described in [model reference](model.md#units-prices-and-storage) |
| Marketplace fees, insurance, order lifetimes, price bounds | `MarketplaceOptions` in `source/game/market/include/manifest/market/Marketplace.h` |

## Open questions carried by the plans

The plans record unresolved choices explicitly. They are not decisions and must not be treated as
implemented. The densest concentrations are the economy plan's remaining commodity authoring and
population sizes, the stage 2 content plan's life-support and exploration content follow-up, the
planetary plan's transport, fee, capacity and control-change rules, the contract plan's numeric
settlement defaults, and the small-ship plan's compartment dimensions, costs, endurance and berth
fit. The dedicated server's transport, hosting and measured capacity remain open in the
[roadmap](roadmap.md#networked-idle-game-and-remote-clients).

## Rules for maintaining this ledger

1. A decision is stated once, in its canonical section; this file names it and links. Plans,
   the roadmap, `scope.md`, `architecture.md` and `session-handoff.md` link by ID instead of
   restating the rule.
2. Numeric values are never duplicated. They live in the canonical section, and move to authored
   YAML under task T4, after which the index names the data keys.
3. Selecting, changing or superseding a decision is a planning edit that appends or updates a row
   in the same change that records the reasoning; the session handoff narrates the decision and
   links the ID.
4. `implemented` requires a named verifying suite. Documentation-only completion is never
   recorded as implemented, and a planning document does not mark a stage complete on configure
   success.
5. Rows carry owner and stage, so a planning document's scope is checkable: a mechanic with no row,
   no stage and no named SDK API/gap triplet is not scheduled.
