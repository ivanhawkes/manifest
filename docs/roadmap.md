# Roadmap

Where Manifest is going, by area. Each area says what it is for, what is next and where its design
lives; the tasks themselves, in order, are the [delivery order](delivery-order.md). Only open work
is here: what is built is in the feature documents (the [index](README.md)), and how it came to be
is in [history](history.md). Why Manifest exists, and what is in and out of it, is
[scope](scope.md).

## Waiting on you

What needs the user's answer or action and is not a task in the queue. Each line goes when it is
settled.

- **Review the numbers.** The price bound, 52 times the base since [L51](history/tasks.md) so the
  widest spread content accepts fits, where the shipped spread reaches 27.3
  ([prices](pricing.md#base-prices)); and the drive numbers of [D-108](decision-ledger.md#d-108),
  proposed by an agent.
- **The packed totes' fallout.** `d7843df` priced every tote by its packed weight and was
  committed without the fixture suites (`ctest -R fixture`), and with the trade, company and
  procurement suites' new expectations unreviewed. Run the fixtures and settle what moved. The
  procurement suite's AI now opens more tenders (6 where it expected 2): find out why.
- **Delete `data/core-config/`?** A git-ignored leftover the build and the `packaging` suite skip
  ([content](content.md#limits-and-open-work)).
- **Questions from the docs review:**
  - Companies: the long-haul trader (7,200 totes) is in no scenario. Keep it as the hull the skill
    tree balances against?
  - Persistence: [D-115](decision-ledger.md#d-115) makes a reusable off-thread writer Jettison's,
    and Jettison's roadmap has no item for it. Add one?
  - Travel: fuel is priced on the straight line at departure, not the course flown. Intended?
  - Policies: no shipped scenario has NPC traders, only a test overlay. Intended?
  - Host: [D-070](decision-ledger.md#d-070)'s "nothing defaults" contradicts
    [D-114](decision-ledger.md#d-114)'s defaults: amend or supersede? The world seed is fixed at 0:
    a queue row?
  - Space: the hosted game attaches lanes and cruising but no gates or galaxy. Is the scenario's
    list of connections [space](space.md)'s, or content's and trading's? Is a zero-distance
    expedition right to be refused outright?
  - Economy: a condition's `initial` is never read; the trading fixture's regions grow 150% of
    their staple need, against [D-105](decision-ledger.md#d-105)'s "about 100 to 110%". Which?
  - Planetary: a venue's `public: false` is not enforced at the dock. Should founding require a
    reachable public station?
  - Sandbox: `spaceship.yaml` says the bunkers feed the drive, but `SkillShipFuelBunkers` has no
    prerequisite. Is the data or the comment wrong?
  - Trading: observation rebuilds each docked venue's report every step (a shortcut), and ships
    cannot be transferred. Queue rows?
  - Glossary: split cadence into the economy's and the journal's?

## Ships and drives

A skiff to a corvette flies a clean fusion [torch](glossary.md#torch), the medium freighter the
hauler's deuterium fusion and the pulse carrier the nuclear pulse drive, each with its own fuel and an
exclusion radius it may not thrust within, the pulse drive's about populated bodies too, licensed by
a polity and overridden at a fine ([W22b](history/tasks.md#w22b), [D-108](decision-ledger.md#d-108),
[D-117](decision-ledger.md#d-117)). Life support grows with them: oxygen in a bank of separately
exchangeable tanks scaled to the hull ([L07](delivery-order.md#l07)), algae recycling that stretches
a ship's reach ([L34](delivery-order.md#l34)), and a consequence for running out
([L45](delivery-order.md#l45)). Ships are bought and sold at a venue's shipyard, and the compact
ship gets a settled purpose ([L48](delivery-order.md#l48)). The designs are in the
[ships plan](ships-plan.md).

## Hosting and recovery

The dedicated server runs a world with no player connected, and recovers it after a crash. Before it
is hosted for the public, the service is hardened, profiled and soaked unattended
([L01](delivery-order.md#l01)); its connections are already sealed to the server's key (Jettison's
[J01](../../jettison/docs/history/neutral-sdk.md#j01-encryption), [D-081](decision-ledger.md#d-081)). Saves move off the thread
that steps the world, so a growing world's writes stall neither a step nor a frame
([L40](delivery-order.md#l40)). The built contracts are [the
server](server.md), [persistence](persistence.md) and [the host](host.md).

## Space

The `space` module has the galaxy, jump lanes, gates and routes, but the hosted game crosses one
system by cruising. First they come into the game ([L08](delivery-order.md#l08)). Then new systems
are opened by funded stargate expeditions, timed by distance over the server's running time: their
rules ([L09](delivery-order.md#l09)), a first fixed gate pair ([L10](delivery-order.md#l10)), rival
connections ([L11](delivery-order.md#l11)) and a generated frontier ([L12](delivery-order.md#l12)).
Moongates join the places round one planet ([L35](delivery-order.md#l35)). Gates are
refined ([L38](delivery-order.md#l38)), and station traffic lanes are
parked until traffic needs them ([L36](delivery-order.md#l36)). The design is the [space
plan](space-plan.md); what is built is [space](space.md).

## Corporate power and planetary development

The end goal is a trading company grown into a corporation with a stranglehold on trade, economies
and governments, with no win condition ([scope](scope.md)). Its levers come in order: corruption in
public tenders ([L02](delivery-order.md#l02)), buying AI regions' control
([L03](delivery-order.md#l03)), private-port bookings and access ([L04](delivery-order.md#l04)),
company-built ports ([L05](delivery-order.md#l05)), and resource concessions and company-run
colonies ([L06](delivery-order.md#l06)). Frontier worlds are then settled and terraformed
([L13](delivery-order.md#l13)), and government and media influence follow once that leverage is
proved ([L14](delivery-order.md#l14)).

<a id="region-supply"></a>A server keeps enough regions open to founding as players join, by
generating new ones outward without moving established ones ([L42](delivery-order.md#l42),
[D-076](decision-ledger.md#d-076)). The design is the [planetary development
plan](planetary-development-plan.md); what is built is [governance](governance.md).

## The economy

The economy grows from markets into a physical chain: extraction, surface and orbital transport
jobs, and warehouse handling charges ([L49](delivery-order.md#l49)). Production follows its
recipes, so an unlocked recipe turns real inputs into its outputs
([L50](delivery-order.md#l50)). Consignment storage ([L16](delivery-order.md#l16)) is the base for
warehouse receipts ([L19](delivery-order.md#l19)), forward contracts
([L21](delivery-order.md#l21)), loans against stored goods ([L22](delivery-order.md#l22)) and
auctions of seized goods ([L24](delivery-order.md#l24)). Freight contracts
([L18](delivery-order.md#l18)), paid price reports ([L20](delivery-order.md#l20)), cargo
insurance ([L23](delivery-order.md#l23)) and escrow between players
([L25](delivery-order.md#l25)) stand beside them, with options and futures as a stretch goal
([L17](delivery-order.md#l17)).

Regions' governors set their wage tax ([L29](history/tasks.md), done); regions gain debt and lenders
([L30](delivery-order.md#l30)), a power grid and an AI economy ([L31](delivery-order.md#l31)),
and staple balances by kind of world ([L33](delivery-order.md#l33)); gold and gems move in cases
([L32](delivery-order.md#l32)). The hosted prices reach 27.3 times their base at most, the
scarcity cap, a venue's want, the curve's ceiling and the spread multiplied
([prices](pricing.md#base-prices)). The design is the
[economy plan](economy-and-development-plan.md). The built rules (households and their needs,
health and happiness, grains and medical supplies) are [the regional economy](economy.md), with
[trading](trading.md) and [prices](pricing.md).

## Players' tools

[Standing policies](policies.md) trade unattended today. They gain scouting and fixed routes
([L46](delivery-order.md#l46)), and Wasm scripts that rank their trades without touching settlement
([L39](delivery-order.md#l39), the [policy scripts plan](policy-scripts-plan.md)). The console
reaches every command and shows the whole snapshot ([L44](history/tasks.md), done), and a client sees
a venue's fees before it pays ([L47](history/tasks.md), done).

## The graphical client

The [graphical client](client.md) is how most players will play: it sends the same
commands and reads the same queries as the [terminal UI](console.md), on [the server](server.md),
and must not block the native loop. Its text is in four languages, English, French,
German and Japanese ([languages](client.md#languages)).

Its first loop, L15a, is done ([history](history/tasks.md)): the composition, and the start, company,
market, travel, flight and service screens. What is still to come:

- **Server play.** Its tracer is done, one player trading on a dedicated server
  ([server play](client.md#server-play), L52 in [history](history/tasks.md)), and the content
  checked against the server's (L53), and reconnecting, a lost request resent. Still to come:
  choosing a server in the client ([after the tracer](client-plan.md#after-the-tracer)).
- **Owners' settings** ([L15b](delivery-order.md#l15b)). A venue's fees and a region's taxes shown and edited
  ([D-098](decision-ledger.md#d-098); the tax command is [L29](history/tasks.md)'s), and station policy and
  ownership views ([L04](delivery-order.md#l04), [L05](delivery-order.md#l05)).
- **Branding.** Stations' cultural and corporate look, and their customisation ([planetary
  development plan](planetary-development-plan.md)).
- **The galaxy panel.** The systems, their venues and the lanes between them, to choose a journey on.
  The travel screen lists the venues the player has a report of.
- **Names.** Goods and venues by their names, in each language, rather than their IDs; and a ship
  named once by its player, renamed only with approval ([D-123](decision-ledger.md#d-123)). The
  screens show a ship by its name already, and the starting ship is named as its player founds their
  company (L54).
- **A shipyard screen** beside the market screen: the hulls a station sells, their prices and the
  purse, on [L48](delivery-order.md#l48)'s shipyard. **A fleet screen** to list and attach to the
  company's ships.
- **A body's look as data.** A planet's, moon's or kind's look as a scene fragment a mod can shadow,
  so a system's file and its drawn scene agree (rings included).
- **The station dressing.** Floodlight poles and display ships collided with, not flown through; a
  pad's look as data (its paint, poles, floodlights' aim and a reserved pad's blinking lamps); and
  the floodlights' shadows.
- **A nozzle's glow** smooth and free of extra models, on an emissive scale per draw
  ([J27](../../jettison/docs/roadmap.md#j27)), so a hull drawn from a model can glow too.
- **A jump's cutscene** that the jump waits on, its arrival binding the gate
  ([J22](../../jettison/docs/roadmap.md#j22)); and a jump holding its effect while the new system's
  models stream in ([J13](../../jettison/docs/history.md#j13)).
- **Thruster ports** placed by a hull's definition, on a hull drawn from a model as well as a proxy
  ([J24](../../jettison/docs/roadmap.md#j24)).
- **A station's collision** kept to its layout's decks and piers when it is drawn from a model; a
  station of no layout needs the model's own collision mesh
  ([J12](../../jettison/docs/history.md#j12)), and until then its model is reported and left out.
