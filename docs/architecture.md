# Architecture

## Engine and game responsibility contract

Manifest owns all of its production game rules and data. Jettison provides reusable
capabilities for multiple game types, with space-specific mechanics selected explicitly
rather than required by generic simulation, physics or scene APIs. Engine-local gameplay
examples and fixtures are permitted for automated testing; they must be separate targets
and content packages, not a production dependency of Manifest or the neutral engine host.

For each integration task, read the current [development stages](development-stages.md),
[gameplay requirements](gameplay-and-server-plan.md) and [boundary assessment](engine-game-boundary-assessment.md).
Map the required mechanic to public SDK facilities. When a reusable capability is missing,
specify its contract and tests for implementation in Jettison, then consume its committed
SDK revision here. Manifest keeps policy, authorisation, balance, content and composition.
Do not work around a missing SDK seam by copying engine infrastructure into the game.

The boundary assessment records current violations and planned extraction. The existing
engine app still includes sample trading, and several modules still assume star systems;
this documentation does not claim those have been separated. The descriptions below
record the current Manifest implementation unless explicitly marked planned.

## Current module architecture

Manifest is a Jettison consumer. `source/external/CMakeLists.txt` fetches the pinned engine first,
with its tests, installation and dependency probe disabled. The engine owns all shared dependency
versions and ABI definitions. Manifest reuses `EnTT::EnTT`, `yaml-cpp::yaml-cpp`, and
`doctest::doctest`; it does not redeclare them. FTXUI 7.0.3 is development-only and optional.

The source tree follows the engine's categories. `source/game/` is the counterpart of
Jettison's `source/engine/`: one directory per module, with its own `CMakeLists.txt`,
`include/manifest/<module>/`, `src/`, and unit `tests/` where applicable. Public files are
PascalCase. Developer tools live under `source/tools/`; cross-module suites under `source/test/`.

The modules, in dependency order:

| Module | Owns | Depends on |
| --- | --- | --- |
| `types` | Decimal prices; effect domains, stats, modifiers and operators | Standard library, Jettison Core invariant reporting |
| `config` | Configuration records: ages, categories, items and skills, header only | Standard library |
| `universe` | Ordered universe name pools | Standard library |
| `progression` | Prerequisites, tiers, costs and exclusive branches | Config |
| `sim` | World generation, production, consumption, pricing and trade routes | Config, universe, Jettison Math |
| `ecs` | `EntityStore`, a string-ID entity store, and plain components: development and the market and effects code, off the hosted world until Stage 4 moves them | Types, engine EnTT |
| `effects` | Catalogue and ship/civilisation effect systems | Types, config, progression, ECS |
| `market` | Venues, reservations, order matching and settlement | Effects, simulation, ECS, progression |
| `content` | Reflected definitions loaded through `Jettison::Sim::DefinitionRegistry`, record names and migrations, the content set's validation and the canonical content manifest and fingerprint | Config, universe, progression, sim, effects, Jettison Reflect, Serialise, Sim and VFS |
| `world` | The hosted state on the engine's `SimWorld`: owner identities, organisations, regions, accounts and the saved counters new IDs come from (`Identities`), seeded from the scenario | Content, Jettison Sim |
| `command` | The one admission path into the hosted world and every actor's saved request ledger: ordering, retries, bounds and structured results (`Requests`) | World, Jettison Serialise and Sim |
| `company` | A player's company as commands on that path: provisioning (principal, company and credits, home region, docked starter) and its ships' cargo loaded and unloaded at their venue | Content, World, Command |
| `query` | What an actor may see of the hosted world: a value snapshot at a completed step of their own company's credits, stock, ships and cargo (`Observe`); no market yet | World, Command |
| `host` | Manifest composed into `Jettison::Host`: package policy and start-up | Content, world, command, Jettison Host |

`Manifest::Host` is outside `Manifest::Game`, so the model never depends on the host that drives
it. `Manifest::Host::Start` mounts the options' sources read-only, the package and then each
explicit overlay ([D-070](decision-ledger.md#d-070)), with no user mods or writable mount, and
starts a `Jettison::Host::Session` whose world holds the scenario's state
(`Manifest::World::CreateInitialState`). The engine
checks the description's format, engine requirement and required packages; Manifest requires a
`mod.yaml` named `Manifest` with a release and an engine requirement, and loads its definitions
into the session's registry as one content set for the scenario (`Manifest::Content::LoadScenario`,
its game mode the scenario's), so a record that is missing, unreadable, damaged or breaks a
rule fails start-up, naming the record, the field and its mounts. The `manifest` executable (`source/apps/manifest/`) is
`manifest <options> --check-startup`: it starts, logs readiness, shuts down and exits 0; a failed
start-up logs the phase, source and cause and exits 1, a bad command line prints usage and exits 2.
`manifest-validate <options>` (`Manifest::Host::Validate`) loads the same sources with the same
rules and no world, and logs the mounts in order, the scenario and the content fingerprint, with
the host's exit statuses, and with `--manifest <file>` writes the content manifest, which the build
compares between the staged directory and its pack; the options are recorded under
[versions, layering and migration](stage-2-content-plan.md#versions-layering-and-migration).

`cmake/ManifestModule.cmake` mirrors Jettison's module helper: static module libraries, public
header sets, private warnings and implementation includes, and doctest suites. An edge to a module
not yet added fails configuration. `Manifest::Game` aggregates the module targets, analogous to
`Jettison::Engine`; `Manifest::Core` remains a consumer alias. The existing C++ types retain their
`Manifest::Core` namespace, so reorganising implementation modules does not rename symbols again.

The core compiles with `-ffp-contract=off`. The `sim` module uses Jettison's PCG32 generator and
distributions, with a specified Fisher-Yates shuffle instead of `std::shuffle`. Maps and stable IDs
keep simulation iteration ordered. Replay tests pin exact generation and hundred-tick state hashes
across Debug, Release and sanitised builds.

Module unit tests sit beside their implementations. The effects suite exercises the catalogue,
progression, ships, civilisations and markets together, so it is
`source/test/EffectsIntegrationTests.cpp`. `DataTests.cpp`, `ModTests.cpp` and `TestHelpers.h` also
belong in `source/test/`. They do not publish game headers.

`manifest_tui` is a development executable outside the simulation. Its `ManifestConsole` session
registers the development commands with `Jettison::DevTools::Console`, the same backend used by
Jettison's shell. The engine owns tokenisation (including quoted arguments), command dispatch,
help, clear, vars, history, completion, log capture and the bounded 2000-entry scrollback. Manifest
commands validate argument counts and numeric bounds; rejected commands become console errors.
Configuration reloads replace the session together: a failure preserves the previous configuration
and world, while a successful load resets the world and progression before the next generation.

The `manifest_console` tooling library links `Jettison::Devtools`, reusing its Core, Reflect and
ImGui dependencies. The console backend draws nothing; the terminal does not initialise ImGui,
Vulkan or the engine application. `manifest_tui` alone owns FTXUI, wall-clock refresh and the
command transcript. Its sidebar, history navigation and Tab completion read the engine's console
rather than keeping a second command list or history. A build-time path locates the staged
package independently of the working directory. Console tests drive the session without a terminal.

The mod package is content: `mod.yaml` and `manifest/definitions/`, one record per definition in
the engine's form. The host, the console and the data suite read it through one entry point,
`Manifest::Content::LoadContent`.
`manifest_mod` stages it and `manifest_pack` uses `jettison_add_pack`. The mod test reads both
forms with `Jettison::Vfs::FileSystem` and compares their contents. These paths are namespaced so
mounting Manifest does not replace the engine sample's definitions accidentally.

The [host/script boundary](host-script-boundary.md) defines the gameplay integration: native
Manifest modules register authoritative state and systems in one Jettison simulation world;
Wasm scripts supply bounded policy inputs through the engine's utility decisions and script host.
The first planned policy scores feasible trades. Event handlers initially observe outcomes only.
This contract does not add runtime bindings or a game executable.

The current C++ loaders still accept host file paths. [Market prices](market-prices.md) use a
decimal representation; quantities, population, effect stats and physics retain floats/doubles,
and existing credit balances and fees remain integers. These are development APIs, not a Jettison
script or save ABI. Engine API conversions, VFS-only content and capability-limited scripts
remain integration work; see the roadmap.

## Dedicated server and administration (planned)

The [gameplay and persistent server plan](gameplay-and-server-plan.md) defines the planned
ownership and recovery contracts. The [regional correction](regional-economy-and-governance-plan.md)
separates physical planets from their multiple economic/governmental regions. Players control
companies and initially one region's development/public budget; AI governs other regions.
Regional population, household/public accounts, health/happiness and local projects are
authoritative; planet totals are derived. Company assets, government/control, bloc membership,
civilisation affiliation and physical body remain distinct without duplicate population.
Goods reside at venues or aboard ships, with native loading, unloading and reservation
checks. Worlds advance while the server runs and resume without downtime
catch-up. These contracts must guide host integration; the existing development classes
do not yet implement them.

The planned host persists accepted command ordering before acknowledging durable success,
recovers through atomic snapshots and journal replay, and pins world content compatibility.
Clients receive filtered snapshots and use retry-safe commands. See that plan for acceptance
criteria, content validation, operational requirements and deferred scope.

The approved direction is a dedicated network server, launched through a CLI and hosted on a
server platform. It owns the authoritative simulation and networking and runs unattended under
a service manager or container platform, without a renderer or interactive terminal. Startup
uses configuration and command-line arguments; operation provides logs and clean shutdown.
This is a future architecture, not an existing server implementation.

The existing TUI is to become a required remote gameplay client with `connect <endpoint>` and
`disconnect`. Extract its authoritative state into the server, retain useful FTXUI/console
presentation and share a client command/query layer with Manifest's GUI. Clients display
filtered server snapshots and submit intentions. The GUI can use the selected public engine
features for presentation; the server selects simulation capabilities without rendering,
audio, input or a terminal. Administration has separate explicit permissions, and all state
changes enter the controlled fixed-step command path.

The world, NPC AI, autopilot, enabled physical auto-trading and development advance with zero
clients connected. Disconnecting cannot stop enabled jobs, relinquish a region or govern the
server lifetime. Persist standing policies and progress; reconnect returns current state,
retry outcomes and a bounded activity summary. Manual spaceport trading in the TUI is a
stretch goal; auto-trading is required. Do not use presentation event dispatch to schedule
authoritative work. Preserve command/protocol coverage independently of FTXUI and rendering;
today console tests are still gated by `MANIFEST_BUILD_TUI`.

The [planetary/corporate impact plan](planetary-development-plan.md) extends these contracts.
Later AI regional-control purchases must reject player-governed regions, including acquired
regions, while allowing otherwise eligible AI neighbours on the same planet without a
sibling governor's veto. Transfer public decision rights, retain public money and private
asset owners, and preserve reserved onboarding regions. Acquiring one region never implies
sibling control. Frontier resource concessions cover eligible resources in one named region;
neighbours retain independent eligibility and timers. Port construction eligibility is regional;
ports may serve several regions on the same planet under owner terms and protected bookings.
The open-ended aim is corporate control of
trade, economies, government and media; there is no win condition. These game rules belong in
Manifest. Jettison supplies reusable host, transport, archives, world services and optional
spatial capabilities as their public gaps are provisioned. Networking is not implemented merely
because in-memory command/snapshot facilities exist.

Corporate spaceports are a further physical/control boundary: a company can fund an expensive
orbital port for its own, an eligible AI or an eligible unclaimed region. Each eligible region
has one initial construction opportunity, unaffected by neighbouring ports. Other players'
governed regions are protected against unsolicited construction. Physical stations retain a
named regional association for eligibility; frontier concession expiry enables competing ports.
Fees, priority and rival-access refusal are owner policies. Resource exploitation and colonies
with company-controlled puppet governments connect the port to long-term revenue and influence.
Manifest owns construction, control links, contracts, extraction and separate colonial/public
accounts; Jettison needs reusable docking admission and queue-policy seams. Existing docking
checks authorise the ship's controller, not admission to the station.
The regional construction rule supersedes earlier planet-wide port eligibility. Shared ports
may serve several regions on the same planet under owner-chosen terms and protected bookings.
Physical deliveries name a destination region; shared service cannot pool economies, transfer
government/resource rights, consume neighbouring construction opportunities or reveal other
stations' live market data. Onboarding requires an operational starter-compatible public
station and physical regional delivery connection, with continuing basic docking/trading/delivery
access corporate players cannot revoke. New players also require an available working
station life-support connection and spawn connected, with actual connection state preserved
through recovery. Initial starter supplies are 30 kg oxygen and one fresh installed
30-occupant-day cartridge, provisioned once and then preserved as actual remaining state.
Operating reserves use dedicated service space outside the starter's 20 m³ cargo hold;
trade supplies and loose spares use cargo space. Track stock locations and conserved
transfers. Compartment dimensions, station provisioning and later outages remain open. Charges,
finite capacity/goods/funds and private-port policies remain. Detailed transport, starter
fees/capacity/funds, onboarding/claim compatibility
and control changes during construction still need explicit rules. Resource concessions cannot
block exploitation in independently eligible siblings.
Physical planet, station ownership and regional government are separate predicates.

The [physical economy/development plan](economy-and-development-plan.md) refines the rules:
authorised credit settlement is instant, while goods retain title/custody/location through
mines, haul, warehouses, ships and local delivery. Brokers trade finite stock/funds, earning
spreads alongside separate warehouse handling charges. Production/facilities and physical
resource imports gate new supply and tech; scarce stock and demand drive broker pricing.

Initial frontier exploitation is exclusive for real elapsed server-running time, pausing
during shutdown. Afterward rivals may exploit and build ports even under a puppet government;
starting homes remain protected. This amends the initial no-second-port restriction on
reopened concessions. The host persists running-time observations and expiry events through
ordered world inputs; simulation systems/replay do not read the wall clock. Neither a
restart nor government control can renew a frontier grant or convert it to home protection.

Later frontier expansion must preserve stable physical identities and current journeys and
recover generated topology before restoring world entities. Generated world data is distinct
from frozen authored rules. The [stargate plan](stargate-expansion-plan.md) requires funded
automated unmanned FTL delivery before new-system access, with distance-based server-running
flight duration and owner-controlled gate access/fees. The delivery needs no pre-existing
destination lane; regular SDK lane travel alone does not establish that bootstrap capability.
The inspected engine registration APIs do not yet demonstrate
atomic topology publication or durable generated-system recovery. Protocol/transport, hosting,
deployment tooling and days/months of server pacing remain design choices.
