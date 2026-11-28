# Decision ledger

Every settled project decision, once: a short title, the rule in a sentence or two, and its status.
Each decision has an ID, `D-nnn` ([glossary](glossary.md#decision-d-nnn)), and an anchor, so a
document links it as `decision-ledger.md#d-087`. The feature documents hold the detail and the
reasoning; a row links its feature document rather than restating it. How a decision came to be is
in [history](history.md).

The decisions are grouped by the feature document they shape, under that document's title, so a
feature document links its group (`decision-ledger.md#companies-ships-and-cargo`). Each group ends
with the decisions filed elsewhere that the feature also follows. Within a group, rows are in ID
order.

A status is one of:

| Status | Meaning |
| --- | --- |
| Implemented | Built and tested; the row names the task that built it and the suites that verify it. |
| Selected, open as *task* | Chosen by the user and not yet built; the task is its row in the [delivery order](delivery-order.md). "No task yet" marks one the queue does not hold. |
| Superseded by D-xxx | Replaced; the row that replaced it says what changed. |

A decision built in part says which part, and which task holds the rest.

## Product and scope

What Manifest is, and where it ends and Jettison begins. The product intent is in [scope](scope.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-003"></a>D-003 | Engine and game ownership | Jettison stays genre-neutral, its sample gameplay in sample and test targets; Manifest owns its rules and content. A reusable gap is provisioned in Jettison and consumed through a committed pin. See [architecture](architecture.md#manifest-on-jettison) and Jettison's [game integration](../../jettison/docs/game-integration.md). | Implemented: Manifest builds on the pin, its dependencies checked at configure |
| <a id="d-004"></a>D-004 | Native rules, bounded scripts | Authoritative rules live in Manifest's linked native modules. Capability-limited Wasm scripts supply bounded policy inputs and observe; there is no native C++ mod ABI. See the [policy scripts plan](policy-scripts-plan.md). | Implemented for the native modules; the scripts open as [L39](delivery-order.md#l39). Kept observe-only when the server came to run mods' scripts (Jettison's J62): a script sends no command (the user's, 2026-11-27) |
| <a id="d-005"></a>D-005 | A CPU-only consumer | A consumer that configures and runs with no presentation dependencies is an acceptance test of the engine's separation. | Implemented in Jettison: the `clang-cpu` preset and `source/test/consumer` |
| <a id="d-010"></a>D-010 | No win condition | Players accumulate corporate wealth, power and control over trade, economies, governments and media, and nothing ends a game. A server's lifespan and pacing are undecided. See [scope](scope.md#product-intent). | Implemented: no rule ends a game; lifespan and pacing open, no task yet |

## Content, scenarios and validation

The goods, their units and categories, the game modes and the content options. Detail:
[content](content.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-049"></a>D-049 | LPG | LPG is a distinct commodity in Fuels, by the tonne since [D-064](#d-064). Its storage, production and consumption uses are open, and it is no ship's fuel: a drive burns reaction mass and fusion pellets ([D-108](#d-108)). | Implemented (C04; `data`) |
| <a id="d-059"></a>D-059 | The first item and hull authoring | The food grains are `CategoryConsumables`, basic medical supplies `CategoryMedicalEquipment`, LPG `CategoryFuels`, and oxygen and the scrubber cartridge a new `CategoryLifeSupport`, the cartridge traded by the piece. The long-haul trader, the lone-trader starter and the compact ship are hull definitions, each unselected value left unset. | Implemented (C04; `data`, `content`) |
| <a id="d-060"></a>D-060 | Fluids by the kilolitre | Every fluid trades by the kilolitre, while a ship's oxygen stays in kilograms. Drinking water is `ItemDrinkingWater`, `CategoryWater`, of the Stone Age, by the kilolitre, its need authored per 1,000 people a day. | Superseded by [D-064](#d-064) (units) and [D-093](#d-093) (its price); drinking water by the kilolitre stands |
| <a id="d-061"></a>D-061 | Two game modes | Astralis (`astralis`) is short and fast, on a smaller list of goods to keep its play uncluttered. Manifest (`manifest`) is long-term and large-scale, on every item, ship and skill. See [content: game modes](content.md#game-modes). | Implemented (`data/game-mode/`; `content`, `data`) |
| <a id="d-062"></a>D-062 | Manifest authors Jettison's dock records | Every game starts from Jettison's ship and station definition types: pad classes, hulls and station layouts. Manifest authors its own records of them, depending on no other game's files. The starter is a skiff, the skiff class's largest hull. | Implemented (C09; `content`, `data`) |
| <a id="d-063"></a>D-063 | Categories and ages by period | The first-draft categories and ages are revised to the period each good was first made and traded in, with new categories (currency, alcohol, scientific equipment and others) and distinct descriptions. The data is the list: `data/trade-items/`. | Implemented; its grains' age superseded by [D-064](#d-064), `CategorySlaves` retired by [D-068](#d-068) |
| <a id="d-064"></a>D-064 | Each good in its market's unit | Each good trades in the unit its real market quotes: bulk goods, grains and LPG by the tonne, precious metals by the troy ounce (`CategoryPreciousMetals`), gems by the carat, minor metals, medicines and oxygen by the kilogram, drinks by the litre, water alone by the kilolitre. `Gram` is retired and the food grains and seeds move to the Stone Age. See [content: units and storage](content.md#units-and-storage). | Implemented (`data`, `content`); supersedes [D-060](#d-060)'s fluids and [D-063](#d-063)'s grain age |
| <a id="d-065"></a>D-065 | Astralis's 20 goods | Astralis trades the 20 goods an industrial-age world trades most, in place of its placeholder list; brass and bronze are `CategoryAlloys`. | Superseded by [D-066](#d-066) and [D-067](#d-067) |
| <a id="d-066"></a>D-066 | Thirty more goods | Thirty goods join, each the first product of a craft, an empty category or an empty age (timber, ores, coal, steel, crude oil, plastics, AI accelerators and others). Timber trades by the new `CubicMetre`, crude oil by the new `Barrel`. | Implemented (`data`); its Astralis list superseded by [D-067](#d-067) |
| <a id="d-067"></a>D-067 | Astralis allows 30 goods | Astralis trades 30 goods: [D-066](#d-066)'s 20, six of the goods it dropped, and four more of the industrial revolution's trade (`data/game-mode/astralis.yaml`). | Implemented (`data`, `content`) |
| <a id="d-068"></a>D-068 | Twenty-one more goods | Twenty-one goods join, among them natural gas, traded by its energy in the new `MMBtu` and kept apart from LPG. `CategoryHumans` holds tourists, prisoners, slaves and passengers, and `CategorySlaves` is retired. | Implemented (`data`) |
| <a id="d-069"></a>D-069 | No collectibles category | `CategoryCollectibles` is retired: no good was filed under it once coins became currency. | Implemented (`data`) |
| <a id="d-071"></a>D-071 | No fallback category | `CategoryUnknown` is retired: a good outside the taxonomy gets a category of its own. | Implemented (`data`) |
| <a id="d-124"></a>D-124 | The world under `defs/` | Manifest's world, its definitions and its scenarios, is under `defs/` in its package, where Jettison looks for world content (`Host::kWorldDirectories`): each record type in `defs/<kind>/`, the scenarios in `defs/scenarios/`, so the server's pack keeps it and a client's content check covers it. The rest of the package (scene, screens, words, fonts) is presentation. A server's overlay mounts as `mod:<identifier>`, its directory's name or its pack's without `.pack`, as Jettison's shell mounts a player's mod, so one mod on both ends is one package to the check. The user's, 2026-11-28. See [content: processing the YAML](content.md#processing-the-yaml). | Implemented (L53; `content`, `mod`, `host`) |
| <a id="d-093"></a>D-093 | Drinking water's price | Drinking water's base price is raised so a one-tote fill no longer rounds to nothing under the [price rules](pricing.md#from-a-price-to-credits). | Implemented (`data/trade-items/food-and-drink.yaml`; `data`); supersedes [D-060](#d-060)'s price |

Also followed here: [D-087](#d-087), [D-109](#d-109), [D-111](#d-111).

## The host, the command line and the distribution

How Manifest starts, what it takes on the command line and how it ships. Detail: [host](host.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-070"></a>D-070 | One option contract | The host and the validator take one option contract: the content, then overlays in the order given, each mounted under its `mod.yaml` name, and a required scenario that names the game mode. See [host: the options](host.md#the-options). | Implemented (C06; `host`, `cli_*`, `validate_*`); its "nothing defaults" narrowed by [D-114](#d-114): the content defaults to the pack beside the executable; its overlays' mount names superseded by [D-124](#d-124): `mod:<identifier>`, as the shell mounts a mod |
| <a id="d-114"></a>D-114 | A standalone bundle | Manifest ships as a tarball for x86-64 Linux at or above a [glibc floor](glossary.md#glibc-floor), and through nix-ld on NixOS: the server, validator, token CLI and TUI, statically linked to the C++ runtime, beside the packed content. Writable data goes under the XDG directories, never beside the executable. A Nix closure is not a supported runtime. See [host: the distribution](host.md#the-distribution). | Implemented (R07; `distribution`, `bundle_smoke`, `scripts/test-bundle.sh`) |

Also followed here: [D-086](#d-086), [D-111](#d-111), [D-116](#d-116).

## The hosted world: state, commands and queries

The world a server steps, and how requests reach it. Detail: [world](world.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-002"></a>D-002 | Determinism | Every module builds with `-ffp-contract=off` (`cmake/ManifestModule.cmake`). Randomness is Jettison's PCG32 generator with a specified Fisher-Yates shuffle, state uses ordered maps and stable IDs, and replay and state-hash tests run in every preset. | Implemented (`sim`, `world`) |
| <a id="d-086"></a>D-086 | The step and one travel command | The hosted world steps at 1/60 simulation second, an [economic turn](glossary.md#economic-turn) every 3,600 steps ([the step, in order](architecture.md#the-step-in-order)). One `Travel(ship, venue)` command starts a saved journey that the host drives through Jettison's pad release, cruise and pad request. | Implemented (W02, W05; `world`, `travel`); its free travel and docking superseded by [D-097](#d-097) and [D-100](#d-100) |

Also followed here: [D-082](#d-082), [D-089](#d-089), [D-097](#d-097), [D-111](#d-111),
[D-113](#d-113).

## Companies, ships and cargo

What a player owns: a company, its ships and the goods they carry. Detail:
[companies](companies.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-044"></a>D-044 | The compact ship | A compact single-occupant ship, with a tiny long-journey cabin and 32 totes of cargo, and a matching smaller berth, is distinct from the starter. Jettison's pad classes and hull fit are definition data, so Manifest authors a berth with no engine edit. See the [ships plan](ships-plan.md#the-compact-ship). | Implemented for the hull and Jettison's pad-class data; Manifest's smaller berth and the ship's purpose open in the ships plan, no task yet |
| <a id="d-077"></a>D-077 | Starting credits | A player's trading company starts with about a hold of cheap goods' worth of credits in the starter (`company.startingCredits`), so a new player begins on cheap goods. | Implemented (A04; `company`) |
| <a id="d-078"></a>D-078 | Whole IBCs | Goods are held and moved in whole [IBCs](glossary.md#ibc) of one cubic metre. A hull's cargo volume is bounded by the largest hull class's envelope, and a warehouse holds at most a hundred of those of one item for one owner. | Superseded by [D-087](#d-087) for the count; the hold and warehouse caps stand, in totes (`kMaxHoldTotes`, `kMaxWarehouseTotes`) |
| <a id="d-087"></a>D-087 | Totes | Goods are counted in [totes](glossary.md#tote), an eighth of an IBC: holdings, holds, orders and limits count totes, so a trader buys a dear good in smaller amounts. A hull's `CargoVolume` and a venue's `stock` are authored in totes. See [companies: holdings and the hold](companies.md#holdings-and-the-hold). | Implemented (W03; `company`, `trade`, `content`); supersedes [D-078](#d-078)'s and [D-083](#d-083)'s IBC count |
| <a id="d-089"></a>D-089 | A start at a neutral spaceport | A new player is given a company, the starting credits and the starter ship at a neutral public spaceport (a scenario's `starterVenue`), and trades at once. No region or polity is allocated; they may found a homeworld later ([D-110](#d-110)). See [companies: provisioning a player](companies.md#provisioning-a-player). | Implemented (W16; `company`, `world`); supersedes [D-075](#d-075) |
| <a id="d-096"></a>D-096 | Upgrades as offers | The starter has a cargo-hold slot that takes the `CargoHolds` effect unchanged. An offer sells a tier priced beyond a new company's credits and within what one completed trade leaves. It is bought while the ship is docked, at any venue, and its credits leave the economy. A hull's mass is authored, and a hull with none is not spawned. See [companies: upgrades](companies.md#upgrades). | Implemented (W08; `company`, `content`, `data`) |
| <a id="d-104"></a>D-104 | Cases for high-value goods | Gold, the other precious metals, gems and luxury goods trade in a case of about a litre, each item naming its container. Some goods stay out of a new player's reach on purpose until a few runs pay for them. | Selected, open as [L32](delivery-order.md#l32) |
| <a id="d-123"></a>D-123 | A ship named once, renamed with approval | A player names each of their ships once, in the graphical client; the starting ship is named as the player founds their company (the user's, 2026-11-28). Renaming a named ship needs approval, an administrator's or an outside process's, not yet defined. A ship's name is what the screens show, not its ID. The user's, 2026-11-28. | Implemented for the starting ship (L54): `Company::Provision`'s ship name, `company`, `query`, `remote`, `console`, `client_model`, `client_screens`, `client_serve`; a later ship's naming and the renaming taken up with the client's names ([roadmap](roadmap.md#the-graphical-client)) |

Also followed here: [D-045](#d-045), [D-062](#d-062).

## Orders, brokers and market observations

The book, its brokers and what a trader knows of other markets. Detail: [trading](trading.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-021"></a>D-021 | Brokers and merchant income | Brokers trade real stock with their own funds. Their buy and sell spreads, with warehouse handling charges at both ends, are the merchant income. Production capacity caps output, and scarcity against demand sets prices. | Implemented for brokers ([D-084](#d-084)) and production caps; handling charges open, no task yet |
| <a id="d-043"></a>D-043 | What a trader knows | Live market data is for a fleet member docked at the venue. Elsewhere a trader has dated, remembered [observations](glossary.md#observation), with spawn-time reports of nearby markets. The same boundary holds for queries, native auto-traders and scripts. See [trading: market observations](trading.md#market-observations). | Implemented (W07; `trade`, `query`) |
| <a id="d-072"></a>D-072 | Consignment storage | A ship owner may unload goods into a market's warehouse under its own title, listed until bought or withdrawn, within space caps, for a storage fee. See the [economy plan](economy-and-development-plan.md). | Selected, open as [L16](delivery-order.md#l16) |
| <a id="d-073"></a>D-073 | Options and futures | Options and futures on a market's goods are a stretch goal, after consignment storage and forward contracts. | Selected, open as [L17](delivery-order.md#l17) |
| <a id="d-074"></a>D-074 | Eight market services | Markets offer freight contracts, warehouse receipts, paid price reports, forward contracts, loans against stored goods, cargo insurance, auctions, and escrow and letters of credit. Each one's rules are open until its task. | Selected, open as [L18](delivery-order.md#l18) to [L25](delivery-order.md#l25) |
| <a id="d-084"></a>D-084 | A broker at each venue | A company trades against one [broker](glossary.md#broker) at each venue: a principal owned by the venue's operator, with its own account and holdings, seeded from the scenario. Each economic turn it reposts one bid and one ask an item, bounded by its holdings and funds, with no mint. See [trading: brokers](trading.md#brokers). | Implemented (W03; `trade`); its fixed quote superseded by [D-102](#d-102) |
| <a id="d-085"></a>D-085 | Order life and bounds | A resting order lives one simulation day by default (`market.orderLifeSeconds`; 0 is no expiry). One matching pass settles at most `kMaxFills` fills, and one side of a venue's book for an item holds at most `kMaxRestingOrders`, so no remainder rests crossed. | Implemented (W03; `trade`) |
| <a id="d-095"></a>D-095 | Starting reports and connections | A new player starts with reports of every other station market of their system and of each directly connected system. A scenario's system names its direct connections (`connections`) until gates replace them. The Administrator has no reports; the console's `book` is the host's view. | Implemented (W07; `trade`, `query`, `content`) |

Also followed here: [D-083](#d-083), [D-087](#d-087), [D-102](#d-102).

## Prices

How a price is written, set and turned into credits. Detail: [pricing](pricing.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-001"></a>D-001 | Exact decimal prices | Market prices are an exact decimal ([Decimal price](glossary.md#decimal-price)); quantities, physics and effect stats stay floating point, and credit ledgers stay integers. See [pricing: the Decimal type](pricing.md#the-decimal-type). | Implemented (`types`, `market`, `sim`) |
| <a id="d-083"></a>D-083 | Prices per authored unit | An order is priced per the item's authored unit, and a fill's gross is the price times the units it moves, in whole credits ([from a price to credits](pricing.md#from-a-price-to-credits)). A venue takes a listing fee from each side (`market.listingFeeBps`). | Implemented (W03; `trade`); its IBC count superseded by [D-087](#d-087) |
| <a id="d-102"></a>D-102 | Supply and demand | A broker prices each good on a curve of its stock against a target and posts a [ladder](glossary.md#ladder), so a trade moves the price. Households bid higher as a need's [pressure](glossary.md#pressure) rises, and buy optional goods only when every need is met. Makers sell a surplus down to its cost, and governments tender by need. See [pricing](pricing.md#the-brokers-curve). | Implemented (W19, W12; `trade`, `policy`, `procurement`, `fixture`) |

Also followed here: [D-084](#d-084).

## The regional economy

Regions' production, households, payroll and condition. Detail: [economy](economy.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-018"></a>D-018 | Core and frontier | Inner worlds start at higher technology with depleted reserves; outer worlds are resource-rich and less developed. Frontier expansion adds systems at the map edge without moving established ones. See the [planetary plan](planetary-development-plan.md). | Selected, open as [L06](delivery-order.md#l06) and [L12](delivery-order.md#l12) |
| <a id="d-020"></a>D-020 | Credits, title and custody | Credits settle instantly between authorised accounts. Commodities follow the whole physical chain: extraction, local haul, station warehouse, ship, destination warehouse and local use. See the [economy plan](economy-and-development-plan.md). | Implemented for credits and venue holdings; the physical chain open in the economy plan, no task yet |
| <a id="d-022"></a>D-022 | Aggregate population | Population counts people in aggregate. Consumption is authored per 1,000 people per simulation day, scaled by population and the interval, with fractions kept. See [economy: households](economy.md#households). | Implemented (W04; `trade`) |
| <a id="d-023"></a>D-023 | The household basket | A region's households need staple food, met by any qualifying grain by mass, drinking water and basic medical supplies. Bread stays outside the basket, and seeds keep their IDs. | Implemented (W04; `trade`); the basket has since grown to five needs with legumes as staples (`balance.yaml`: `needs`), the grains by the tonne ([D-064](#d-064)) |
| <a id="d-024"></a>D-024 | Health and happiness | Each region has a [health](glossary.md#health) and [happiness](glossary.md#happiness) from 0 to 100. They survive changes of governor, fall on essential shortages and recover when every essential is met, happiness faster than health. See [economy: condition](economy.md#condition). | Implemented (W04; `trade`) |
| <a id="d-025"></a>D-025 | Shortage severity | Severity is the weighted average of the essentials' unmet fractions, each need's fulfilment capped, so a surplus of one cannot offset a missing other. | Implemented (W04; `trade`) |
| <a id="d-026"></a>D-026 | Condition slows work only | Lower condition slows the [work rate](glossary.md#work-rate) alone. A completed batch keeps its inputs, outputs and gross wages, and missing materials, funds or facilities can still stop work. | Implemented (W04; `trade`) |
| <a id="d-027"></a>D-027 | The economic turn's order | Production uses the previous step's condition and settles completed batches. At each due interval (`economy.intervalSeconds`) payroll and tax come before household buying and consumption, then condition updates. Regular pay subtracts prior settlement, so nothing is paid twice. | Implemented (W04; `trade`) |
| <a id="d-092"></a>D-092 | The employer and the households' buying | Each region has one native employer, with its own account and goods, that makes the region's market rows and sells them to the venue's broker through the book. Its batches are whole totes of about a day's output, its wage bill a share of their value (`production.wageShare`). Households buy whole totes at the brokers' asks, cheapest first. See [economy](economy.md#production). | Implemented (W04; `trade`) |
| <a id="d-099"></a>D-099 | Regional debt | Regions run on credit: households and treasuries may buy essentials beyond their funds, owing the seller or a lender interest up to the usury limit ([D-098](#d-098)). Merchant and banking classes control regions through that debt; what debt gives its holder and what default does are open. | Selected, open as [L30](delivery-order.md#l30) |
| <a id="d-103"></a>D-103 | Power, compute and the AI economy | The later ages run on power and compute: a regional power balance with a shortfall order, compute from server farms, and AI capability from AI labs, new capacity a public project. Its numbers are open. | Selected, open as [L31](delivery-order.md#l31) |
| <a id="d-105"></a>D-105 | Staples by kind of world | A region's staple supply follows its world: developed regions are near self-sufficient, undeveloped worlds start short and grow, ruined advanced worlds fall short, and resource worlds import their food. | Selected; the scenarios follow it, generation open as [L33](delivery-order.md#l33) |

Also followed here: [D-093](#d-093), [D-102](#d-102).

## Regions, governors and public projects

Who governs a region, founding, projects and tenders. Detail: [governance](governance.md); open
work: the [planetary plan](planetary-development-plan.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-014"></a>D-014 | Several regions on one planet | Several governed regions can share one planet, each with its own population, households, accounts, condition and projects. A player governs one region alongside a separate trading company, and AI governs the others. | Implemented for regions sharing a planet (`data/scenarios/governance.yaml`; `world`, `fixture_governance`); the starting region superseded by [D-089](#d-089) |
| <a id="d-015"></a>D-015 | Protection from acquisition | Player-governed regions cannot be bought by AI. A player may buy an eligible AI neighbour's public decision rights without a sibling governor's veto, leaving its treasury public and private owners intact. | Selected, open as [L03](delivery-order.md#l03) |
| <a id="d-016"></a>D-016 | Company-built spaceports | A corporation may build an expensive orbital spaceport for its own, an AI or an unclaimed region, one opportunity a region. Neighbouring ports do not block it, and other players' regions are protected. | Selected, open as [L05](delivery-order.md#l05) |
| <a id="d-028"></a>D-028 | What advances a planet | A planet's technology advances through resource exploitation, imports of what it lacks and built production facilities, not credits or knowledge alone. | Implemented for facilities and delivered inputs (W12, W13; `procurement`, `fixture_development`); exploitation open as [L06](delivery-order.md#l06) |
| <a id="d-029"></a>D-029 | Bronze metallurgy | The first technology: a seeded local workshop plus delivered project inputs gate timed work, and completion unlocks repeat production batches. Interruption pauses work, and [knowledge](glossary.md#knowledge) survives a facility's loss. See [governance: projects and the queue](governance.md#projects-and-the-queue). | Implemented (W12; `procurement`, `fixture_governance`) |
| <a id="d-030"></a>D-030 | The concession's clock | A frontier [concession](glossary.md#concession)'s exclusive term runs on real server-running time, paused while the server is down. It starts when the first eligible port is operational, is disclosed before construction, and cannot be renewed by transfer, rebuild or change of government. | Selected, open as [L06](delivery-order.md#l06) |
| <a id="d-040"></a>D-040 | Competitive bids, then corruption | Public contracts take competitive corporate bids, the governor's own company among them. A corruption trait and abstract bribery may sway eligible rankings only after clean tendering works. | Implemented for clean tendering (W12; `procurement`); corruption open as [L02](delivery-order.md#l02) |
| <a id="d-041"></a>D-041 | Paid as delivered; bookings honoured | A supplier is paid for each accepted partial delivery as it arrives. A station owner honours accepted bookings: changed fees, priorities or exclusions apply to later requests only. | Implemented for payment (W12; `procurement`) and fees (W18); bookings open as [L04](delivery-order.md#l04) |
| <a id="d-042"></a>D-042 | Performance security | A tender award needs a supplier-funded [performance security](glossary.md#performance-security), held until close and forfeited in proportion to what goes undelivered. Its rounding is [D-101](#d-101)'s. | Implemented (W12; `procurement`) |
| <a id="d-075"></a>D-075 | A home region at provisioning | A player's home region was the first ungoverned `playerHome` region in the world, governed by a new polity for that player. | Superseded by [D-089](#d-089); retired in W16 |
| <a id="d-076"></a>D-076 | Region supply | A server launches with a configurable number of generated regions. As players join it keeps the share open to founding within a range by generating new ones, without moving established regions, so early players hold the centre. | Selected, open as [L42](delivery-order.md#l42) |
| <a id="d-101"></a>D-101 | The tender lifecycle | Every project input is tendered and accepted from a supplier's ship docked at the region's venue. The issuer sets bid and delivery windows within authored bounds (`procurement`). Payment is cumulative to the nearest credit; the bond rounds up and its forfeit down. An AI issuer retenders at once after a default, and a standing policy can bid and deliver. See [governance: a tender's life](governance.md#a-tenders-life). | Implemented (W12; `world`, `procurement`, `policy`, `fixture`); its budget superseded by [D-102](#d-102) |
| <a id="d-110"></a>D-110 | Founding and the development queue | A player founds one homeworld in a region no one governs, paying the founding price (`company.foundingPrice`) into its treasury; a new polity they direct governs it. A region's projects run in queue order, the head alone working; a governor's policy tenders its inputs within a limit. See [governance: founding a homeworld](governance.md#founding-a-homeworld). | Implemented (W13; `world`, `procurement`, `query`, `fixture_development`) |

Also followed here: [D-089](#d-089), [D-102](#d-102).

## Space: star systems, lanes, gates and cruising

Where the venues are and how ships cross between them. Detail: [space](space.md); open work: the
[space plan](space-plan.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-017"></a>D-017 | Stargate expeditions | Opening a new system takes a funded, automated, unmanned FTL stargate delivery, its duration by distance over server-running time. The working gate's owner sets access and fees. | Selected, open as [L09](delivery-order.md#l09) (with [L08](delivery-order.md#l08) to [L12](delivery-order.md#l12)) |
| <a id="d-088"></a>D-088 | Systems and drives as records | A scenario's star system is a `Space.StarSystem` record, its planets and venues bodies on orbits, a venue a station built to the layout it names. Each hull has a `Space.CruiseDrive` record, its cruise time a power law of distance. See [space: star systems and the galaxy](space.md#star-systems-and-the-galaxy). | Implemented (W05; `space`, `travel`, `content`); kept by Astralis, superseded in the Manifest mode by [D-090](#d-090) |
| <a id="d-091"></a>D-091 | Moongates | Moongates, for a fee, act as stargates do but connect only the places round one planet or moon, within one star system. | Selected, open as [L35](delivery-order.md#l35) |

## Travel and fuel

A journey, its cruise and the fuel it burns. Detail: [travel](travel.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-090"></a>D-090 | Cruises limited by acceleration | In the Manifest mode a cruise is limited by acceleration, turning at the midpoint: a manned ship holds a hard burn for a few hours of the trip and less between, an unmanned one more. Local trips take minutes to hours, a manned cruise between worlds days, and gates are the fast ways. Astralis keeps [D-088](#d-088)'s cruise. See [space: cruising](space.md#cruising). | Implemented (W17; `travel`, `space`, `fixture_cruise`) |
| <a id="d-107"></a>D-107 | The burn is a choice | A ship's drive burns fuel to accelerate, and how hard is a choice in planning a route: more fuel for a faster run, or less for a cheaper one. The burn is a share of the cruise's time thrusting and an acceleration in g; a standing policy picks the most profitable an hour. See [travel: the burn](travel.md#the-burn). | Implemented (W21; `travel`, `company`, `policy`, `fixture_burn`) |
| <a id="d-108"></a>D-108 | The drives, circa 2176 | Small hulls fly a clean fusion [torch](glossary.md#torch), burning [reaction mass](glossary.md#reaction-mass) and dear [fusion pellets](glossary.md#fusion-pellets), so a hard burn must pay for itself. Larger hulls fly dirtier drives, deuterium fusion and nuclear pulse, kept clear of stations and populated bodies. See the [ships plan](ships-plan.md#drives-past-the-torch). | Implemented for the torch (W21; `travel`, `company`, `fixture_burn`) and the hauler fusion drive, its exclusion radius crossed on thrusters (W22a; `space`, `data`, `fixture`); and the pulse drive, its radius about populated bodies, licence and override (W22b; `space`, `travel`, `fixture`). The drives' numbers were an agent's proposal, accepted by the user on 2026-11-26 to tune later |
| <a id="d-117"></a>D-117 | The pulse drive under licence | The pulse drive burns pulse units, sold only at polity-operated venues to a company that polity licenses: an AI polity for a fee, a player's at its director's word. Its first hull, the pulse carrier, docks only at a dry dock; a scenario grants the hulls it chooses, at venues it chooses; deuterium is sold wherever reaction mass is; and a pulse ship's override of a radius is fined, harms the regions about it and loses it the AI polities' licences. See the [ships plan](ships-plan.md#w22b-the-design). | Implemented by [W22b](history/tasks.md#w22b) (`space`, `travel`, `content`, `fixture`); the agent's proposals, accepted by the user on 2026-11-27 to tune later |

Also followed here: [D-086](#d-086), [D-088](#d-088).

## Stations: berths, life support and charges

What a station gives a docked ship and what it charges. Detail: [stations](stations.md); open work:
the [ships plan](ships-plan.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-045"></a>D-045 | The starter's life support | The lone-trader starter carries operating oxygen and an installed scrubber cartridge in life-support space outside the trade hold. Oxygen and scrubbing are separate limits, used per occupant per simulation day (`lifeSupport`). | Implemented (C04, W06; `company`, `data`) |
| <a id="d-046"></a>D-046 | Docked ships pause their own use | A ship connected to a station's working [life support](glossary.md#life-support) pauses its own oxygen and scrubber use, keeping what it has. Leaving resumes it, and docking refills nothing. See [stations: station life support](stations.md#station-life-support). | Implemented (W06; `company`, `travel`) |
| <a id="d-047"></a>D-047 | Starting regions with a station | Starting regions were to be allocated only with a working public station, a delivery connection and a free life-support connection, new players spawning connected. | Superseded by [D-089](#d-089); the connected spawn kept by [D-094](#d-094) |
| <a id="d-048"></a>D-048 | Basic access stays | Basic docking, trading and regional delivery stay available despite corporate restrictions, with their charges, finite capacity and private-port policies. | Selected, kept by [L04](delivery-order.md#l04) and [L05](delivery-order.md#l05) (no restriction exists yet) |
| <a id="d-050"></a>D-050 | Modular oxygen banks | A ship's oxygen is a bank of tanks, each refilled or replaced on its own, so the rest keep supply; larger hulls get proportionally more. See the [ships plan](ships-plan.md#modular-oxygen-banks). | Implemented in part: the starter's tanks (`World::LifeSupport`, `data/ships/hulls.yaml`; `company`); tank exchange and other hulls' banks open as [L07](delivery-order.md#l07) |
| <a id="d-058"></a>D-058 | Eleven pad classes | [Pad classes](glossary.md#pad-class) are eleven ship sizes, shuttle, skiff, cutter, corvette, frigate, destroyer, cruiser, battlecruiser, battleship, dreadnought and titan, named after military classes but sizes for any ship: civilian and merchant types have hulls of their own on the smallest class they fit (the user's, 2026-11-27, replacing the seven of 2026-11-18). The classes above destroyer dock only at dry docks, and a titan-class ship, a titan or an ark, at none: it docks with no other ship, but flies to a coordinate and parks, and the dry dock docks with it (the user's, 2026-11-27). | Implemented, every class with its fees and a hull type of its name, and nine civilian types (`data/docks/pads/`, `data/ships/hulls.yaml`; `content`); the dry dock and its yards built by [W22b](history/tasks.md#w22b) |
| <a id="d-094"></a>D-094 | Connections and resupply | A ship connects to its venue's life support as it docks, while a connection is free, and lets go as it leaves; a new player spawns connected. Oxygen and cartridges come from the ship's own hold, bought through the book. An exhausted supply has no consequence yet. See [stations: resupply](stations.md#resupply-refill-and-replace). | Implemented (W06; `company`, `world`); exhaustion's consequence open as [L45](delivery-order.md#l45) |
| <a id="d-098"></a>D-098 | Owners set their charges | Owners set the charges on what they hold: an operator its docking and berth fees, a warehouse owner its storage fee, a gate's owner its toll, a governor its region's taxes. A change applies to later charges only. Unpaid charges are arrears at the creditor's rate, up to the usury limit (`charges.maxArrearsRateBps`). | Implemented for station fees (W18; `company`, `travel`) and a region's wage tax (L29; `trade`, `query`, `console`); storage fees and tolls open as [L16](delivery-order.md#l16) and [L10](delivery-order.md#l10) |
| <a id="d-100"></a>D-100 | Fees by fee class | A ship pays by its [fee class](glossary.md#fee-class): a docking fee as its journey starts and a berth fee each hour docked, plus a share for each connected occupant (`charges`). An AI operator moves its fees with occupancy once a day. [Arrears](glossary.md#arrears) hold the ship and, after a grace, its cargo is sold. See [stations: fees by fee class](stations.md#fees-by-fee-class). | Implemented (W18; `company`, `travel`, `policy`, `fixture_traffic`) |
| <a id="d-106"></a>D-106 | Algae oxygen recycling | Algae recycling is a life-support upgrade bought in tiers, each supplying a share of the ship's daily oxygen and taking up carbon dioxide, so its tanks and cartridge last longer; it never replaces them. | Selected, open as [L34](delivery-order.md#l34) |
| <a id="d-120"></a>D-120 | A region's tax, bounded and steered | A region's wage tax is its one tax for now; other bases are rows of their own when wanted. A rate is held between bounds in the tuning, and an AI governor steers its own towards its budget: up when its treasury falls short of its commitments, down on a surplus, by a bounded step a day within the bounds. The user's, 2026-11-27. Built with the bounds 0 to 5,000 basis points, a step of 100 a day, and as an AI governor's commitments a reserve of 20 credits a head, its surplus over five times that (L29's best guesses, 2026-11-28). | Implemented (L29; `trade`); see [the regional economy](economy.md#a-regions-wage-tax) |

Also followed here: [D-097](#d-097).

## Standing policies and NPC traders

Ships that trade on their own. Detail: [policies](policies.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-097"></a>D-097 | Standing trade policies | A native system runs each ship's saved [job](glossary.md#job) (choose, travel, buy, travel, sell) through the players' own commands, for players' and NPCs' ships alike. A [lot](glossary.md#lot)'s cost is what its fills paid, shared by its partial sales, with one [loss allowance](glossary.md#loss-allowance). An edit applies at the next decision. An unknown market is skipped, not scouted. See [policies: the job's phases](policies.md#the-jobs-phases). | Implemented (W10; `policy`, `fixture`); its nominal fees superseded by [D-100](#d-100); scouting open as [L46](delivery-order.md#l46) |

Also followed here: [D-004](#d-004), [D-100](#d-100), [D-101](#d-101), [D-107](#d-107).

## Saves, the journal and backups

What a server writes, and what a crash may lose. Detail: [persistence](persistence.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-111"></a>D-111 | A save of its own build | A world loads only in a build of its own identity: the world's `identity` file's game and SDK trees, fixed step, command versions and content manifest must equal the running build's, checked before anything is recovered. Until player testing ([D-109](#d-109)) a world does not outlive its build. See [persistence: the identity](persistence.md#the-identity). | Implemented (R03, R04; `host`) |
| <a id="d-112"></a>D-112 | A batch before every step | Each journal batch was committed before its steps ran, a receipt sent only after the commit. | Superseded by [D-113](#d-113) the day it was made: it wrote 16 GB a day whatever the load |
| <a id="d-113"></a>D-113 | A crash may lose a flush interval | A server steps and answers at once, writing the steps and requests since the last flush as one batch every flush interval (`--flush-seconds`) and the world's snapshot every snapshot interval (`--snapshot-seconds`). A crash may lose up to a flush interval; a clean stop loses nothing. A receipt is not a durable acknowledgement. See [persistence: what a crash loses](persistence.md#what-a-crash-loses). | Implemented (R03, R04; `host`, `cli_serve`); supersedes [D-112](#d-112) |
| <a id="d-115"></a>D-115 | Saves off the step's thread | A server's saves leave the thread that steps its world, so no flush, snapshot or backup holds a step or a frame. The catalogue is published only once what it names is on disk. A reusable writer is Jettison's. | Selected, open as [L40](delivery-order.md#l40) |

Also followed here: [D-114](#d-114).

## The dedicated server and remote clients

A server with no players, and the clients that connect to it. Detail: [server](server.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-011"></a>D-011 | A server with no players | A command-line dedicated server advances one authoritative world with no one connected: the economy, NPCs, autopilot and auto-trading run without clients. Downtime pauses the world. | Implemented (N02; `remote`, `host`, `cli_serve`) |
| <a id="d-057"></a>D-057 | Networking split | Jettison provides networking common to many games; Manifest owns its dedicated server and every game-specific mechanic, identity mapping, authorisation and visibility rule built on it. See Jettison's [networking](../../jettison/docs/networking.md). | Implemented (N01, N02; `remote`) |
| <a id="d-079"></a>D-079 | UDP through ENet | The transport is UDP through ENet in Jettison: reliable-ordered channels for the handshake and requests, unreliable-sequenced ones for snapshots, so one stack serves the idle game and real-time variants. | Implemented (N02; `remote`) |
| <a id="d-080"></a>D-080 | Bearer tokens | A client authenticates with a bearer [token](glossary.md#token) the server's CLI issues, one per principal, stored as a digest. An administrator token is a separate kind, so identity, game authorisation and admin rights stay distinct. See [server: tokens and revocation](server.md#tokens-and-revocation). | Implemented (N02; `remote`, `host`) |
| <a id="d-081"></a>D-081 | Every connection sealed to the server's key | Every message is sealed to keys made with the server's key (Jettison's [J01](../../jettison/docs/history/neutral-sdk.md#j01-encryption)), so a server may be hosted outside a trusted network. The server keeps the secret half in a file its owner alone may read (`manifest-token key`); each client pins the public half from its server list, which a build ships and a player's own replaces, by `host:port`. TLS is not used over UDP. See [server: connections are sealed](server.md#connections-are-sealed). | Implemented (2026-11-26; `remote`, `console`, `cli_serve`); a server key that signs working keys, for changing keys without re-pinning, not provisioned |
| <a id="d-082"></a>D-082 | Three sessions an actor | An actor may hold three [sessions](glossary.md#session) at once, each sent its own replies and the actor's snapshot. They share the actor's sequences, so a clash is `RequestConflict` and never runs twice; a fourth is closed as `ServerFull`. | Implemented (N03; `remote`) |
| <a id="d-122"></a>D-122 | A player founds their own company, once | Every player starts with a company they name. On a server a principal's token may send `provision` for its own principal alone, refused once it has a company; the principal is the token's, never the payload's, and an administrator still provisions anyone. The client names the company on its start screen, and from the console as the game starts. The user's, 2026-11-28. | Implemented (L52): `Command::Newcomers`, `company`, `remote`, `client_model`, `client_serve` |

## The console and the terminal UI

The development console and the TUI that plays on a server. Detail: [console](console.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-012"></a>D-012 | The TUI as a remote client | The TUI is a gameplay client of the server, with `connect` and `disconnect`. Manifest's graphical client shares the same command and query layer, and administrative permission stays apart from gameplay. | Implemented for the TUI and administrator tokens (N02; `console`, `remote`); the graphical client's model sends the same commands on a local world or a server's ([server play](client.md#server-play), L52; `client_model`, `client_serve`) |
| <a id="d-121"></a>D-121 | The terminal UI stays English | The terminal UI, mainly for administration, prints English outside the string tables; only the graphical client's text is the players' and in all four languages ([L43](history/tasks.md)). Translating it was rejected. The user's, 2026-11-27. | Implemented: `client_strings` covers the graphical client alone |
| <a id="d-013"></a>D-013 | Manual trading a stretch goal | Manual spaceport trading through the TUI is a stretch goal; configuring and observing auto-trading from it is required. | Implemented for manual trading (the console's `trade`; `console`); auto-trading set from the TUI by `policy` ([L44](history/tasks.md), done 2026-11-27) |
| <a id="d-125"></a>D-125 | The TUI resends on the ledger's sequences | The console keeps each request it sent until it hears it complete or refused, dials again after a lost connection with the same token, and sends what it kept under the sequences it was first sent with, relying on the server's ledger to answer a sequence it admitted as that request, so nothing runs twice. No second deduplication was added. A refusal is not resent, and a refusal of the connection itself is not dialled again. | Implemented ([L41](history/tasks.md#l41); `console`) |

Also followed here: [D-095](#d-095), [D-107](#d-107), [D-114](#d-114).

## The development sandbox

No decision is filed here: the sandbox follows the ones listed below. Detail: [sandbox](sandbox.md).

Followed here: [D-001](#d-001), [D-002](#d-002), [D-061](#d-061).

## Art standards

How models, textures and materials are authored. Detail: [art standards](art-standards.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-118"></a>D-118 | Manifest defines the art standards | Manifest's art standards set the files, names, scale, geometry, materials and texture coordinate sets for every asset; Jettison supports the workflow and its test content follows them. | Selected, no task yet; Jettison's side is its J64 to J67 |
| <a id="d-119"></a>D-119 | The art repository on Git LFS | The artists' work and source trees are a Git repository of their own, outside Manifest's, with binary files in Git LFS. | Selected, no task yet |

Also followed here: Jettison's [texture coordinate sets by role and the material
library](../../jettison/docs/decisions.md#rendering).

## Build and process

The SDK pin, the gate, CI and how work is done. Detail: [build and gates](build-and-gates.md) and
[agent workflow](agent-workflow.md).

| ID | Decision | Statement | Status |
| --- | --- | --- | --- |
| <a id="d-006"></a>D-006 | Environment parity | The pinned SDK revision, `devenv.nix`, `devenv.yaml` and `devenv.lock` stay identical to the engine checkout's; shared dependency targets are reused, never refetched. | Implemented (`scripts/check-env-parity.sh`, in CI; [D-054](#d-054)) |
| <a id="d-007"></a>D-007 | Gate evidence | Gate evidence is staged and named: a recorded gate lists the stages and presets that ran, and a partial run is never reported as a gate. clang-tidy is opt-in locally and runs at every gate before a push. | Implemented (`scripts/ci.sh`); its tidy in CI superseded by [D-116](#d-116) |
| <a id="d-051"></a>D-051 | Machine-specific concurrency | Build concurrency is the machine's: `scripts/ci.sh` defaults to two jobs, and the environment's `CMAKE_BUILD_PARALLEL_LEVEL` and `CTEST_PARALLEL_LEVEL` override it. See [build and gates](build-and-gates.md#machine-specific-concurrency). | Implemented (`scripts/ci.sh`) |
| <a id="d-052"></a>D-052 | Tidy after the builds | The full gate completes its build and test presets before strict clang-tidy; tidy stays fatal, with no warning baseline. | Implemented (M01; `scripts/ci.sh`) |
| <a id="d-053"></a>D-053 | Promotion to main | `main` receives only exact-commit, verified fast-forward promotions from `feature-base`, with no second gate. See [build and gates: promoting to main](build-and-gates.md#promoting-to-main). | Implemented (M02; `scripts/promote.sh`, `.github/workflows/ci.yml`) |
| <a id="d-054"></a>D-054 | No desktop tools in the SDK shell | Desktop-only tools stay out of the shared SDK shell; Manifest's pin and its identical environment files change together, parity checked against the pin in CI. | Implemented (M04; `scripts/check-env-parity.sh`, `scripts/test-env-parity.sh`); keeps [D-006](#d-006) |
| <a id="d-055"></a>D-055 | Editor examples | The tracked editor examples use explicit Debug presets, real target paths, the development shell's tools and the host's concurrency; a debugger launch is verified directly. | Implemented (M05; `.vscode/*.example`) |
| <a id="d-056"></a>D-056 | Stubs at discretion | An implementer may write stubs where useful; unfinished behaviour stays explicit and does not meet a dependent task's acceptance. See [agent workflow](agent-workflow.md#implementer-discretion-to-use-stubs). | Implemented (a working rule) |
| <a id="d-109"></a>D-109 | Formats change in place | Until player testing, a change to a save, definition, scenario or other versioned format rewrites every file of it, in Manifest and Jettison, in the same commit, with no version bump or migration. Versions are still checked, and a field whose meaning changes takes a new name. Jettison holds the same [decision](../../jettison/docs/decisions.md). | Implemented: the suites load every file |
| <a id="d-116"></a>D-116 | CI is not the gate | CI checks the environment's parity, stages the distribution's bundle and smoke-tests it on a clean Ubuntu runner, outside the development shell. Builds, suites, sanitisers and tidy are the local gate's, before every push. See [build and gates: CI](build-and-gates.md#ci). | Implemented (`.github/workflows/ci.yml`); supersedes [D-007](#d-007)'s tidy in CI |

## Unused IDs

D-008, D-009, D-019 and D-031 to D-039 were never assigned. They stay unused: an ID is never
reused.

## Numeric values

The ledger states no tuning numbers. They live in the data, which the `content` and `data` suites
hold to their ranges; the feature documents explain them.

| Values | Where | Explained in |
| --- | --- | --- |
| Economic interval; the wage tax's starting rate, bounds and the AI governor's step and budget | `data/tuning/balance.yaml`: `economy` | [Economy: a region's wage tax](economy.md#a-regions-wage-tax) |
| Household needs, weights and rates per 1,000 people a day | `balance.yaml`: `needs`, `optionalGoods` | [Economy: households](economy.md#households) |
| Health and happiness rates and the work-rate floor | `balance.yaml`: `condition` | [Economy: condition](economy.md#condition) |
| Production batches and the wage share | `balance.yaml`: `production` | [Economy: production](economy.md#production) |
| Starting credits and the founding price | `balance.yaml`: `company` | [Companies: provisioning a player](companies.md#provisioning-a-player), [governance](governance.md#founding-a-homeworld) |
| Onboard oxygen and scrubber use | `balance.yaml`: `lifeSupport` | [Stations: station life support](stations.md#station-life-support) |
| A torch's fuel and the policy's burns | `balance.yaml`: `fuel` | [Travel: the fuel a leg burns](travel.md#the-fuel-a-leg-burns) |
| Listing fee, order life and sales window | `balance.yaml`: `market` | [Trading](trading.md) |
| Brokers' treasury and spread | `balance.yaml`: `brokers` | [Trading: brokers](trading.md#brokers) |
| Station fees, arrears, seizure and the AI operator's pricing | `balance.yaml`: `charges` | [Stations: fees by fee class](stations.md#fees-by-fee-class) |
| Frontier concession term | `balance.yaml`: `concessions` | [Planetary plan](planetary-development-plan.md) |
| Performance security and tender windows | `balance.yaml`: `procurement` | [Governance: a tender's life](governance.md#a-tenders-life) |
| Project inputs, work and recipes | `balance.yaml`: `projects`, `recipes` | [Governance: projects and the queue](governance.md#projects-and-the-queue) |
| Hulls' cargo, occupants, oxygen tanks, cartridges and mass | `data/ships/hulls.yaml` | [Companies: hulls](companies.md#hulls) |
| Item base prices, densities and units | `data/trade-items/*.yaml` | [Content: units and storage](content.md#units-and-storage) |
| Offers' costs and tiers | `data/offers/*.yaml` | [Companies: upgrades](companies.md#upgrades) |
| The fixtures' regions, venues, stocks and rates | `data/scenarios/trading.yaml`, `governance.yaml` | [Content: scenarios](content.md#scenarios) |
| The sandbox marketplace's fees, insurance and bounds | `MarketplaceOptions` in `source/game/market/include/manifest/market/Marketplace.h` | [Sandbox](sandbox.md) |

## Rules for maintaining this ledger

1. A decision is stated once, here, in a sentence or two; its detail is in its feature document.
   Plans, the roadmap, `scope.md` and the feature documents link the ID instead of restating the
   rule.
2. Numbers are never restated here. They live in the data, and the table above names the keys.
3. A new decision takes the next free ID, in the group of the feature it shapes, with an anchor. IDs
   are never reused or renumbered.
4. A changed decision is a new row. The old row's status becomes "Superseded by" the new one, which
   says what it supersedes. Both change in the same commit as the reasoning, recorded in the
   feature document or plan.
5. "Implemented" names the task that built it and a verifying suite. Documentation alone never makes
   a decision implemented.
6. "Selected" names the open task in the [delivery order](delivery-order.md) that will build it. A
   selected decision with no task is not scheduled.
