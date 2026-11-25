# Roadmap

Where Manifest is going, by area. Each area says what it is for, what is next and where its design
lives; the tasks themselves, in order, are the [delivery order](delivery-order.md). Only open work
is here: what is built is in the feature documents (the [index](README.md)), and how it came to be
is in [history](history.md). Why Manifest exists, and what is in and out of it, is
[scope](scope.md).

## Ships and drives

Today a skiff to a corvette flies a clean fusion [torch](glossary.md#torch). Larger hulls come next,
on dirtier drives: the hauler's deuterium fusion and the nuclear pulse drive, each with its own fuel
and an exclusion radius round stations it may not thrust within ([W22](delivery-order.md#w22),
[D-108](decision-ledger.md#d-108)). Life support grows with them: oxygen in a bank of separately
exchangeable tanks scaled to the hull ([L07](delivery-order.md#l07)), algae recycling that stretches
a ship's reach ([L34](delivery-order.md#l34)), and a consequence for running out
([L45](delivery-order.md#l45)). Ships are bought and sold at a venue's shipyard, and the compact
ship gets a settled purpose ([L48](delivery-order.md#l48)). The designs are in the
[ships plan](ships-plan.md).

## Hosting and recovery

The dedicated server runs a world with no player connected, and recovers it after a crash. Before it
is hosted for the public, connections are encrypted (Jettison's
[J01](../../jettison/docs/roadmap.md#j01), [D-081](decision-ledger.md#d-081)), and the service is
hardened, profiled and soaked unattended ([L01](delivery-order.md#l01)). Saves move off the thread
that steps the world, so a growing world's writes stall neither a step nor a frame
([L40](delivery-order.md#l40)). Stage 5 left the terminal UI's retry, the bundle's container tests
and stripped release binaries ([L41](delivery-order.md#l41)). The built contracts are [the
server](server.md), [persistence](persistence.md) and [the host](host.md).

## Space

The `space` module has the galaxy, jump lanes, gates and routes, but the hosted game crosses one
system by cruising. First they come into the game ([L08](delivery-order.md#l08)). Then new systems
are opened by funded stargate expeditions, timed by distance over the server's running time: their
rules ([L09](delivery-order.md#l09)), a first fixed gate pair ([L10](delivery-order.md#l10)), rival
connections ([L11](delivery-order.md#l11)) and a generated frontier ([L12](delivery-order.md#l12)).
Moongates join the places round one planet ([L35](delivery-order.md#l35)). Cruising and gates are
refined ([L37](delivery-order.md#l37), [L38](delivery-order.md#l38)), and station traffic lanes are
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

Regions gain governor-set tax rates ([L29](delivery-order.md#l29)), debt and lenders
([L30](delivery-order.md#l30)), a power grid and an AI economy ([L31](delivery-order.md#l31)),
and staple balances by kind of world ([L33](delivery-order.md#l33)); gold and gems move in cases
([L32](delivery-order.md#l32)). The design is the
[economy plan](economy-and-development-plan.md). The built rules (households and their needs,
health and happiness, grains and medical supplies) are [the regional economy](economy.md), with
[trading](trading.md) and [prices](pricing.md).

## Players' tools

[Standing policies](policies.md) trade unattended today. They gain scouting and fixed routes
([L46](delivery-order.md#l46)), and Wasm scripts that rank their trades without touching settlement
([L39](delivery-order.md#l39), the [policy scripts plan](policy-scripts-plan.md)). The console
reaches every command and shows the whole snapshot ([L44](delivery-order.md#l44)), and a client sees
a venue's fees before it pays ([L47](delivery-order.md#l47)).

## The graphical client

The graphical client ([L15](delivery-order.md#l15)) is how most players will play: it sends the same
commands and reads the same queries as the [terminal UI](console.md), on [the server](server.md),
and must not block the native loop. Its text is in four languages from the start
([L43](delivery-order.md#l43)). It holds:

- **Composition.** Jettison's rendered shell started through a host session, so Manifest is not
  composed twice ([J30](../../jettison/docs/roadmap.md#j30)).
- **Owners' settings.** A venue's fees and a region's taxes shown and edited
  ([D-098](decision-ledger.md#d-098), with [L29](delivery-order.md#l29)), and station policy and
  ownership views ([L04](delivery-order.md#l04), [L05](delivery-order.md#l05)).
- **Branding.** Stations' cultural and corporate look, and their customisation ([planetary
  development plan](planetary-development-plan.md)).
- **Travel.** A thrust slider and an acceleration in multiples of g for a journey
  ([D-107](decision-ledger.md#d-107)), and the galaxy and flight panels.
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
  models stream in ([J13](../../jettison/docs/roadmap.md#j13)).
- **Thruster ports** placed by a hull's definition, on a hull drawn from a model as well as a proxy
  ([J24](../../jettison/docs/roadmap.md#j24)).
- **A station's collision** kept to its layout's decks and piers when it is drawn from a model; a
  station of no layout needs the model's own collision mesh
  ([J12](../../jettison/docs/roadmap.md#j12)), and until then its model is reported and left out.
