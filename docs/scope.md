# Scope

What Manifest is for, and what it builds, plans, defers and leaves out, area by area. Each area
links the [feature document](README.md) that says how the built part works, and the plan and
[task IDs](glossary.md#task-id) of the open part. The engine's own scope is Jettison's
[scope](../../jettison/docs/scope.md).

## Product intent

Manifest is an idle, networked space-trading game on the Jettison engine. Its prime purpose (the
user's, 2026-11-21) is a playfield: a place where people experiment with, and learn, how greed,
fear, monopolies and other modern woes affect everyone's lifestyle, income and happiness.

- **No win condition** ([D-010](decision-ledger.md#d-010)). Players seek ever greater corporate
  wealth, power and control over trade, economies, governments and media. Developing and
  expanding planets serves that open-ended aim. A server's lifespan and pacing over days or months
  are undecided.
- **The world runs without its players** ([D-011](decision-ledger.md#d-011)). One authoritative
  [hosted world](glossary.md#hosted-world) runs on a dedicated server; its economy, NPC traders and
  standing policies advance with no client connected, and a disconnect does not pause it.
- **Balance numbers are starting points.** They are refined stepwise, with an understanding of
  macroeconomics, toward an enjoyable game, not a faithful emulation of today's economy. Markets
  are liquid and volatile, their swings driven by [needs](glossary.md#need) and spending power,
  not by noise.
- **Goods are physical.** Credits move at once, but goods are carried in ships and held in
  [totes](glossary.md#tote); supply is limited by production, and scarcity sets the price.
- **A player starts small** ([D-089](decision-ledger.md#d-089)): a company, its credits and a
  starter ship at a neutral public [station](glossary.md#station), trading at once. A homeworld
  comes later, founded with what trading has earned ([D-110](decision-ledger.md#d-110)).

## The boundary with Jettison

Jettison owns rendering, input, audio, physics, scheduling, the VFS, serialisation, networking and
the script runtime; Manifest owns its rules, prices, content and screens, space included. The
boundary and how a gap in the engine is provisioned are in Jettison's
[game integration](../../jettison/docs/game-integration.md); Manifest's side is in
[architecture](architecture.md).

| Item | Verdict | Why |
| --- | --- | --- |
| Authoritative rules in linked native modules | In | One build of the rules, deterministic and tested ([D-004](decision-ledger.md#d-004)) |
| A second renderer or engine dependency stack | Out | Jettison is the engine; a gap is provisioned there and consumed through the [pin](glossary.md#pin) |
| A native shared-library mod ABI | Out | Mods change data, and later policy through sandboxed scripts ([D-004](decision-ledger.md#d-004)) |

## Verdicts

| Verdict | Meaning |
| --- | --- |
| In | Built and tested. Open work on it is a queue row, linked |
| Planned | Accepted, not built: a row in the [delivery order](delivery-order.md), linked |
| Deferred | Possibly wanted; taken up only when a later mechanic needs it |
| Out | Not built in Manifest |

## Content and mods

How: [content, scenarios and validation](content.md).

| Item | Verdict | Why |
| --- | --- | --- |
| Authored YAML content, one validated [content set](glossary.md#content-set) | In | Designers change goods, hulls, venues and tuning without code; `manifest-validate` checks a set before it runs |
| [Scenarios](glossary.md#scenario) and [game modes](glossary.md#game-mode) | In | A world starts from authored data, not code |
| Manifest's content as a Jettison [mod](glossary.md#mod) (`mods/manifest`) | In | Overlays layer through the engine's own mounts ([files and mods](../../jettison/docs/files-and-mods.md)) |
| Policy scripts in Wasm | Planned, [L39](delivery-order.md#l39) | Players' and modders' trading policy as bounded inputs to native rules ([policy scripts plan](policy-scripts-plan.md)) |
| Player-facing text in English, French, German and Japanese | In, L43 ([history](history/tasks.md)) | Every string key in all four tables (the user's, 2026-11-22), which a check holds ([languages](client.md#languages)). The tables are for stress-testing the GUI now, German's long words, French's diacritics and Japanese kanji, and for native translators later, who replace the first drafts before release. The engine's mechanism is `Ui::StringTable` ([Jettison's UI](../../jettison/docs/ui.md), open parts [J15](../../jettison/docs/roadmap.md#j15)) |

## Host, server and clients

How: [the host](host.md), [the server](server.md), [the console and the terminal UI](console.md),
[saves](persistence.md).

| Item | Verdict | Why |
| --- | --- | --- |
| A standalone bundle for x86-64 Linux | In | One tarball: server, validator, token tool and TUI beside the packed content ([D-114](decision-ledger.md#d-114)) |
| A Nix closure as a supported runtime | Out | The bundle is the product; NixOS runs it through nix-ld ([D-114](decision-ledger.md#d-114)) |
| A dedicated server with no renderer or terminal | In | `manifest --serve` steps the world with zero clients ([D-011](decision-ledger.md#d-011)) |
| The TUI as a remote client | In | `connect` and `disconnect` play on a server as a token's actor ([D-012](decision-ledger.md#d-012)) |
| Administrators apart from players | In | An administrator's token provisions anyone's company; a player's only its own, once ([D-122](decision-ledger.md#d-122)) |
| Manual trading from the TUI | In | Typed commands (`trade buy`, `cargo load`), locally or on a server; the stretch goal of [D-013](decision-ledger.md#d-013) |
| Saves, the [journal](glossary.md#journal) and backups | In | A restart resumes the world where it stopped |
| The world catching up for time the server was down | Deferred | Downtime pauses the world ([D-011](decision-ledger.md#d-011)) |
| Saves off the step's thread | Planned, [L40](delivery-order.md#l40) | A large snapshot should not stall the step ([D-115](decision-ledger.md#d-115)) |
| Stage 5's loose ends (TUI retry, the bundle test, debug files) | In, [L41](history/tasks.md#l41) | The TUI resends a lost request ([D-125](decision-ledger.md#d-125)); CI runs the bundle in containers |
| Encrypted connections, hosting outside a trusted network | In, Jettison's [J01](../../jettison/docs/history/neutral-sdk.md#j01-encryption) | Every connection sealed to the server's key, which clients pin ([D-081](decision-ledger.md#d-081)) |
| Deployment hardening, capacity profiling, unattended soak | Planned, [L01](delivery-order.md#l01) | Before public hosting |
| The graphical client | In for the first loop ([the graphical client](client.md)); the rest planned, [L15](delivery-order.md#l15) | Shares the server interface with the TUI ([D-012](decision-ledger.md#d-012)); its screens are listed in the [roadmap](roadmap.md#the-graphical-client) |
| The development [sandbox](glossary.md#sandbox) | In | The older skill-tree and effects model, reached only from the console ([the sandbox](sandbox.md)) |

## The hosted world

How: [state, commands and queries](world.md).

| Item | Verdict | Why |
| --- | --- | --- |
| One authoritative world, changed only by admitted [requests](glossary.md#request) | In | Every change is ordered, retried safely and saved |
| Queries that show each actor only what it may see | In | Clients read [snapshots](glossary.md#snapshot-query), never the world |

## Companies, ships and stations

How: [companies, ships and cargo](companies.md), [stations](stations.md),
[travel and fuel](travel.md).

| Item | Verdict | Why |
| --- | --- | --- |
| Companies, the starter ship, cargo and upgrades | In | |
| [Berths](glossary.md#berth), [life support](glossary.md#life-support), station charges, [arrears](glossary.md#arrears) and [seizure](glossary.md#seizure) | In | A ship lives on what it can pay for |
| Travel by [torch](glossary.md#torch), fuel and cruise profiles | In | |
| Drives past the torch | Built, [W22a and W22b](history/tasks.md#w22b) | The torch, the hauler fusion drive and the pulse drive ([travel](travel.md)) |
| Modular oxygen banks for larger hulls | Planned, [L07](delivery-order.md#l07) | [Ships plan](ships-plan.md) |
| Algae oxygen recycling | Planned, [L34](delivery-order.md#l34) | [Ships plan](ships-plan.md) |
| Station appearance: cultures, corporate colours and logos | Planned, [L15](delivery-order.md#l15) | Detailed visual design waits for the graphical client |
| Combat and piracy | Deferred | Not designed; the first loop is trade and development |

## Trading and prices

How: [orders, brokers and observations](trading.md), [prices](pricing.md), [standing policies and
NPC traders](policies.md).

| Item | Verdict | Why |
| --- | --- | --- |
| Limit [orders](glossary.md#order), [brokers](glossary.md#broker) and settlement in whole totes | In | |
| Live and stale market [observations](glossary.md#observation) | In | A trader knows a distant market only from its last visit |
| Exact decimal prices from supply and demand | In | [D-001](decision-ledger.md#d-001), [D-102](decision-ledger.md#d-102) |
| Standing policies and NPC traders | In | Trade goes on while the player is away |
| Consignment storage | Planned, [L16](delivery-order.md#l16) | [Economy plans](economy-and-development-plan.md) |
| Freight contracts, warehouse receipts, paid price reports, forward contracts, loans, auctions, escrow | Planned, [L18](delivery-order.md#l18) to [L22](delivery-order.md#l22), [L24](delivery-order.md#l24), [L25](delivery-order.md#l25) | [Economy plans](economy-and-development-plan.md) |
| Cargo insurance | Planned, [L23](delivery-order.md#l23) | Needs a way to lose cargo first, so it follows combat and piracy |
| Options and futures | Planned, [L17](delivery-order.md#l17) | A stretch goal ([D-073](decision-ledger.md#d-073)) |
| Cases for high-value goods | Planned, [L32](delivery-order.md#l32) | |

## The regional economy

How: [the regional economy](economy.md).

| Item | Verdict | Why |
| --- | --- | --- |
| [Households](glossary.md#household), needs, [condition](glossary.md#condition), production, [payroll](glossary.md#payroll) and [wage tax](glossary.md#wage-tax) | In | A region's people buy what they need and work what they can |
| The physical chain: extraction, warehouses and transport jobs | Planned | [Economy plans](economy-and-development-plan.md); extraction comes with [L06](delivery-order.md#l06) |
| Tax rates set by a region's governor | In | A player governor sets the wage tax, an AI one steers it to its budget ([a region's wage tax](economy.md#a-regions-wage-tax)) |
| Regional debt, merchant and banking classes | Planned, [L30](delivery-order.md#l30) | |
| The power grid, compute and the AI economy | Planned, [L31](delivery-order.md#l31) | |
| Regions by kind of world; a richer, less developed outer map | Planned, [L33](delivery-order.md#l33) | Inner worlds start advanced with depleted reserves, outer ones rich and raw: a resource gold rush ([D-018](decision-ledger.md#d-018)) |

## Regions, governance and corporate power

How: [regions, governors and public projects](governance.md). Open plans:
[planetary development, corporate power and the frontier](planetary-development-plan.md).

| Item | Verdict | Why |
| --- | --- | --- |
| Several governed [regions](glossary.md#region) on one planet, each with its own people and accounts | In | [D-014](decision-ledger.md#d-014) |
| Founding a homeworld, a development queue, competitive public [tenders](glossary.md#tender) | In | [D-110](decision-ledger.md#d-110), [D-040](decision-ledger.md#d-040) |
| Simple corruption and abstract bribery | Planned, [L02](delivery-order.md#l02) | Only after clean tendering works ([D-040](decision-ledger.md#d-040)) |
| AI regional-control acquisition | Planned, [L03](delivery-order.md#l03) | Player-governed regions stay protected ([D-015](decision-ledger.md#d-015)) |
| Private-port bookings and owner policy | Planned, [L04](delivery-order.md#l04) | Basic docking, trade and delivery stay open to all ([D-048](decision-ledger.md#d-048)) |
| Corporate port construction | Planned, [L05](delivery-order.md#l05) | One opportunity per eligible region ([D-016](decision-ledger.md#d-016)) |
| Resource exploitation, [concessions](glossary.md#concession) and company-controlled colonies | Planned, [L06](delivery-order.md#l06) | Exclusivity runs on real server time ([D-030](decision-ledger.md#d-030)) |
| New regions generated as players join | Planned | Keeps regions open to founding ([D-076](decision-ledger.md#d-076)); no queue row yet |
| Government and media influence | Planned, [L14](delivery-order.md#l14) | A bounded next mechanic after L03 to L06 |
| Detailed politics; whole-civilisation sovereignty | Deferred | One region's control is not a planet's or a civilisation's |

## Space

How: [space](space.md), [travel and fuel](travel.md). Open plans: [space plans](space-plan.md).

| Item | Verdict | Why |
| --- | --- | --- |
| [Star systems](glossary.md#star-system), [lanes](glossary.md#lane), [gates](glossary.md#gate) and cruising | In | A static map Manifest owns, on Jettison's transit and docks |
| Stargates opening new systems | Planned, [L08](delivery-order.md#l08) to [L12](delivery-order.md#l12) | A funded, unmanned delivery over server time ([D-017](decision-ledger.md#d-017)) |
| Settlement and terraforming in the frontier | Planned, [L13](delivery-order.md#l13) | |
| Moongates | Planned, [L35](delivery-order.md#l35) | Gates round one planet or moon ([D-091](decision-ledger.md#d-091)) |
| Gate refinements | Planned, [L38](delivery-order.md#l38) | |
| Station traffic lanes | Deferred, [L36](delivery-order.md#l36) | Parked |

## Decisions

The product decisions are under [product and scope](decision-ledger.md#product-and-scope) in the
decision ledger; each area's are under its feature document's heading there.
