# Manifest documentation

Manifest is a persistent trading game built on the [Jettison](../../jettison/docs/README.md) engine:
a playfield for learning how greed, fear and monopolies change people's lives, incomes and
happiness. These documents describe what the game does today, what is planned, and how to work on
it. Terms are defined once, in the [glossary](glossary.md).

## Start here

| Document | Purpose |
| --- | --- |
| [Scope](scope.md) | What Manifest is for, and a verdict on each area: in, planned, deferred or out |
| [Architecture](architecture.md) | The overview: Manifest on Jettison, the repository, the modules, the step in order, threads, the server |
| [Glossary](glossary.md) | Every term, defined once |

## Features

What is built, how it works and how to use it, one document a feature.

| Document | Purpose |
| --- | --- |
| [Content, scenarios and validation](content.md) | The YAML content, units and storage, game modes, scenarios, tuning, loading, mods and overlays, content identity |
| [The host, the command line and the distribution](host.md) | Start-up, the programs and their options, the default places, the build's identity, the bundle |
| [The hosted world: state, commands and queries](world.md) | The world's state, admission and the request ledger, retries, and what each actor may see |
| [Companies, ships and cargo](companies.md) | Provisioning a player, holdings and the hold, loading, upgrades, hulls |
| [Orders, brokers and market observations](trading.md) | The book, matching and settlement, brokers, and the live and stale market reports |
| [Prices](pricing.md) | Decimal prices, base and native prices, the broker's curve and ladder, tender prices |
| [The regional economy](economy.md) | Production, payroll and wage tax, households and their needs, health, happiness and the work rate |
| [Regions, governors and public projects](governance.md) | Who governs a region, founding a homeworld, projects, tenders and bids |
| [Space: star systems, lanes, gates and cruising](space.md) | The space setting on the engine's transit |
| [Travel and fuel](travel.md) | Journeys between venues, the torch's burn, fuel and refuelling |
| [Stations: berths, life support and charges](stations.md) | Station life support, resupply, fees by class, arrears and seizure |
| [Standing policies and NPC traders](policies.md) | A ship's saved job, how it chooses and trades, contracts, NPC traders |
| [Saves, the journal and backups](persistence.md) | The world's directory, the journal, snapshots, recovery, backups and restore |
| [The dedicated server and remote clients](server.md) | `--serve`, tokens, sessions, the protocol and its bounds |
| [The console and the terminal UI](console.md) | The development console, its commands, and the TUI as a local or remote client |
| [The graphical client](client.md) | `manifest_client`: Manifest composed into Jettison's rendered shell, its screens and how it plays the first loop |
| [The development sandbox](sandbox.md) | The older model (skill tree, effects, sandbox marketplace) and what the hosted game still borrows from it |

## Plans

Open work only. Each task has an ID in the queue.

| Document | Purpose |
| --- | --- |
| [Delivery order](delivery-order.md) | The task queue: every open task in order, with what it needs first |
| [Roadmap](roadmap.md) | Where Manifest is going, by area, and the graphical client's list |
| [The economy: open plans](economy-and-development-plan.md) | Consignment, market services, futures, tax, debt, power, cases, the physical chain, recipes |
| [Planetary development, corporate power and the frontier](planetary-development-plan.md) | Bribery, buying regions, private ports, port construction, concessions, settlement, influence |
| [Space: open plans](space-plan.md) | Several systems, stargates, moongates, traffic lanes, cruising and gate refinements |
| [Ships and life support: open plans](ships-plan.md) | Drives past the torch, oxygen banks, algae recycling, the compact ship |
| [Policy scripts: open plan](policy-scripts-plan.md) | The Wasm trade-scoring script |

## Working on Manifest

| Document | Purpose |
| --- | --- |
| [Art standards](art-standards.md) | How models, textures and materials are authored: files, names, scale, geometry, materials, texture coordinate sets |
| [Coding standards](coding-standards.md) | Manifest's additions to Jettison's standards |
| [Build and gates](build-and-gates.md) | Presets, suites, the gate, CI and the bundle |
| [Agent workflow](agent-workflow.md) | How models and agents share the work |
| [Session handoff](session-handoff.md) | Where things stand, and the next step |

## Records

| Document | Purpose |
| --- | --- |
| [Decision ledger](decision-ledger.md) | Every settled decision (D-001 onwards), by feature, with its status |
| [History](history.md) | The index of finished work: task records, the closed stage plans and earlier assessments |
