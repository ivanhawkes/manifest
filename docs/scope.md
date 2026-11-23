# Scope

Manifest supplies the civilisation, progression and trading model for a Jettison game. The
current scope is the C++ development harness, its terminal interface, authored YAML and mod
content packaging. Jettison owns rendering, input, audio, physics, engine scheduling, VFS,
serialisation and the sandboxed script runtime.

A prime purpose of the game (the user's, 2026-11-21) is a playfield where people experiment with
and learn how greed, fear, monopolies and other modern woes affect everyone's lifestyle, income
and happiness. Its balance numbers are starting points, refined stepwise with an understanding of
macroeconomics toward an enjoyable game, not a faithful emulation of today's economy. Markets are
liquid and volatile, their swings driven by needs and spending power, not by noise.

The configuration package mounts as a Jettison mod. It does not yet make the sample game execute
Manifest's economic rules. The [host/script boundary](host-script-boundary.md) defines linked
native ownership of those rules and capability-limited Wasm policy inputs. Decimal market prices
are implemented. Content loaders, world adapters and the scripted scenario remain next; see
[roadmap](roadmap.md). A second renderer, engine dependency stack or native shared-library mod
loader does not belong in this repository.

The approved future scope includes a CLI-launched dedicated network server for unattended
idle play. Economy, NPCs, autopilot, enabled auto-trading and development advance with zero
clients connected. Morph the existing TUI into a required remote gameplay client with
`connect`/`disconnect`; Manifest's GUI shares its server interface and uses public engine
presentation features. Manual TUI spaceport trading is a stretch goal. Admin permissions
remain separate. The server runs without a renderer or terminal. This remains planned work;
see [architecture](architecture.md#dedicated-server-and-administration-planned) and
[roadmap](roadmap.md#networked-idle-game-and-remote-clients).

The [gameplay/server plan](gameplay-and-server-plan.md) retains a tiny one-system/two-venue
trading fixture. The [regional correction](regional-economy-and-governance-plan.md) and
[development plan](planetary-development-plan.md) require multiple governed economic regions
on one planet, one homeworld region per player, founded once trading has paid for it
([D-110](decision-ledger.md#d-110)), alongside a separate trading company, regional
population/household/public accounts and AI neighbours. Leave enough regions open to founding
for the group and later joiners. Player-governed regions are protected from AI
acquisition; otherwise eligible AI neighbours on the same planet remain purchasable without
a sibling governor's veto. Treasury stays public and private asset owners retain ownership.
Frontier resource concessions cover eligible resources in one named region, leaving neighbours
independently available under their own eligibility rules and government control separate.
Port construction opportunities are regional; a port may serve several regions on that planet
under owner terms, with protected bookings, physical delivery and separate economies/rights.
one region's control is not whole-planet sovereignty. Later systems may extend the frontier
with less developed
or uninhabited planets for terraforming and settlement.

There is no win condition. Players seek ever greater corporate wealth, power and control
over trade, economies, governments and media. Planet development and expansion support that
open-ended objective. Server lifespan and pacing over days or months remain undecided.
First controls cover development projects, investment budgets and public procurement.
Public contracts require competitive corporate bids; simple government corruption and
abstract bribery can influence awards after basic tendering works. Detailed politics is deferred.
Corporate leverage begins with infrastructure, essential supplies and access contracts;
company-built stations may set fees, priorities and refuse rivals access to their services.
A very expensive corporate orbital spaceport may be built for a company's own, an eligible AI
or an eligible unclaimed region, with one initial opportunity per eligible region. Neighbouring
ports do not block it; other players' governed regions are protected against unsolicited builds.
Ports enable resource exploitation and colonies with company-controlled puppet governments.
Starting regions require an operational starter-compatible public station, a physical regional
delivery connection and continuing basic docking/trading/delivery access corporate players
cannot revoke. Charges and finite capacity/goods/funds still apply; private-port policies remain.
New players spawn connected to working station life support, requiring an available
connection at allocation. The starter receives 30 kg oxygen and one fresh installed
30-occupant-day cartridge once, in dedicated life-support space outside its 160-tote trade
hold. Trade supplies and loose spares use cargo space; dimensions and later outages remain open.
Exact colony powers, detailed shared-port transport and starter fees/capacity/funds remain planning work.
Stations should express
their cultures and corporate owners/operators through distinct customisable appearances,
corporate colours and logos; detailed visual design is deferred.
On frontier concessions, exploitation exclusivity is temporary and uses real elapsed
server-running time, pausing in shutdown. Afterwards rival exploitation/ports are allowed,
including under an incumbent puppet government; starting homes stay protected. This refines
the initial no-second-port rule.

Player-facing text ships in English, French, German and Japanese (the user's, 2026-11-22): every
string key in all four tables, partly to test the GUI against each one's hard case (German's long
words, French's diacritics, Japanese kanji), partly so the tables are ready at release for
interested native speakers to provide high-quality translations.

The [physical economy/development plan](economy-and-development-plan.md) records instant
credits, physically transported goods, finite principal brokers, spreads/warehouse charges,
production-limited supply and scarcity pricing. Regional tech requires resource exploitation,
imports and real production facilities. The inner map starts higher-tech with depleted
reserves; outer worlds are richer and less developed, driving a resource gold rush.
World persistence, governed development and influence mechanics are planned requirements,
not current capabilities. Frontier expansion and government/media mechanics follow the first
idle trading/development loop. Whole-civilisation sovereignty, combat, piracy, insurance claims
and server-downtime catch-up remain deferred; players' disconnects do not pause the running world.
