# Scope

Manifest supplies the civilisation, progression and trading model for a Jettison game. The
current scope is the C++ development harness, its terminal interface, authored YAML and mod
content packaging. Jettison owns rendering, input, audio, physics, engine scheduling, VFS,
serialisation and the sandboxed script runtime.

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
trading fixture. The [planetary development plan](planetary-development-plan.md) adds one
governed home planet per player alongside a separate trading company and public budget,
AI neighbours, and enough planets for the intended group and later joiners. Players may
purchase eligible AI planets or settle frontier worlds; other players' planets are protected
from this acquisition mechanic. Later systems may extend the frontier with less developed
or uninhabited planets for terraforming and settlement.

There is no win condition. Players seek ever greater corporate wealth, power and control
over trade, economies, governments and media. Planet development and expansion support that
open-ended objective. Server lifespan and pacing over days or months remain undecided.
First controls cover development projects, investment budgets and public procurement.
Public contracts require competitive corporate bids; simple government corruption and
abstract bribery can influence awards after basic tendering works. Detailed politics is deferred.
Corporate leverage begins with infrastructure, essential supplies and access contracts;
company-built stations may set fees, priorities and refuse rivals access to their services.
A very expensive corporate orbital spaceport may be built around a planet with no port that
is not owned/controlled by another player, enabling resource exploitation and colonies with
company-controlled puppet governments. Exact resource/colony rights remain planning work.
On frontier concessions, exploitation exclusivity is temporary and uses real elapsed
server-running time, pausing in shutdown. Afterwards rival exploitation/ports are allowed,
including under an incumbent puppet government; starting homes stay protected. This refines
the initial no-second-port rule.

The [physical economy/development plan](economy-and-development-plan.md) records instant
credits, physically transported goods, finite principal brokers, spreads/warehouse charges,
production-limited supply and scarcity pricing. Planetary tech requires resource exploitation,
imports and real production facilities. The inner map starts higher-tech with depleted
reserves; outer worlds are richer and less developed, driving a resource gold rush.
World persistence, governed development and influence mechanics are planned requirements,
not current capabilities. Frontier expansion and government/media mechanics follow the first
idle trading/development loop. Whole-civilisation sovereignty, combat, piracy, insurance claims
and server-downtime catch-up remain deferred; players' disconnects do not pause the running world.
