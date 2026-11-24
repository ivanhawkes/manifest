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
| `space` | The space setting, moved from Jettison in its space extraction (2026-11-23): star systems, their orbits and the galaxy as data, a star system as a physics frame, and cruising, lanes, gates and routes on the engine's transit, portals and slots; reflected names its own since the extraction's seventh slice (`Space.StarSystem`, `Space.Cruising`) | Jettison Ai, Physics, Sim, Spatial and Transit |
| `content` | Reflected definitions loaded through `Jettison::Sim::DefinitionRegistry`, record names and migrations, the content set's validation and the canonical content manifest and fingerprint | Config, universe, progression, sim, effects, space, Jettison Reflect, Serialise, Sim and VFS |
| `world` | The hosted state on the engine's `SimWorld`: owner identities, organisations, regions, accounts and the saved counters new IDs come from (`Identities`), seeded from the scenario | Content, Jettison Sim |
| `command` | The one admission path into the hosted world and every actor's saved request ledger: ordering, retries, bounds and structured results (`Requests`) | World, Jettison Serialise and Sim |
| `company` | A player's company as commands on that path: provisioning (principal, company and credits, the starter docked at the scenario's starter venue) and its ships' cargo loaded and unloaded at their venue | Content, World, Command |
| `query` | What an actor may see of the hosted world: a value snapshot at a completed step of their own company's credits, stock, ships and cargo (`Observe`); no market yet | World, Command |
| `procurement` | Regions' public projects and their procurement (W12): tenders and their accounts, sealed bids and the award, contracts delivered, paid and fulfilled or in default; a project's inputs consumed once, its work at the region's workshop and the recipe it unlocks there | Content, world, command, trade |
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
(`source/tools/console`, built and tested with `MANIFEST_BUILD_TUI` off) registers the development commands with `Jettison::DevTools::Console`, the same backend used by
Jettison's shell. The engine owns tokenisation (including quoted arguments), command dispatch,
help, clear, vars, history, completion, log capture and the bounded 2000-entry scrollback. Manifest
commands validate argument counts and numeric bounds; rejected commands become console errors.
Configuration reloads replace the session together: a failure preserves the previous configuration
and world, while a successful load resets the world and progression before the next generation.
Those commands play a local sandbox no player shares. The hosted commands (`host`, `observe`,
`provision`, `cargo`, `trade`, `cancel`, `travel`, `book`, `step`) drive one hosted world, started through `Host::Start`, only through its
admission (`Command::Requests`) and `Query::Observe`; `Host::HostedServices` gives a frontend the
world's services. Nothing in the sandbox reaches the hosted world, and there is at most one.

The `manifest_console` tooling library links `Jettison::Devtools` and `Manifest::Host`, reusing its Core, Reflect and
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

## Space

The `space` module is the space setting, moved here from Jettison in its space extraction
(2026-11-23, [Jettison's plan](../../jettison/docs/space-extraction-plan.md)). It builds on the engine's neutral
capabilities, frames and rails, transit, portals and slots, steering and the docks
([Jettison's architecture](../../jettison/docs/architecture.md#transit)), and the engine names none of it. Its
reflected names are `Space.*` since the extraction's seventh slice.

### Star systems and the galaxy

A star system (`space/StarSystem.h`) is a physics frame (`Space::DefineStarSystem`,
`StarSystemFrame.h`): its bodies on Kepler orbits (`Orbit.h`) are the frame model's rails, a
station's kinematic body follows its orbit, and each body with a gravitational parameter pulls as a
point mass. The galaxy (`Galaxy.h`) is a graph: each system by name, with its system file and its
position in light years, and each lane joining a jump point in one system to a jump point in
another, travelled both ways, with a transit time in simulation seconds. A jump point is the end of
one lane, so a gate says where it leads. A lane's toll, in whole credits, is the light years between
its systems at the galaxy's toll per light year. Each system's file is checked against the lanes
that end in it when it is loaded.

### Jumps

A jump carries an entity along a lane (`Lanes.h`), a portal of the engine's: `Space::EnterLane`,
from a sender that commands it (a gate sends a ship it cleared as no one in particular), takes it out
of its system through a jump point (its `InFrame` none, a `Space::OnLane` saying where it is going
and the step it arrives), gives up its destination (`Transit::Depart`), and the system `lanes`,
after physics, brings it out (`Transit::ComeOut`) at the far jump point when the transit's whole
steps are run. A body comes out at rest in the gate's own frame (or that of a station it keeps by),
facing the way the gate faces (`Space::JumpPointFacing`: its planet), with its flight controls
taken against nothing; ahead of the ring, clear of its sensor by half the ring's 60 m depth, its hull
and 20 m, so the gate does not see it pass through; and clear of every body already there and of
what came out before it in the step, on rings about the place across the way the gate faces, each a
hull's breadth and 20 m further out, so two ships through one gate a moment apart do not come out in
each other.

### Gates

A gate sends the ships it has cleared (`Gates.h`), a gated portal of the engine's: each jump point's
gate is an entity, `Space::Gate`, made at the start (`Gates::MakeGates`) and saved, a kinematic
sensor on the jump point's rails, a cylinder of the ring's radius and 60 m deep (`Gates::RingShape`),
turned as the jump point faces (`Space::BodyOrientation`). It is given in command order, as a pad
is: `Space::RequestTransit`, with its sender, clears a ship at once if the gate is free or puts it at
the back of the queue, the gate's clearance a slot and the ship's `Space::Transit` its claim. The
checks are the sender's command of the ship, the ship flying and in the gate's system and frame, a
lane leaving through the gate, the hull fitting the ring, and the game's toll rule
(`Space::TollRule`), asked whether the ship's owner can pay and told to charge it. Each step the
system `gates`, after the lanes', gives up a clearance whose ship has gone, left the frame or is
cruising; sends a cleared ship touching its ring along the lane, charging its owner as it enters;
lets every other body that began to touch a ring pass through, saying why (`Space::GatePassed`);
lapses a clearance unused for two minutes; and clears the next in each free gate's queue. A ring is
placed, not swept, so a ship crossing it at under its depth a step, 3.6 km/s, is seen.

A route across the galaxy (`Routes.h`) is A* (`Ai::FindPath`) over its jump points, each lane an
edge at its transit time and each pair of a system's gates an edge at what crossing the system
costs, which the caller says; `Space::CrossingByCourse` says it from a course through the system's
waypoints, round its star, planets and moons.

### Cruising

A system is crossed by cruising: a station 1.4 × 10⁸ m from its planet and the planet 3 × 10¹⁰ m from
the next are days apart at a flight model's 150 m/s. `Space::Cruise`, with its sender, takes a ship
to a body of its system (`Cruise.h`), a passage of the engine's: it leaves physics and its frame, its
body kept in its `Space::Cruising` until it arrives, placed each step along its course by the system
`cruise`, after the lanes', and arrives at rest facing the body, a quarter of the body's radius and
4 km beyond it, in the body's frame or, for a gate kept by a station, the station's. A cruise to a
body with gravity arrives no nearer than where it pulls at 1 m/s², which a ship's assist holds
against. A frame changes at a cruise's ends and a lane's and nowhere else. A cruise does not engage
within a station's reach, so docking stays in physics; `Space::ClearOf` flies a ship out of it first.

How long a cruise takes is the ship's drive (`Space::CruiseDrive`): the seconds a cruise of 10⁸ m
takes, rising as the distance's fourth root, or a motion profile (`Transit::MotionProfile`), a boost
acceleration for at most a time, half at each end, and a sustained one between, which may thrust for
only a share of the time (`thrustShare`) and coast between, `Transit::Thrust` giving each end's
seconds, which fuel is priced from. Where the target will be on arrival is solved rather than
chased, the time and the target's place each from the other in eight rounds. The course is a few
straight legs round the stars, planets and moons in the way (`Ai::Waypoints`, 2 km clear of each),
plotted relative to its carrier, the nearest body both ends orbit, and carried with it; a body in
the way is placed where the ship passes it by its time. Along its course a ship eases out and in, or
covers it as its profile does, slowed to whole steps, and faces along its leg. Arrivals, the lanes'
and the cruises', share one placement (`Transit::ClearPlace`), clear of every body in the system by
both radii and `kArrivalClearance`, in entity order, a gate's ring being no body an arrival keeps
clear of.

The retired sample's presentation of these, the jump's transition behind a fade, the galaxy panel
and the flight panel, is in [Jettison's history](../../jettison/docs/history.md), for the graphical client (L15).

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

The host recovers through atomic snapshots and journal replay, losing up to 30 s of play in a crash
([D-113](decision-ledger.md#d-113)), and pins world content compatibility.
Clients receive filtered snapshots and use retry-safe commands. See that plan for acceptance
criteria, content validation, operational requirements and deferred scope.

The approved direction is a dedicated network server, launched through a CLI and hosted on a
server platform. It owns the authoritative simulation and networking and runs unattended under
a service manager or container platform, without a renderer or interactive terminal. Startup
uses configuration and command-line arguments; operation provides logs and clean shutdown.
Its first slice exists (N02): `manifest --serve` steps the world unattended and serves it
through the `remote` module on Jettison's `net`, with `manifest-token` for its tokens; a world's
directory is made and held by one server (`Host::OpenWorldDirectory`, R02), which writes what has
run to the world's journal every 30 s and its save every hour, and recovers from them at a start,
a crash losing up to the 30 s ([D-113](decision-ledger.md#d-113), `Host::Journal`,
[the journal and its snapshots](stage-5-recovery-plan.md#the-journal-and-its-snapshots-r03-r04-2026-11-24)).
A stop runs what it admitted and writes a snapshot, a backup is written beside every snapshot and
`--restore` restores one into an empty directory
([R05](stage-5-recovery-plan.md#shutdown-backups-and-limits-r05-2026-11-24)). Hosting outside a
trusted network does not exist yet.

The existing TUI is to become a required remote gameplay client with `connect <endpoint>` and
`disconnect`. Extract its authoritative state into the server, retain useful FTXUI/console
presentation and share a client command/query layer with Manifest's GUI. Clients display
filtered server snapshots and submit intentions. Its `connect` and `disconnect` exist (N02), beside
the local hosted commands of the development harness. The GUI can use the selected public engine
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
