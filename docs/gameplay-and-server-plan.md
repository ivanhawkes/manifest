# Gameplay and persistent server plan

Recorded on 2026-11-15 at the user's request after resolving the future-planning
questions. The subsequent networked idle-game, corporate-power and planetary-development
decisions below extend that baseline. These are planning requirements, not implemented capabilities.
Implementation is assigned to Claude. This document extends the
[standalone mod plan](standalone-mod-plan.md); it does not replace its SDK integration
stages or the [host/script boundary](host-script-boundary.md).

The [planetary development and frontier plan](planetary-development-plan.md) records the
2026-11-15 additions and impact assessment. It supersedes the optional administration-only
TUI direction and the assumption that all planetary development is permanently AI-controlled.
There is no win condition: players grow trading companies and accumulate wealth, power and
control over trade, economies, governments and media. Days versus months concerns server
lifespan and pacing; it does not establish a victory timer or automatic reset.

## Product decisions

| Question | Decision | Implementation consequence |
| --- | --- | --- |
| First playable loop | Operate a trading company: buy at one venue, transport goods, sell elsewhere and reinvest. | Connect the economy to ships, docking, cargo and company progression. |
| Persistence | Worlds persist between sessions and advance while the server runs. No catch-up for server downtime initially. | Resume the saved authoritative step; player disconnects do not stop the world. |
| Player control | One trading company per player, initially with one ship, plus control of one planet's development and public budget. AI governs other inhabited planets. | Company finances, planetary budgets, civilisation affiliation and control rights are distinct. |
| Initial fixture | One server, one star system, two civilisations, two venues, eight goods and up to four connected clients for the original trading test. | Keep this tiny fixture; add a governance scenario with one home per participant and AI neighbours. It is not the complete server-world size. |
| Capacity target | Benchmark 16 companies, 16 venues and 10,000 open orders before expanding public hosting. | These are provisional validation targets, not demonstrated capacity or release promises. |
| Idle play | World systems, NPCs, enabled auto-trading and development continue with zero clients connected. | Authoritative schedules and standing policies cannot depend on presentation or connection count. |
| Remote frontends | Morph the existing TUI into a gameplay client with `connect`/`disconnect`; Manifest's GUI uses the same server interface. | Shared authenticated commands and filtered snapshots; separate admin permissions. Manual TUI trading is a stretch goal, physical auto-trading is required. |
| Corporate expansion | Purchase control of eligible AI planets or settle frontier worlds; other players' planets are protected from this acquisition mechanic. | Persist per-planet rights and budgets; initial one-planet control is not a permanent cap. |
| First development controls | Projects, investment budgets and public procurement. | Keep public project finances and company delivery/payment distinct. |
| Procurement awards | Corporations must bid, including a governor's own company; later add a simple government corruption trait and abstract bribery that influence outcomes. | Prove clean competitive tendering/fulfilment first; keep influence costs distinct from public budgets and test awards/retries/recovery. |
| First corporate leverage | Infrastructure, essential supplies and access contracts; powerful companies can build/control stations with fees, priorities and rival-access refusal. | Physical services and contractual rights create dependence; government financing and media systems follow. |
| Corporate spaceports and colonies | A corporation may build a very expensive orbital port at any planet with none, provided it is not owned/controlled by another player; exploit resources and establish colonies with company-controlled puppet governments. | Define site races, physical construction/supply, resource rights and colonial/public finances. Long-term profit comes from the resulting economy. |
| Supply payment | Pay accepted partial deliveries as they arrive. | Atomic goods/entitlement/payment receipts; proposed contract-specific rounding is separate from marketplace settlement. |
| Station commitments | Honour accepted bookings; owner fee/priority/access changes affect future requests. | Persist accepted terms and operation identity; distinguish quotes from committed visits. |
| Credits and commodities | Credits transfer instantly between authorised accounts; goods follow the full extraction/industry, haul, warehouse, ship and destination chain. | Separate title, custody, location and reservations; no money-driven teleport of goods. |
| Brokers and fees | Brokers buy/sell their own finite stock/funds, earning through spreads; warehouses charge separately at both ends. | Quote real liquidity, charge each provided service once, and include all costs in auto-trade decisions. |
| Development and geography | Tech needs exploited/imported resources and actual facilities; inner worlds start higher-tech and depleted, outer worlds richer and less developed. | Validate cross-facility/import dependencies and seeded spatial economic generation. |
| Frontier rights | Initial exclusive exploitation lasts for elapsed real server-running time, pausing in shutdown; afterwards rivals can exploit and build ports despite a puppet government. | Keep starting homes protected; amend no-second-port rules for reopened concessions and persist host time/events for replay. |

The first integration fixture runs without a renderer, using commands and automated tests.
A small real network/TUI slice should follow the minimal command/ownership foundation rather
than waiting for graphical gameplay. This sequencing amendment is proposed, not implemented.
Graphical presentation uses the same command/query interface and selected public SDK features.

See the [supply-contract and station-booking handoff](procurement-and-station-contracts-plan.md)
for the proposed first tender and commercial booking lifecycle. It separates confirmed
payment/commitment decisions from proposed scoring, arithmetic, deadlines and failure rules.
The [physical economy/development plan](economy-and-development-plan.md) records supply caps,
scarcity pricing, commodity custody/handling, imported tech inputs, map gradients and timed
concessions. Initial port eligibility is refined by reopening after exclusivity; controller
changes cannot make a frontier concession permanently private.

## Ownership and physical trading

Companies own ships, money, cargo, venue-local inventory and orders. Planetary public budgets,
population, development and institutions have separate ownership/control records. Civilisation
affiliation is not a company identity. A player may direct public investment alongside their
company without merging their wallets; procurement or other transfers need explicit rules.
Preserve existing authored civilisation IDs and define separate company/planet identities
and control references. Influence over government/media does not itself grant legal ownership
of all assets or override another player's protected control rights.

Purchasing goods transfers them into company-owned inventory at the selling venue.
Loading requires a docked ship and available capacity. Unloading transfers cargo into
inventory at the destination venue. Sell orders reserve goods already present at that
venue. A global stockpile must not permit instantaneous transport between venues.

Players and AI use the same asset ownership and native validation rules. Civilisation
production and consumption must integrate with local inventory explicitly. Decide the
concrete component/service representation during host integration; retain one authoritative
world and one settlement owner as required by the host/script boundary.

Start progression with one company purchase that increases cargo capacity or trading
capability. Civilisation progression remains separate. The initial scenario does not
require every authored skill or effect to become playable.

## Commands, identity and queries

The existing development APIs assume trusted local callers. For example, an order
request carries a civilisation ID and tick, and cancellation accepts an order ID without
a caller identity. Do not expose those signatures directly as a remote authority boundary.

Define a host command contract with:

- Authenticated actor identity resolved by the host, never trusted from a claimed owner field.
- A request identifier and persisted retry-detection semantics.
- Validated target identities and payload.
- Server-assigned execution step and deterministic ordering.
- Structured acceptance, rejection and completion results.

Authentication and permissions are host concerns; ownership, docking, affordability,
inventory, reservations and progression eligibility remain native simulation validation.
Administrative mutations use the controlled command path with explicit permissions.
Define stale-command handling and the scope/lifetime of retry identifiers before publishing
a protocol. A retried committed request must not execute twice after reconnect or restart.

Queries return structured, access-filtered snapshots and reasons. Frontends own text and
visual formatting. Clients submit intentions and never advance an authoritative economy.
Begin with full snapshots at a bounded frequency; introduce deltas only when measurements
justify them. Transport, encoding and authentication mechanism remain design choices for
the networking milestone, after the command contract works locally.

Acceptance includes another player being unable to cancel an order, predictable stale
request rejection, retry safety and replay of the exact accepted command ordering.

## Persistence, recovery and content compatibility

Distinguish command receipt from durable commitment. Persist accepted commands with their
execution step and ordering before reporting durable success. Use periodic atomic snapshots
and a command journal to recover from the last consistent state. Snapshot boundaries must
identify precisely which journal entries are already included so recovery never applies
an order or refund twice.

Target no loss of durably acknowledged commands after a process crash, assuming intact
storage. Backups cover storage loss separately. Snapshot frequency, journal format,
durability batching and retention are implementation decisions to measure and document;
they must preserve the acknowledgement contract.

Persist all authoritative state, including outstanding reservations, orders, company and
civilisation progression, inventory location, economic counters and ID allocation. Restart
must not reuse order identities. Save/load continuation and replay must match the world hash.

Freeze content for a running world. Record engine/game versions, schema versions and a
content fingerprint with saves. Upgrades require validation, a backup and a controlled
restart. Specify supported migrations and rollback limits; do not silently run an old
world against changed balance data. No live balance reload initially.

Test interrupted snapshot writes, recovery around journal/snapshot boundaries, retries
after restart, incompatible content and recovery from a backup. A corrupt new snapshot
must not destroy the previous usable snapshot.

The market's current 64-entry fill history serves presentation. Design operational audit
history separately, with explicit retention, rather than treating that buffer as a complete
record or keeping unbounded diagnostic data in authoritative state.

## Content authoring and validation

Add a command-line validator using the same VFS loaders and semantic rules as the host.
It must validate a directory or pack without launching the game and run during packaging
and CI. Report the virtual path, definition ID and responsible source/override for failures.

Cover duplicate IDs, unresolved references, progression graph errors, unsupported schemas,
invalid numeric values and semantic effect combinations, and missing required scripts or
assets. Explain effective definitions after layering so an author can identify which
package supplied an override. Reuse shared validation rather than duplicating loader rules.

Keep authored IDs and YAML keys stable. Retain the established decimal per-unit price
policy and floating quantity types; this plan does not authorise a numeric rewrite.

## Tooling and administration

Extract host-facing command/query services from terminal presentation. Today
`ManifestConsole` owns its own simulation and progression, and its tests are gated by
the TUI option. Keep it as a development harness during migration, then adapt its command
workflows to the host services as appropriate.

Command coverage must run with `MANIFEST_BUILD_TUI=OFF`. The existing TUI is to become a
required remote gameplay frontend, with a shared client layer for the GUI. Its local harness
is transitional. A scriptable administration interface remains useful, but gameplay commands
must not require administrator permissions. Disconnecting a client cannot affect server
lifetime, relinquish a planet, stop enabled jobs or create a competing simulation. Do not
expose the unrestricted development console directly as a remote protocol.

On reconnect, return current state, retry outcomes and a bounded summary of unattended trading
and development. Persist standing policies, job progress, reservations and control rights.
Separate public/company views and budgets. AI governors and player-policy executors obey the
same native spending, cargo, project and acquisition validation rules.

## Scale, stability and operations

Create small, expected and stress workloads with explicit company/civilisation, venue,
good and open-order counts. Measure simulation-step duration, command backlog, matching
and expiry costs, memory growth, save size and long-run economy behaviour. Run replay
checks over substantially longer sequences than the current hundred-tick baseline.

Marketplace operations currently gather and sort active orders repeatedly. Measure those
paths before adding indexes or caches. Introduce parallel simulation work only with a
demonstrated need and a preserved deterministic ordering contract.

Long-run invariant checks must account explicitly for production, consumption, fees and
other designed sources/sinks. Do not assert total resource conservation where gameplay
intentionally creates or consumes resources. Verify reservation accounting and rejection
atomicity independently of balance tuning.

Before external hosting, provide a true renderer-free build, graceful shutdown, readiness
reporting, structured logs, bounded command queues and backup/restore procedures. Diagnostics
identify engine/game/content versions. Test release archives outside both source checkouts
and validate save compatibility before deploying an upgrade. Hosting platform, performance
budgets and transport selection remain to be settled with implementation evidence.

## Initial acceptance scenario

1. Start a world with two venues having complementary production and demand, using the
   small eight-good content set.
2. Create a company with one ship and a bounded starting balance.
3. Buy goods, load them, travel, dock, unload and sell; verify ownership, local inventory,
   cargo capacity, reservations and settlement at each transition.
4. Purchase one company upgrade using the proceeds.
5. Disconnect and reconnect without changing ownership or duplicating requests.
6. Restart the server and recover the same committed state without downtime catch-up.
7. Replay recorded commands and obtain the same state hash.

Implement steps 1–4 and local save/replay evidence first. Add actual connection/reconnection
coverage with the networking milestone. Directory and pack content must produce equivalent
results. A content-mount test alone does not satisfy the scenario.

## Sequencing and deferred scope

The [next development stages](development-stages.md) retain five foundational work packages.
Their amendments and the [planetary plan](planetary-development-plan.md) add required remote
idle play, governed development and later corporate/frontier growth. Retain the ownership and
persistence contracts here when refining the extended schedule.

| Milestone | Deliverable |
| --- | --- |
| Next | Public Jettison host, validated content and authoritative commands/ownership, accounting for separate planet and company control. |
| Then | Small network/TUI slice, physical idle trading and governed development; complete recovery before durable production operation. |
| Alongside integration | Content validator and frontend-independent command tests. |
| Network slice | Renderer-free server, identity/permissions, retry semantics and filtered snapshots; no durability claims before recovery evidence. |
| Before public hosting | Measured capacity, operational diagnostics, compatible release packages and restore testing. |

Interstellar expansion/settlement and government/media influence are roadmap goals after the
first idle trading/development loop. Whole-civilisation sovereignty, combat, piracy, insurance
claims and server-downtime catch-up remain deferred. Existing definitions may remain without
making every associated mechanic a release requirement. This deferral does not remove
existing fee/insurance calculations from the model or authorise rebalancing them silently.

## Handoff and validation status

Baseline: Manifest `feature-scaffold` at `9f17d34`; the selected Jettison SDK is
`f85092027eda49c29001f7589608df1f0b8827fb`. Recheck working trees before implementation.
The review inspected documentation, public gameplay interfaces, console ownership, tests
and CI read-only. No builds, benchmarks, recovery tests or capacity measurements were run.

Claude should apply this document when designing the host adapter, company/venue inventory
model, command payloads and persistence schemas. Follow the standalone plan's validation
requirements, two-job local compile limit and opt-in local clang-tidy policy. Record actual
completion in the roadmap only when the relevant acceptance evidence exists.
