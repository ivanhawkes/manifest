# Glossary

The one home of every short definition in Manifest's documents. Each term says what the word means
in the code and links the document that explains it. Other documents link a term's first use here,
as `[tote](glossary.md#tote)`, rather than define it again.

Terms are in alphabetical order. Words Jettison owns (entity, component, system, berth class,
platform, vehicle, definition registry, ...) are defined in
[Jettison's glossary](../../jettison/docs/glossary.md); an entry here says only how
Manifest uses one, and links it. The [documentation index](README.md) lists every document.

### Account

A balance of whole credits (`World::Account`) on an account entity, whose identity is its kind and
its owner's ID: a company's, a region's public and household accounts, an employer's, a broker's, a
venue's, a tender's and a bond's (`World::Kind`). Only an account receives credits. See [the hosted
world](world.md).

### Actor

Who sends a [request](#request) (`Command::Actor`): a player, by their [principal](#principal)'s ID,
or the host itself as the Administrator. The trusted caller resolves it, a server from the client's
[token](#token); it is never read from a payload. See [the hosted world](world.md).

### Administrator

The [actor](#actor) of the host itself (`Command::ActorKind::Administrator`), the only one that may
send administrative commands such as `Manifest.Company.Provision`. Its [snapshot](#snapshot-query)
shows every company. See [the hosted world](world.md).

### Admission

The one way a [request](#request) reaches the world (`Command::Requests::Admit`): it checks the
actor, the command and its version, the payload and the sequence, assigns the next global order and
queues the request to run at the start of the next [step](#step). A refusal consumes no sequence.
See [the hosted world](world.md).

### Age

A period of technology (`Core::AgeNode`, `data/skill-tree/ages.yaml`): each item names the age that
opens it, and each age has a gate, a Tech [skill node](#skill-node) of one tier that requires the
previous age's gate. See [the development sandbox](sandbox.md).

### AI issuer

The part of the tenders' system that tenders for a region whose governor the AI runs, or whose
player's development policy allows it: its head project's inputs and the reliefs its households
need, with procurement's AI windows. See [regions, governors and public projects](governance.md).

### AI operator

A venue operator no player directs. Once a day it moves its venue's [pricing level](#pricing-level)
with how many of its station's pads are taken (`manifest.pricing`). See [stations](stations.md).

### Arrears

Berth fees a docked ship's company could not pay (`World::Arrears`), owed to the venue and growing
by daily interest at the venue's rate, at most the tuning's usury limit
(`charges.maxArrearsRateBps`). They are paid first as the next hour ends, and in full before the
ship may leave. See [stations](stations.md) and [seizure](#seizure).

### Backup

A copy of a world written with each [journal snapshot](#snapshot-journal) when the server names a
backups directory: the world's identity, the snapshot and a [catalogue](#catalogue) naming it, as
`<world>.<batch>`, the newest 24 kept by default (`Host::Storage`). It lives outside the [world
directory](#world-directory), and `manifest --restore` brings one back. See [saves, the journal and
backups](persistence.md).

### Batch (journal)

One immutable file of the [journal](#journal): the steps taken since the last flush and the requests
that ran in them, followed by its 64-bit FNV-1a, which is also its identity in the chain of batches.
A crash loses only what ran since the last batch. Not a production batch, which is an
[employer](#employer)'s run of work. See [saves, the journal and backups](persistence.md).

### Berth

Jettison's place where one vehicle lands ([berth](../../jettison/docs/glossary.md#berth)); Manifest
calls a station's berths pads. A ship docked on one pays the venue's hourly berth fee
(`manifest.berths`). See [stations](stations.md).

### Bid

A company's sealed offer on an open [tender](#tender) (`World::Bid`): a total in whole credits, from
1 to the budget, for all its totes by the delivery close, seen only by its own principal. The lowest
total wins at the bid close, the earlier accepted on a tie, if its company can fund the [performance
security](#performance-security). See [regions, governors and public projects](governance.md).

### Book

A venue's resting [orders](#order) for one item, buys on one side and sells on the other. Matching
is best price first, then oldest, at the resting order's price, and each side holds at most
`Trade::kMaxRestingOrders` (64). See [orders, brokers and market observations](trading.md).

### Broker

A venue's market maker (`World::Broker`), a trading principal of the venue's operator, by the
venue's ID, with its own account and stock. On each [economic turn](#economic-turn)'s last step it
requotes: a [ladder](#ladder) of asks for what it holds and bids within a share of its funds, around
each good's [native price](#native-price). See [orders, brokers and market observations](trading.md)
and [prices](pricing.md).

### Bundle

The distribution: a tarball, `manifest-<version>-<game tree>-linux-x86_64.tar.gz`, staged from a
clean checkout by `scripts/stage-bundle.sh`. It holds the release executables (`manifest`,
`manifest-token`, `manifest-validate`, `manifest_tui`), the content pack under `share/manifest` and
the licences, and runs on x86-64 Linux above the [glibc floor](#glibc-floor). See [the
host](host.md).

### Burn

A journey's choice of how hard its drive works (`Travel::Travel`): the share of the cruise's time
the engines thrust, in basis points (`thrustBps`), and the acceleration in thousandths of g. A
harder burn arrives sooner and costs more fuel; a softer one [coasts](#coast). See [travel and
fuel](travel.md).

### Cadence

How often something recurs in steps. The economic cadence is `economy.intervalSeconds` over the
fixed step (3,600 steps): it sets the [economic turn](#economic-turn). The journal's cadence
(`Host::Cadence`) is how often it flushes a [batch](#batch-journal) (every 30 s) and writes a
[snapshot](#snapshot-journal) (every hour). See [the regional economy](economy.md) and [saves, the
journal and backups](persistence.md).

### Cartridge

A carbon scrubber cartridge, the good `ItemCarbonScrubberCartridge`, counted in occupant-days
(`lifeSupport.cartridgeOccupantDays`). A hull's scrubber holds a set number;
`Manifest.Company.Replace` installs a fresh one from the hold in place of the most used. See
[stations](stations.md).

### Catalogue

The [journal](#journal)'s index file (`Host::Catalogue`): the head batch and its identity, the step
after it, and the snapshots kept. It is written last and replaced whole, so it always names a
complete set. See [saves, the journal and backups](persistence.md).

### Coast

The middle of a cruise flown at less than full thrust: the engines thrust for a share of the time,
half at each end, and the ship coasts between, burning no fuel. A policy can choose a softer
[burn](#burn) to save fuel. See [travel and fuel](travel.md).

### Company

A player's trading business (`World::Company`), owned by a [principal](#principal), with an account,
goods at venues and ships; its ID is `Company<n>`. Provisioning gives a new player one. NPC traders
have companies too. See [companies, ships and cargo](companies.md).

### Concession

Planned: a corporation's exclusive term to exploit a frontier, `concessions.termHours` (72) of real
server-running hours, not simulation time. Only the tuning value exists; no rule reads it yet. See
the [planetary development plan](planetary-development-plan.md).

### Condition

A region's [health](#health) and [happiness](#happiness), each 0 to 100. A shortage of essentials
lowers both, scaled by its [severity](#severity); a turn with every essential met recovers them
(`Trade::Updated`, the `condition` block of `balance.yaml`). Condition sets the [work
rate](#work-rate). See [the regional economy](economy.md).

### Content identity

What a world is held to about its content (`Content::ContentIdentity`): the canonical content
manifest, every loaded definition in a fixed binary layout plus the selected scenario's name, and
its [fingerprint](#fingerprint). Host paths, mounts and pack compression are not in it, so a
directory and a pack of the same content are alike. See [content](content.md).

### Content set

Everything a game runs on (`Content::ContentSet`): every [definition](#definition) loaded and
checked, resolved for one [game mode](#game-mode) and, for a hosted world, one
[scenario](#scenario), with its [content identity](#content-identity). It keeps no reference to the
files it came from. See [content](content.md).

### Credit

The game's currency, held as whole credits in 64-bit [accounts](#account). Credits move instantly
between authorised accounts; goods do not (see [title and custody](#title-and-custody)). Prices are
finer, in [decimal prices](#decimal-price). See [prices](pricing.md).

### Cross-review

An adversarial review of a stage's commits by Codex, after the [gate](#gate-ci), its prompt and
findings under `build/reviews/`. Each finding is answered with a fix or evidence, for at most three
rounds. Each stage's rounds are recorded in [history](history.md); see also Jettison's
[cross-review](../../jettison/docs/glossary.md#cross-review).

### Cruise profile

How a drive covers a cruise when its game mode gives one (`Jettison::Transit::MotionProfile` on a
`Space::CruiseDrive`): a boost, a sustained acceleration, turning at the midpoint, and an optional
thrust share. A game mode's `cruise` sets a manned and an unmanned profile; without one, a drive's
time follows a power law of the distance and burns no fuel. See [travel and fuel](travel.md) and
Jettison's [motion profile](../../jettison/docs/glossary.md#motion-profile).

### Decimal price

A price in millionths of a credit per the item's authored unit (`Core::Decimal`, 64 bits). Authored
prices are parsed exactly from text, and amounts settle to whole [credits](#credit) with stated
rounding. Quantities, physics and effect stats stay floating point. See [prices](pricing.md).

### Decision (D-nnn)

A settled choice, numbered `D-001` upwards, in the [decision ledger](decision-ledger.md), each row
anchored as `decision-ledger.md#d-087`. Documents cite a decision where it explains why, never as
the explanation itself.

### Definition

A typed record of game data loaded from YAML through Jettison's definition registry: an item, a
hull, a scenario, the tuning. Each file's name is its record name, the authored ID in lower case
with hyphens (`ItemCopper` is `item-copper`, `Content::RegistryName`). See [content](content.md) and
Jettison's [definition](../../jettison/docs/glossary.md#definition).

### Development queue

A region's public [projects](#project) in the order they run (D-110): only the head tenders and
works, the rest wait as Queued. The governor's director queues and cancels projects
(`Manifest.Procurement.Queue`, `CancelProject`) and may set a development policy that lets the [AI
issuer](#ai-issuer) tender within a spending limit. See [regions, governors and public
projects](governance.md).

### Dock hull

A hull's record in Jettison's docks (`Dock.VehicleDefinition`, `data/docks/hulls`): its size and the
[pad class](#pad-class) it lands on. Its pad class is the ship's [fee class](#fee-class). See
[companies, ships and cargo](companies.md).

### Economic turn

The interval at which the regional economy settles, `economy.intervalSeconds` (60 simulation
seconds). Production works every step; on a turn's last step employers pay wages and sell,
households buy and consume, condition moves and brokers requote. See [the regional
economy](economy.md).

### Effect

What a tier of a [skill node](#skill-node) buys in the sandbox (`Core::EffectTypes`): a list of
signed modifiers, one per stat, each adding, multiplying or setting per tier, landing on a domain
(ship, yard, route, survey, market or body) from a skill or a technology. The hosted game reads a
ship's effects for its hold and fuel tanks. See [the development sandbox](sandbox.md).

### Employer

A region's producer (`Kind::Employer`): it works each market row the region makes in production
batches of whole totes, reserves each batch's gross wage bill from its account as the batch starts,
pays the wages as the work is done and sells its output to the venue's broker. See [the regional
economy](economy.md).

### Escrow

The credits a resting buy [order](#order) holds back from its owner's account to pay for what it may
still fill, fees included; what is left returns when the order leaves the book. See [orders, brokers
and market observations](trading.md).

### Facility

A region's built facility (`World::Facility`), seeded by the scenario: for now only a workshop,
operational or not. A [project](#project) works only while its region has an operational workshop.
See [regions, governors and public projects](governance.md).

### Fast-forward

Advancing the hosted world many steps at once to the state stepping would leave
(`SimWorld::FastForward`), when every system's next [quiet step](#quiet-step) allows. The server's
loop and the console's long `step` use it. See Jettison's
[fast-forward](../../jettison/docs/glossary.md#fast-forward) and [the hosted world](world.md).

### feature-base

The integration branch: work is pushed there, CI runs on its pushes, and `scripts/promote.sh`
fast-forwards `main` to a commit of it whose run passed. See [build and gates](build-and-gates.md).

### Fee class

The class a venue charges a ship by (`Content::FeeClass`): the [pad class](#pad-class) of its hull's
[dock hull](#dock-hull). A venue posts a docking fee and an hourly berth fee for each class. See
[stations](stations.md).

### Fill

One match of an incoming [order](#order) with a resting one, at the resting order's price. Each side
pays the venue's listing fee on the gross, and the fill is recorded in the venue's sales. One
placement settles at most `Trade::kMaxFills` (64). See [orders, brokers and market
observations](trading.md).

### Fingerprint

The 64-bit FNV-1a of a [content identity](#content-identity)'s manifest: a short name a list or a
log can show, not an authenticity check. The manifest itself is what a world is compared on. See
[content](content.md).

### Fixture

The trading scenario run end to end through the public commands and queries
(`source/test/FixtureTests.cpp`, ctest `fixture*`): bought at Ore, flown to Brine, sold, the ship
upgraded, with the economy, life support and observations running. It is the cross-feature evidence.
See [build and gates](build-and-gates.md).

### Founding

A player's company founding their homeworld (`Manifest.Company.Found`, D-110): in a region no one
governs it pays `company.foundingPrice` into the region's public account, and a new
[polity](#polity) the player directs governs the region. A player founds at most one. See [regions,
governors and public projects](governance.md).

### Fusion pellets

The torch's dear fuel, the good `ItemFusionPellets`, by the kilogram, refined at few stations. They
fill a drive's magazine (the hull's `pelletMagazine`) from totes in the hold, and burn at a rate
that rises steeply above 1 g (the drive's `curveExponent`). See [travel and fuel](travel.md).

### Game mode

A selection of ages, items and skills (`Content::GameModeDefinition`, `data/game-mode`), with an
optional [cruise profile](#cruise-profile) for its hulls: `manifest`, the full game, and `astralis`.
A [scenario](#scenario) names the mode it runs. See [content](content.md).

### Gate

A jump point's stargate (`Space::Gate`): a ring the ship flies through once the gate has cleared it,
which sends it along the [lane](#lane), its owner charged the lane's toll. It is built on Jettison's
[gate (transit)](../../jettison/docs/glossary.md#gate-transit). The hosted world does not attach
gates yet. See [space](space.md).

### Gate (CI)

`scripts/ci.sh`, the local check a chunk of work passes before a push: formatting, the Debug build
and suites, the same with the TUI and presentation off, Release, optionally ASan/UBSan and
clang-tidy. The GitHub workflow is not the gate (D-116). See [build and gates](build-and-gates.md).

### glibc floor

The oldest glibc the [bundle](#bundle) runs on, 2.39 (`cmake/Distribution.cmake`). Symbols glibc
versioned later are bound to their older versions (`source/apps/distribution/GlibcFloor.cpp`), and
`scripts/check-distribution.sh` refuses a binary that needs a newer one. See [the host](host.md).

### Governor

The [organisation](#organisation) that governs a region (`World::Region::governorId`), empty for a
region no one governs. A player directs it when the organisation's `principalId` is theirs;
otherwise the AI runs it. See [regions, governors and public projects](governance.md).

### Happiness

One half of a region's [condition](#condition), 0 to 100: it falls faster than [health](#health) in
a shortage and recovers faster. See [the regional economy](economy.md).

### Health

One half of a region's [condition](#condition), 0 to 100: it falls with the [severity](#severity) of
the households' shortage and recovers only in a turn with every essential met. See [the regional
economy](economy.md).

### Holding

An owner's goods of one item at one venue's warehouse, in totes (`World::Holding`): available, and
reserved behind a sell order. An owner's holdings are its `World::Stock`. Goods in a ship are its
cargo instead. See [companies, ships and cargo](companies.md).

### Hosted world

The authoritative game world a host runs (`Host::Start`): Jettison's `SimWorld` with Manifest's
state, commands and systems, stepping at 1/60 s, changed only by admitted [requests](#request) and
its own systems. A server, the console's `host` and the TUI run it. See [the hosted world](world.md)
and [architecture](architecture.md).

### Household

A region's population as a buyer (`Kind::Household`): its money in the region's household account
and its goods in a `World::Pantry`. On each economic turn it buys and consumes its [needs](#need),
then, every need met, [optional goods](#optional-good). See [the regional economy](economy.md).

### Hull

A ship's build (`Manifest.Hull`, `data/ships/hulls.yaml`): its base stats, the effects it has slots
for, its occupants, oxygen tanks, cartridges, pellet magazine and mass. The starter is
`HullLoneTraderStarter`. See [companies, ships and cargo](companies.md).

### IBC

An intermediate bulk container: one cubic metre of a good, its `unitsPerCubicMetre` in its unit,
eight [totes](#tote) (`Core::kTotesPerIbc`). See [content](content.md).

### Job

What a ship's [standing policy](#policy-standing) is doing (`World::Job`): choosing a trade, flying
to the source, buying, flying to the destination, selling, blocked, or bidding on a tender. It acts
only through its owner's own commands (`Command::Requests::Act`). See [standing policies and NPC
traders](policies.md).

### Journal

How a server keeps its world (`Host::Journal`): the world steps and answers at once, and every 30 s
the steps since the last flush are written as a [batch](#batch-journal); every hour a
[snapshot](#snapshot-journal) is written. A start loads the newest snapshot and replays the batches
after it. See [saves, the journal and backups](persistence.md).

### Jump

A ship's passage along a [lane](#lane) (`Space::OnLane`): out of its system through a jump point, in
no system for the lane's transit time, and out at the far jump point. Not yet part of the hosted
world, which gives the lanes no galaxy. See [space](space.md).

### Jump point

A body of a star system (`Space::BodyKind::JumpPoint`) where a [lane](#lane) ends and a
[gate](#gate) stands. See [space](space.md).

### Knowledge

What a region knows (`World::Knowledge`): its skills, by skill node ID, and the [recipes](#recipe)
its projects have unlocked there, which outlast the facility that worked them. See [regions,
governors and public projects](governance.md).

### Ladder

A broker's quotes for one good: up to four orders a side (`Trade::kLevels`), each a rung of an
eighth of its [target stock](#target-stock), priced on its supply curve so that asks climb as it
sells down and bids fall as it buys up. A [market report](#report-market) keeps the same four levels
a side. See [prices](pricing.md).

### Lane

A jump lane between jump points of two star systems (`Space::JumpLane`), travelled both ways, with a
transit time and a toll from its length in light years. The lanes are the galaxy's graph
(`Space::Galaxy`). See [space](space.md).

### Life support

What keeps a ship's occupants alive: its own [oxygen tanks](#oxygen-tank) and
[cartridges](#cartridge) in transit, or a connection to a docked venue's life support, of which a
venue has `lifeSupportCapacity` (`manifest.lifeSupport`). See [stations](stations.md).

### Loss allowance

The whole credits below a lot's floor (its cost, charges and the policy's target profit) that its
owner lets it sell for (`World::Policy::lossAllowance`). What a lot spends of it stays spent. The
default is 0. See [standing policies and NPC traders](policies.md).

### Lot

The totes of one item in a ship's hold (`World::Lot`). A [job](#job)'s lot is the one it is trading,
with what it cost and the charges, life support and fuel attributed to it, which its sale must
recover. See [standing policies and NPC traders](policies.md).

### Mod

A content [package](../../jettison/docs/glossary.md#package) described by a `mod.yaml`. Manifest's
own content is one (`mods/manifest/mod.yaml`), and an [overlay](#overlay) is another mounted over
it. A host mounts no player-installed mods. See [content](content.md) and Jettison's
[mod](../../jettison/docs/glossary.md#mod).

### Mount

One content source in the file system. A host mounts the Manifest package as `manifest`, then each
[overlay](#overlay) under its `mod.yaml` name, read-only (`Host::Mounts`). See [content](content.md)
and Jettison's [mount](../../jettison/docs/glossary.md#mount).

### Native price

A good's reference price at a venue, fixed as the world starts: the item's base price moved by the
scenario's total supply and demand of it, and nudged toward the venue's own demand or production. A
broker quotes it at its [target stock](#target-stock). See [prices](pricing.md).

### Need

An essential the households consume (`Content::NeedDefinition`, `balance.yaml` `needs`): a rate per
thousand people per day in its unit, met by any of its items, and a weight in the
[severity](#severity) of a shortage. See [the regional economy](economy.md).

### NPC trader

A trader the scenario starts with (`Content::TraderDefinition`): a principal, company and ship
provisioned as a player's, its principal the world's own (`World::Principal::npc`), its ship's
standing policy on with contracts. No client may connect as it. See [standing policies and NPC
traders](policies.md).

### Observation

Refreshing a player's [market reports](#report-market) (`manifest.observe`): after travel, at the
end of each step, of every venue where a ship of their fleet is docked. See [orders, brokers and
market observations](trading.md).

### Offer

Something a company may buy with its own credits (`Content::OfferDefinition`, `data/offers`): a tier
at a time of a [skill node](#skill-node) fitted to its ship as a ship system, paid to no one
(`Manifest.Company.Upgrade`). See [companies, ships and cargo](companies.md).

### Opened tote

What is left of a tote taken out of whole-tote accounting (`World::Opened`), in the item's unit: the
households' in their pantry, or a ship's for its life support or magazine, which still takes a whole
tote of the hold. See [companies, ships and cargo](companies.md).

### Optional good

A good no [need](#need) names, which households buy only in a turn every need is met, at up to a
premium over its native price and a share of its demand set by its category's bucket (`balance.yaml`
`optionalGoods`). See [the regional economy](economy.md).

### Order

A limit order to buy or sell totes of an item at a venue (`World::Order`, `Manifest.Trade.Place`).
It matches at once against the [book](#book); what is left rests as `Order<n>` until it fills, is
cancelled or expires after `market.orderLifeSeconds`. A sell reserves its totes and a buy holds
[escrow](#escrow). See [orders, brokers and market observations](trading.md).

### Organisation

A governing or corporate body (`World::Organisation`): a [polity](#polity) or a corporation,
directed by a player's principal or, with none, run by the AI. A region's governor and a venue's
operator are organisations. See [regions, governors and public projects](governance.md).

### Overlay

A directory or pack mounted over the Manifest package with `--overlay`, named by its `mod.yaml`, a
later one winning (D-070). Tests use overlays to change content, as
`source/test/overlays/one-trader` does. See [content](content.md).

### Oxygen tank

A tank of a hull's installed oxygen bank (the hull's `oxygenTanks`, in kilograms). In transit each
occupant draws `lifeSupport.oxygenKilogramsPerOccupantDay`; `Manifest.Company.Refill` fills the
tanks from `ItemOxygen` in the hold. See [stations](stations.md).

### Pack

A directory of content in one Jettison pack file. A build carries Manifest's as
`share/manifest/manifest.pack` beside its executables (`Host::DefaultContent`). See Jettison's
[pack](../../jettison/docs/glossary.md#pack) and [content](content.md).

### Pad class

A size of pad, a Jettison [berth class](../../jettison/docs/glossary.md#berth-class) Manifest
authors (`data/docks/pads`): skiff, corvette, frigate, destroyer and cruiser. A hull lands on the
smallest class it fits, which is also its [fee class](#fee-class). See [stations](stations.md).

### Payroll

An [employer](#employer)'s payment of the wages its batches have earned, on each economic turn's
last step: the gross bill is `production.wageShare` of the output's value at its base price,
reserved as the batch starts, split by [withholding](#withholding). See [the regional
economy](economy.md).

### Performance security

The bond a tender's winner locks at award (`BondAccount`): `procurement.performanceSecurityRate` of
its total, rounded up. It is returned when the contract is fulfilled; on default the undelivered
share, rounded down, goes to the region's public account. See [regions, governors and public
projects](governance.md).

### Pin

A value fixed so that a change to it is a decision. The SDK pin is the Jettison commit Manifest
builds against, `MANIFEST_JETTISON_REVISION` in `source/external/CMakeLists.txt`; a pinned hash in a
test fails when a world's results or layout change. See [build and gates](build-and-gates.md) and
Jettison's [pin](../../jettison/docs/glossary.md#pin).

### Planet

A physical body of a star system with no population or account of its own
(`Content::PlanetDefinition`): its [regions](#region) hold those. See [regions, governors and public
projects](governance.md).

### Policy (standing)

A ship's standing trade policy, set by its owner (`World::Policy`, `Manifest.Policy.Set`): whether
its [job](#job) runs, the profit a trade must make, the [loss allowance](#loss-allowance), the
thrust every leg flies or 0 to choose, and whether it bids on tenders. See [standing policies and
NPC traders](policies.md).

### Polity

A government [organisation](#organisation) (`OrganisationKind::Polity`). A player gets one by
[founding](#founding) their homeworld. See [regions, governors and public projects](governance.md).

### Pressure

How short a need has gone (`World::Unmet::pressure`): its unmet share smoothed over an hour, 0 to 1.
It raises what households will pay above the ask, toward the curve's ceiling, and past 0.25 calls a
[relief](#relief) tender. See [prices](pricing.md).

### Pricing level

A venue's fees as a share of the tuning's defaults (`World::Pricing`, basis points, 10,000 at the
start). An [AI operator](#ai-operator) moves it daily by occupancy within `charges.pricing`'s
bounds. See [stations](stations.md).

### Principal

A player's control identity in the world (`World::Principal`). Credentials and connections are not
world state. An NPC's principal is the world's own. See [the hosted world](world.md).

### Project

A region's public project (`World::Project`, a `Content::ProjectDefinition`): it gathers its inputs
through [tenders](#tender), consumes them once all are held, works its days while the region has an
operational workshop, and then unlocks a [recipe](#recipe) or teaches a skill in that region. See
[regions, governors and public projects](governance.md).

### Provisioning

The Administrator's command that brings a player into the world (`Manifest.Company.Provision`):
their principal and starting reports, a company with `company.startingCredits`, and the starter hull
parked at the [starter venue](#starter-venue), fuelled and connected to its life support. See
[companies, ships and cargo](companies.md).

### Quiet step

A step in which a system would write nothing a formula of the clock cannot give. Each Manifest
system declares its next busy step (`Jettison::Sim::Quiet`) so the world can
[fast-forward](#fast-forward). See Jettison's [quiet
step](../../jettison/docs/glossary.md#quiet-step).

### Reaction mass

The torch's cheap fuel, `ItemReactionMass` (purified water ice), by the kilogram, made at every
station and bought straight into a ship's tanks as it refuels. It burns by the rocket equation to
first order. See [travel and fuel](travel.md).

### Receipt

What [admission](#admission) answers (`Command::Receipt`): queued with its global order, completed
for a retry of one that has run, or refused with why. It is not a durable acknowledgement: a crash
loses what ran since the last journal flush. See [the hosted world](world.md).

### Recipe

A transformation of input totes into output totes (`Content::RecipeDefinition`, `balance.yaml`
`recipes`). A project unlocks one in its region's [knowledge](#knowledge); no system works a recipe
yet. See [regions, governors and public projects](governance.md).

### Region

The one owner of a population (`World::Region`): on a planet, of a civilisation, governed by an
organisation or no one, with health, happiness and its public, household and employer accounts. A
venue serves the regions it names. See [regions, governors and public projects](governance.md).

### Relief

A tender a region's government calls for an essential its households are short of (`Trade::Relief`):
when a need's [pressure](#pressure) passes 0.25, the day's shortfall at the cheapest of its goods,
budgeted at an urgency of three times the pressure. See [regions, governors and public
projects](governance.md).

### Report (market)

What a player last saw of a venue's market (`World::Report`): each good's best four bids and asks,
what sold over the [sales window](#sales-window) and its last sale. It is live while a ship of
theirs is docked there and stale otherwise; a venue with none is unknown to them. See [orders,
brokers and market observations](trading.md).

### Request

A command as the world queues, saves and replays it (`Command::Request`): its name, version and
payload bytes, under the [actor](#actor) and the actor's sequence number, with the global order
admission gave it. See [the hosted world](world.md).

### Request ledger

The saved record of every actor's requests (`Command::RequestLedger`): the next sequence, the
completed results kept for retries (`kRetainedResults`, 256) and the pending ones. A retry is
answered from it, across reconnects and restarts. See [the hosted world](world.md).

### Sales window

The span a [report](#report-market)'s sold totes are counted over, `market.salesWindowSeconds` (an
hour). See [orders, brokers and market observations](trading.md).

### Sandbox

The development model from before the hosted world: the `sim`, `ecs`, `effects`, `market`,
`progression` and `universe` modules, reached through the console's `generate`, `tick` and
`upgrade`. It does not touch the [hosted world](#hosted-world). See [the development
sandbox](sandbox.md).

### Scenario

The world a game starts from (`Content::ScenarioDefinition`, `data/scenarios`): its game mode,
starter hull and venue, items, systems, planets, civilisations, organisations, regions, venues and
NPC traders. A host requires one (`--scenario trading` or `governance`). See [content](content.md).

### Seizure

The sale of a ship's cargo for its [arrears](#arrears) (`manifest.seizure`): once they have stood
`charges.seizureGraceDays` (7), at the day's end the cargo is sold through its owner's commands at
the venue's bids until the debt is covered. See [stations](stations.md).

### Session

One admitted connection to a server (`Remote::Server`), as the actor its [token](#token) names, each
sent its own replies and its actor's [snapshot](#snapshot-query); an actor holds at most three. See
[the dedicated server](server.md) and Jettison's [session](../../jettison/docs/glossary.md#session).

### Severity

A turn's weighted share of the households' required needs gone unmet, 0 to 1 (`Trade::Severity`), a
surplus of one need never making up for another. It scales the fall of [condition](#condition). See
[the regional economy](economy.md).

### Skill node

A node of the skill tree (`Core::SkillNode`, `data/skill-tree`): Tech or Skill, with tiers,
prerequisites, a cost per tier, an [effect](#effect) and an optional exclusive group. Regions know
skills, projects teach them, and ships are fitted with them through [offers](#offer). See [the
development sandbox](sandbox.md).

### Snapshot (journal)

A world's whole save written by the [journal](#journal) every hour and at a clean stop; the last two
are kept, with every batch since the older. See [saves, the journal and backups](persistence.md).

### Snapshot (query)

What an actor may see of the hosted world, copied out between steps (`Query::Snapshot`,
`Query::Observe`): their own company, ships and orders, their market reports, every tender and every
region's public figures. A server sends it to each session after each step. See [the hosted
world](world.md).

### Spread

The gap between a broker's bid and ask, `brokers.spreadBps` of the price, half on each side. A round
trip through a broker loses it. See [prices](pricing.md).

### Stage

One of the five delivery stages that built Manifest (host, content, authority, trading, recovery),
all closed; their records are in [history](history.md).

### Star system

A system's bodies in their orbits (`Space::StarSystem`, `data/systems`): star, planets, moons,
stations and jump points, each a Jettison frame. A scenario names the systems it uses. See
[space](space.md).

### Starter venue

The scenario's `starterVenue`, where every new player and NPC trader starts: an operational public
station that docks the starter hull and has life support (D-089). In the trading scenario it is
`VenueOreHighport`. See [companies, ships and cargo](companies.md).

### Station

A venue's physical place in its star system: a station body built to a Jettison platform layout,
whose pads ships dock at (`Travel::OpenStations`). See [stations](stations.md) and Jettison's
[platform](../../jettison/docs/glossary.md#platform).

### Step

One 1/60 s advance of the hosted world (D-086): the queued requests run, then Manifest's and
Jettison's systems in a declared order. See [architecture](architecture.md) for the step in order
and Jettison's [step](../../jettison/docs/glossary.md#step).

### Target stock

The stock a broker quotes a good at its [native price](#native-price): the scenario's seeded totes
of it at that venue, at least one. Below it the broker's prices rise, to four times native when
empty; above it they fall. See [prices](pricing.md).

### Task ID

A letter and number naming a piece of Manifest's work, such as `W22` or `L16`: M maintenance, H to R
the five stages (H, C, A, W, R) and N the network, L later work, T the 2026-11-16 handoffs. Open
tasks are in the [delivery order](delivery-order.md), finished ones in [history](history.md).

### Tender

A region's call for goods (`World::Tender`): a project's input or a [relief](#relief), at a venue
serving the region, with a budget held in its own account, a bid window and a delivery window. At
the bid close it goes to its lowest [bid](#bid); the winner delivers and is paid as it goes. See
[regions, governors and public projects](governance.md).

### Title and custody

Who owns goods and where they are, kept apart from money. A company's goods are holdings at a
venue's warehouse or cargo in its ship; buying changes the owner where the goods already are, and
only a ship moves them. Credits settle instantly. See [companies, ships and cargo](companies.md).

### Token

A bearer secret a server's operator issues to one actor with `manifest-token issue`, printed once;
the server's token file keeps only its digest, and a client connects with it. Revoking it closes the
actor's sessions. See [the dedicated server](server.md) and Jettison's
[token](../../jettison/docs/glossary.md#token).

### Torch

The clean fusion drive of the small hulls, skiff to corvette (D-108): it burns [reaction
mass](#reaction-mass) at its `exhaustKms` and [fusion pellets](#fusion-pellets), more of both the
heavier the ship and the harder the [burn](#burn). The hauler fusion drive of medium freighters is the next class up (`data/ships/drives.yaml`). See [travel and
fuel](travel.md).

### Tote

The unit every good is held, moved and traded in: an eighth of a cubic metre, a container 500 mm a
side, whatever the good (D-087). Only whole totes move. See [content](content.md).

### Venue

A station or port as a market (`Content::VenueDefinition`): in a system, perhaps orbiting a planet,
with an operator, a [broker](#broker), a market of goods, a station layout, life-support capacity
and the regions it serves. See [orders, brokers and market observations](trading.md) and
[stations](stations.md).

### Wage tax

The share of wages withheld for the region's public account, `economy.wageTaxRate` (10%), by
[withholding](#withholding). See [the regional economy](economy.md).

### Withholding

How the [wage tax](#wage-tax) is taken (`Trade::Withhold`): a credit for each whole 1/rate credits
of gross paid, the remainder carried to the employer's next payment, so many small payments withhold
what one large one does. See [the regional economy](economy.md).

### Work rate

The share of its full rate a region's production works at (`Trade::WorkRate`): each of health and
happiness gives a factor from `condition.workRateFloor` at 0 to 1 at 100, and the two multiply. See
[the regional economy](economy.md).

### World directory

A world's writable directory, `manifest/world` under the XDG data directory by default
(`Host::DefaultWorld`), or `--world`: its identity file, batches, snapshots and catalogue, written
durably by one opener (`Host::WorldDirectory`). See [saves, the journal and
backups](persistence.md).

### World lock

The lock on the file `lock` in a [world directory](#world-directory) (`Host::kLockName`), held while
the directory is open, so a second server, this process included, cannot open the same world. See
[saves, the journal and backups](persistence.md).
