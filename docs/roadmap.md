# Roadmap

The [next development stages](development-stages.md) retain the foundational host, content,
ownership, trading and recovery plans. The subsequent networked idle-game and planetary
development goals below extend that baseline. Networking is a required gameplay boundary;
the old optional administration-only TUI direction is superseded. The delivery amendments
are planning proposals, and none of these capabilities is marked implemented yet.

## SDK and build baseline

The [engine/game boundary assessment](engine-game-boundary-assessment.md) adds pending
cross-repository work: separate Jettison's small space-trading example from engine code,
provide a neutral host and consumer build, and isolate optional space mechanics. Check
Manifest's requirements at each stage and provision reusable SDK gaps in Jettison before
binding the game to them. The example remains useful automated-test coverage; Manifest
owns production gameplay/content. These changes are not included in the completed
historical baseline below.

Completed: consume a pinned `feature-base` Jettison SDK, share dependency targets and development
environment, fix the migrated source layout, apply engine naming and warning conventions, use
doctest, build Debug/Release/sanitised presets, and stage and pack the YAML content as a mod.
The engine RNG and exact state hashes establish a replay check for the development simulation.
The development TUI shares the engine shell's console backend for commands, history, completion
and log capture; its command session is tested independently of terminal rendering.

## Gameplay integration

The [gameplay and persistent server plan](gameplay-and-server-plan.md) fixes the initial
product scope and acceptance scenario: company-owned trading, venue-local inventory,
physical delivery, persistent worlds and server-authoritative commands. Apply these
decisions to the adapters and save schemas below; they remain planned, not implemented.

1. **Defined:** the [host/script boundary](host-script-boundary.md) keeps authoritative economy,
   progression, effects and settlement in linked native game modules. Capability-limited Wasm
   supplies bounded policy inputs, starting with trade preference, and optional event observers.
   Reuse Jettison's world, decisions, reflection and script host; there is no native C++ mod ABI.
   Runtime adapters and an engine-hosted scenario remain in items 2–5 below.
2. **Completed:** [decimal market prices](market-prices.md), with explicit conversion, overflow, rounding
   and reservation tests. Preserve authored IDs, units and prices. Quantities, physics, effect
   stats and other continuous values retain floats/doubles; existing credit ledgers remain
   integers. This supersedes the earlier proposal to convert every economic value to fixed point.
3. Load configuration from Jettison's VFS and definition registry, including layered patches,
   schema versions and migration. Current path-based loaders remain development APIs.
4. Bind progression, economic state and commands to the engine's fixed simulation step, replay,
   serialisation and state hashing. Keep presentation and player input outside simulation code.
5. Add the game's scripts, definitions and presentation assets to the mod staging target. Verify
   an engine-hosted scenario loaded from both a directory and a pack before calling the mod playable.

The existing model and content are recorded in [model reference](model.md).

## Networked idle game and remote clients

**Required direction:** provide a CLI-launched dedicated network server that advances one
authoritative world with zero connected players. Economy, NPC AI, autopilot, enabled
auto-trading and planetary development run independently of rendering or terminal refresh.
Players can leave and return while their standing policies continue. Server downtime still
pauses simulation initially; downtime catch-up is a separate deferred feature.

Morph the existing TUI into a remote gameplay client with `connect <endpoint>` and `disconnect`.
It submits authenticated commands and displays filtered server snapshots. Closing or losing a
client cannot pause the world, relinquish a planet or stop enabled automation. Share the client
command/query layer with Manifest's GUI; the GUI can use all selected public engine features.
Keep administrative permissions separate from ordinary gameplay. Manual spaceport trading in
the TUI is a stretch goal; configuring and observing physical auto-trading is required.

Preserve console workflows where useful, but move authoritative state out of `ManifestConsole`.
Host, protocol and command tests must run without FTXUI or a renderer. Reuse Jettison's public
simulation, AI, transit and persistence facilities; provision demonstrated generic host/network
gaps in Jettison. Current snapshots and render smoke tests do not establish network replication
or a true CPU-only server build. Transport, hosting and measured capacity remain open.

Proposed sequencing: host/content/command foundations, then a small real TUI connection slice,
then physical idle trading and recovery, followed by graphical presentation. Preserve durable
acknowledgement requirements before production hosting. The four-client scenario is an initial
network test, not a limit on registered players or a demonstrated supported capacity.

## Open-ended corporate power

**End goal, 2026-11-15:** grow a trading company into an enormously powerful corporation
with influence over government and media, drawing on the scale of Earth's largest
corporations. Planetary development, logistics, acquisition and settlement support that
corporate progression. There is no win condition: players accumulate wealth, power and control
and use their leverage to establish a stranglehold on trade, economies and governments.
Company wealth, development rights, civilisation rank and political or media influence are
distinct concepts. Specific influence mechanics, rival responses and server lifespan remain
planning decisions. A duration of days or months does not imply a timed victory or reset.
The first leverage mechanics to design are infrastructure, essential supplies and access
contracts. Government financing and media influence follow. A powerful company can build and
control a space station, set fees and shipping priorities, and refuse rivals access to it.
Initial corporate spaceports may be built at eligible planets with no existing port that are
not owned/controlled by another player; the frontier reopening exception is defined below.
They are very expensive and enable profits through resource exploitation and colonies with
company-controlled puppet governments.
Purchasing planetary control is not required before building at an otherwise eligible AI planet.
The station must become a physical source of commercial leverage; ownership alone does not
make it a mandatory gate for every ship in the system. See the [impact plan](planetary-development-plan.md).

New resource concessions give an initial exclusive exploitation period measured in real
elapsed server-running time; shutdown time does not count. Afterwards rivals may exploit
the planet and build competing spaceports, even under the incumbent's puppet government.
Starting home planets stay protected. This amends the earlier no-second-port rule for reopened
frontier worlds. Incumbents retain infrastructure/capital advantages and may try to exhaust
resources before rivals arrive. Duration and the precise first-grant trigger remain undecided.

## Physical economy and material-gated development

Credits transfer instantly between authorised entity accounts. Commodities follow the full
chain: extraction/industry, local haul, station warehouse, ship, destination warehouse and
local use. Brokers trade real stock using their own funds; their buy/sell spreads and separate
warehouse handling charges at both ends provide the intended substantial merchant income.

Production/extraction capacity caps new output; actual unreserved inventories cap trade.
Scarcity and demand determine prices, rather than production-based price bands. Auto-trading
must account for both broker prices, both warehouses and physical operating/access costs.
Planetary tech advancement requires resource exploitation, imports of unavailable resources
and actual production facilities, not just credits or knowledge prerequisites.

Inner map worlds start at higher civilisation technology with depleted natural reserves;
outer worlds are generally resource-rich and less developed, creating a frontier gold rush.
See the [economy/development plan](economy-and-development-plan.md) for custody, production,
brokers, tech dependencies, geography, real-time rights and the capability/validation map.

## Planetary development, acquisition and frontier settlement

**Added goal, 2026-11-15:** each player initially controls one planet's development and public
budget alongside a separate trading company. AI governs other inhabited planets. A server
starts with enough suitable planets for its intended group, later joiners and an AI economy.
The intended play period may be days or months; campaign duration and pacing remain undecided.

The first development controls are projects, investment budgets and public procurement.
Corporations must bid competitively for public contracts, including the governor's company.
After clean tendering works, add a simple government corruption trait and abstract bribery
that can influence eligible bid rankings. Detailed political/official simulation is deferred.
Detailed taxes, subsidies and wider economic regulation are later design work.

Suppliers are paid for accepted partial deliveries as they arrive. Station owners honour
accepted bookings; changed fees, priorities or exclusions apply to future requests. The
[contract/booking plan](procurement-and-station-contracts-plan.md) proposes the clean tender,
physical fulfilment/payment lifecycle and the precise commercial acceptance boundary.

Players may later purchase control of eligible AI-controlled planets or settle frontier worlds.
Other players' planets are protected from acquisition through this mechanic. One planet is a
starting entitlement, not a permanent ownership cap. Public and company funds, inventory and
permissions remain distinct; visitors' private assets retain their existing ownership.

If needed, add systems at the outer reaches of the existing map without moving or regenerating
established systems. Frontier planets are typically less developed and resource-rich, with
some uninhabited and suitable for terraforming and settlement. Expansion, acquisition and colony rules require
separate milestones; they are not currently implemented engine or game capabilities.

See [planetary development and frontier planning](planetary-development-plan.md) for impacts,
the capability map, unresolved choices and acceptance criteria. Its staged direction is:

1. Account for governance and independent planet identities in the foundational state design.
2. Prove one governed planet, AI neighbours and saved development queues in the idle server.
3. Add atomic purchases of eligible AI planets, protected starter capacity and multi-planet policy.
4. Add expensive corporate spaceport construction at eligible unserved planets, physical
   resource exploitation and colonies with company-controlled puppet governments.
5. Prove deterministic frontier expansion, physical access, terraforming and settlement.

The original one-system/two-venue scenario remains a small integration fixture. A later
governance scenario must include a home planet for each test player and additional AI planets.
Do not interpret the old deferral of player-controlled civilisations as excluding this new
planet-development goal. Whole-civilisation sovereignty remains outside the agreed scope.
