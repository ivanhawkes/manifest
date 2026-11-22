# Roadmap

The [next development stages](development-stages.md) retain the foundational host, content,
ownership, trading and recovery plans. The subsequent networked idle-game and planetary
development goals below extend that baseline. Networking is a required gameplay boundary;
the old optional administration-only TUI direction is superseded. The delivery amendments
are planning proposals; what is built is recorded in the [delivery order](delivery-order.md).

Use the [prioritised delivery order](delivery-order.md) for the task queue, explicit
prerequisites, stage exit gates and deferred-work entry conditions. It incorporates
the maintenance tasks and required network/regional amendments without renumbering
the five foundational stages.

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
gaps in Jettison, which owns the low-level networking common to many games while Manifest owns
the dedicated server and its game rules ([D-057](decision-ledger.md#d-057)). Current snapshots
and render smoke tests do not establish network replication
or a true CPU-only server build. Transport, authentication and encoding are selected in the
[network contract](gameplay-and-server-plan.md#network-contract-n01-2026-11-20); hosting and
measured capacity remain open.

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
Initial corporate spaceports have one construction opportunity per eligible region: a company's
own, eligible AI or eligible unclaimed region. Neighbouring ports on the same planet do not
block construction; other players' governed regions are protected against unsolicited builds.
Frontier concession expiry enables competing ports as defined below.
They are very expensive and enable profits through resource exploitation and colonies with
company-controlled puppet governments.
Purchasing regional control is not required before building for an otherwise eligible AI region.
The station must become a physical source of commercial leverage; ownership alone does not
make it a mandatory gate for every ship in the system. See the [impact plan](planetary-development-plan.md).

New resource concessions cover eligible resources in one explicitly named frontier region.
Neighbours remain independently available under their own eligibility rules, with separate
timers; government control remains separate. Shared ports may serve several regions on the
same planet under owner terms and protected bookings, with explicit physical delivery and
separate economies/rights. Starting regions require continuing basic public-port access;
detailed transport and starter fees/capacity/funds remain open.
Concessions give an initial exclusive period measured in real elapsed server-running time;
shutdown time does not count. Afterwards rivals may exploit
the covered regional resources, even under the incumbent's puppet government. Competing ports
are allowed at regional concession expiry despite that government.
Incumbents retain infrastructure/capital advantages and may try to exhaust
resources before rivals arrive. The selected grant starts when the first eligible frontier
spaceport becomes operational and its exploitation concession activates. Publish its duration
before construction; the initial balance-testing term is the [economy plan: exclusive exploitation](economy-and-development-plan.md#real-time-exclusive-exploitation)'s. Idle
facilities and disconnected owners do not pause it; ownership transfers, rebuilding and
government changes cannot restart or extend it. The concession does not transfer government
control. Validate the term against construction costs, extraction capacity and server pacing.

## Physical economy and material-gated development

The selected first [tech example](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)
is Bronze metallurgy: basic smelting knowledge, an operational seeded workshop, local copper
and physically imported tin. Consume the copper and tin inputs once for the timed public
project; completion unlocks repeat batches of copper and tin into bronze ([economy plan: bronze](economy-and-development-plan.md#first-tech-example-bronze-metallurgy-selected)).
Facility interruption pauses project work, and completed knowledge survives facility loss;
production requires an operational workshop. These are initial test quantities for the
separate governance scenario; duration, throughput and profitability still need balancing.

Credits transfer instantly between authorised entity accounts. Commodities follow the full
chain: extraction/industry, local haul, station warehouse, ship, destination warehouse and
local use. Brokers trade real stock using their own funds; their buy/sell spreads and separate
warehouse handling charges at both ends provide the intended substantial merchant income.
A market will also sell on [consignment](economy-and-development-plan.md#consignment-storage-selected-2026-11-19):
a ship unloads into its warehouse and leaves the goods listed, within space caps and at a fee
by space taken. Eight further [market services](economy-and-development-plan.md#further-market-services-selected-2026-11-19)
follow it, freight contracts first, and options and futures on its goods are the stretch goal
after them ([delivery order](delivery-order.md), L16–L25).

Production/extraction capacity caps new output; actual unreserved inventories cap trade.
Scarcity and demand determine prices, rather than production-based price bands. Auto-trading
must account for both broker prices, both warehouses and physical operating/access costs.
Planetary tech advancement requires resource exploitation, imports of unavailable resources
and actual production facilities, not just credits or knowledge prerequisites.

The first household basket has three equally weighted needs: staple food, drinking water
and basic medical supplies. Add rice, wheat, oats and barley as distinct tradable food
commodities, preserving existing seed items. Only those four grains satisfy the single food
need; bread remains tradable outside the essential basket. Food grains trade in kilograms,
with 1 kg of any grain meeting 1 kg of food demand, including mixtures. Retain distinct
stocks/prices/storage and consume actual grains, without a synthetic food commodity.
Regional population counts people in aggregate; author demand per 1,000 people per simulation
day, scaling fractional quantities by population and interval duration. Retain one household
pool per region. Any supported migration needs an explicit conversion from legacy abstract
world units. The initial staple-food and drinking-water demand rates are the [economy plan: households](economy-and-development-plan.md#aggregate-households-wages-and-consumer-demand-selected)'s;
this balance-test requirement retains physical supply/funding limits and shortage recording,
and drinking water is separate from agricultural/industrial demand. Preserve
fractional interval quantities and physical volume through inventory-unit conversions.
Remaining commodity definitions and population sizes still need selection/implementation.

Basic Medical Supplies is selected as a distinct tradable kilogram-based commodity for
routine medical consumables. Antibiotics, anaesthetics and analgesics remain separate
commodities without automatic substitution. Basic-supplies demand is prorated across updates at the
economy plan's rate.
All three first-basket consumption rates are selected; remaining authored item values
are open. The economy plan records the bounded implementer handoff.

Track aggregate population health and happiness in simulation, each on a continuous 0–100
scale (100 ideal, 0 lowest modelled condition), starting both at 100 in the first balance-testing
scenario. Existing values persist on joining, acquisition or governor changes. After household
purchases at each economic cadence ([economy plan: health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected)), essential shortages lower both values with greater losses
for larger shortages; full essentials permit gradual recovery. Happiness declines/recovers
faster than health, with both bounded to 0–100. Partial losses are proportional to severity,
and recovery happens at full essentials, at the initial test rates of the [economy plan: health and happiness](economy-and-development-plan.md#population-health-happiness-and-production-selected). Severity averages required essentials' unmet
fractions with authored importance weights, initially equal; cap fulfilment at 100% so surplus
cannot offset another missing essential. Both values slow production work rate while completed
batches retain declared inputs/outputs/gross wages. Slower work slows proportional wage accrual;
changes affect subsequent steps without reversing progress or earned/paid wages. The initial
work-rate multiplier is the economy plan's. Missing materials/funds or inactive facilities can still stop work entirely.
Production uses prior-step condition and settles completion; on each due economic boundary,
regular payroll/tax precedes household purchases/consumption, then condition updates for the
next fixed step. Full movement/market integration ordering and condition balance need validation.
The [market-information plan](market-information-plan.md) selects
live station data only with a fleet member docked there, dated remembered reports elsewhere,
and spawn-time snapshots for every station market in directly connected systems. Fleet
coverage must constrain UI queries, native trading decisions and script facts as well as
provide the intended competitive advantage; global real-time trader knowledge is superseded.

Add the [compact single-occupant ship and matching berth](small-trader-and-docking-plan.md):
tiny truck-like living accommodation for long journeys and a very small cargo hold
([D-044](decision-ledger.md)). Manifest owns the class and balance; the pinned engine needs a reusable smaller-pad
capability. This ship's primary role is unspecified and trade is secondary. The separate
lone-trader entry ship starts with a larger hold (capacities: the [small-ship plan](small-trader-and-docking-plan.md#selected-ship-roles-and-cargo-capacities)). Dimensions, endurance, costs,
starter upgrades and each hull's berth fit remain design work.
Its selected initial supplies are oxygen and one fresh installed standard scrubber
cartridge ([D-045](decision-ledger.md); amounts and endurance: the [small-ship plan](small-trader-and-docking-plan.md#selected-ship-roles-and-cargo-capacities)).
Provision once and preserve remaining supplies through reconnect/recovery/transfers;
operating reserves use dedicated life-support space outside the trade hold. Trade
supplies and loose spares use cargo volume. The starter's operating oxygen maximum is
its initial supply, so it begins full; cargo oxygen cannot bypass this cap. Compartment dimensions,
tank physical volume, transfer procedures, full journey endurance and compact limits/
reserves remain open.

Add oxygen and carbon scrubbers to the planned item list under a Life Support category.
The [exploration content follow-up](stage-2-content-plan.md#life-support-and-space-exploration-content-follow-up)
tracks the related space-exploration requirements. Carbon scrubbers are replaceable
consumable cartridges with finite capacity, used by installed machinery that stays aboard.
Oxygen is a separate consumable measured and traded in kilograms, required per
occupant per simulation day while relying on onboard life support (rate: the [life-support follow-up](stage-2-content-plan.md#life-support-and-space-exploration-content-follow-up));
retain fractional use. Its reserves and remaining scrubbing
capacity are distinct supply constraints. Scrubbing capacity is measured in occupant-days:
one supports one person for one simulation day, with fractional usage carried forward.
A fresh standard cartridge's rating is the [life-support follow-up](stage-2-content-plan.md#life-support-and-space-exploration-content-follow-up)'s. Cartridge mass/volume/price, tank volume/storage capacity,
cartridge trade units, installed/spare capacity, endurance,
replacement/resupply, spent-cartridge handling and exhaustion consequences remain open.

The subsequent [D-050](decision-ledger.md#d-050) refinement has one canonical tank-bank
specification and is deferred under its recorded re-entry condition.

Docked ships connected to working station life support pause onboard oxygen and scrubber
consumption, retaining actual remaining supplies. Departure or service loss resumes
applicable onboard use. Oxygen refills and cartridge replacements are separate resupply
operations; docking/connection cannot reset supplies. At stations offering connected life
support, ongoing service is included in the berth fee, with refills/cartridges priced
separately and accepted terms protected. Numeric tariffs/billing, availability, capacity,
connection procedures and station resource accounting remain open.

Add LPG as a distinct tradable commodity under Fuels, using kilograms for inventory and
trading. The stage 2 content plan records the addition; prices, availability, physical
storage/packing, production and consumption uses remain open. A ship's drive burns its own
reaction mass and fusion pellets, not LPG ([D-108](decision-ledger.md#d-108), W21).

Inner map worlds start at higher civilisation technology with depleted natural reserves;
outer worlds are generally resource-rich and less developed, creating a frontier gold rush.
See the [economy/development plan](economy-and-development-plan.md) for custody, production,
brokers, tech dependencies, geography, real-time rights and the capability/validation map.

## Planetary development, acquisition and frontier settlement

**Corrected model, 2026-11-16:** the [regional plan](regional-economy-and-governance-plan.md)
requires multiple governed economies on a physical planet. Each player initially controls
one region's development and public budget alongside a separate trading company. Regional
population, households, condition and accounts remain independent; AI governs other regions.
<a id="region-supply"></a>A server launches with a configurable number of regions made by the
universe generator, enough suitable home regions for its group, later joiners and an AI economy.
As players join it keeps the share of free home regions to players within a configurable range,
generating new regions when the share falls below it ([D-076](decision-ledger.md#d-076)); the
range's values are open. Generation runs outward, so allocating in generation order gives early
players the regions nearest the centre: an intended early-joiner advantage.
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

Players may later purchase eligible AI regional control rights or settle frontier worlds.
All player-governed regions are protected from the AI-acquisition mechanic. Eligible AI
neighbours on the same planet remain purchasable without a sibling governor's veto; reserved
onboarding regions stay ineligible. Purchase transfers regional public decision rights,
leaving treasury funds public and private companies, ships and stations with their owners.
Frontier concessions cover eligible resources in one named region; initial port-construction
opportunities are also regional, with competing ports at concession expiry. Shared ports may
serve several regions on that planet without consuming their construction opportunities or
granting government/resource rights. Deliveries name a region and remain physical; owners
choose offered terms and honour accepted bookings. Allocate starting regions only with an
operational starter-compatible public station and physical regional delivery connection.
New players spawn connected to working station life support; allocation requires an
available service connection. The starter's selected oxygen and one fresh installed
cartridge remain intact while connected service works. Station provisioning and later
outages remain separate design choices.
Basic docking, trading and regional delivery access remain available despite corporate
restrictions, retaining charges, finite capacity/goods/funds and private-port policies.
Detailed transport, starter fees/capacity/funds, onboarding/claim compatibility and
construction/control changes need separate rules.
One region is a starting entitlement,
not a permanent ownership cap or sovereignty over sibling regions. Public and company funds, inventory and
permissions remain distinct; visitors' private assets retain their existing ownership.

If needed, add systems at the outer reaches of the existing map without moving or regenerating
established systems. Frontier planets are typically less developed and resource-rich, with
some uninhabited and suitable for terraforming and settlement. Opening trade/development needs
a [government/corporate stargate expedition](stargate-expansion-plan.md): fund and ship the gate
on an automated unmanned FTL flight, timed by distance and configurable speed/pacing over
hours/days of server-running wall time. Shutdown pauses flight. Operational gate owners set
access/fees and may refuse rivals. Gate delivery, live topology activation, acquisition and
colonisation remain separate milestones; these capabilities are not implemented or validated.

See [planetary development and frontier planning](planetary-development-plan.md) for impacts,
the capability map, unresolved choices and acceptance criteria. Its staged direction is:

1. Account for independent planet, region, government/company and public/household identities.
2. Prove multiple governed regions on one planet, AI neighbours and saved development queues.
3. Add eligible AI regional-control purchases and portfolio policies, preserving player-region
   protection, onboarding reserves, public money and private owners.
4. Add expensive corporate spaceport construction for eligible regions, physical
   resource exploitation and colonies with company-controlled puppet governments.
5. Prove deterministic frontier expansion, physical access, terraforming and settlement.

The original one-system/two-venue scenario remains a small integration fixture. A later
governance scenario must include multiple economies on one planet, a home region for each
test player and additional AI regions.
Do not interpret the old deferral of player-controlled civilisations as excluding this new
planet-development goal. Whole-civilisation sovereignty remains outside the agreed scope.
