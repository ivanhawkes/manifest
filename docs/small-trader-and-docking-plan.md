# Compact ship, matching dock and lone-trader starter

Selected product requirements, 2026-11-15. This is planning documentation, not an
implemented ship, station layout or gameplay feature. Manifest inspection used `feature-base`
at `4e78dbb` with planning edits; public docking APIs were checked at SDK pin `f850920`.
No code, content assets, SDK pin or environment changed; no builds or gameplay checks ran.

## Selected ship roles and cargo capacities

The starter ship is the entry point for a lone trader and has a cargo hold of 20 IBC units
(20 m³ under the existing unit contract). It is distinct from the new compact four-IBC
ship below. Twenty IBC units is the selected starting hold capacity; upgrade rules and
other starter specifications remain open.

The subsequently selected initial starter supply is 30 kg of oxygen and one fresh
installed standard scrubber cartridge, providing 30 occupant-days of scrubbing capacity.
At one occupant, the selected onboard rates use both reserves over 30 simulation days.
This is oxygen/scrubbing coverage, not a complete journey-endurance guarantee. Actual
use pauses while connected to working station life support; new players spawn under
that service. These are initial onboarding stocks, not refills on reconnect, docking,
ownership transfer or recovery. Preserve actual remaining supplies thereafter.

The selected starter storage rule places operating oxygen and the installed cartridge in
dedicated life-support space outside the 20-IBC trade cargo hold. Supplies carried for
trade or as loose spares occupy cargo space under the ordinary packed-volume rules.
The hull must physically accommodate both spaces without counting the same stock twice.
Moving goods from cargo into operating reserves must conserve actual stock and capacity;
the transfer/replacement procedure still needs design.

The starter's operating oxygen bank is modular under
[D-050](decision-ledger.md#d-050), detailed in the canonical section below. Cargo oxygen
remains separate until transferred and cannot increase installed capacity. Physical tank
volume/storage method, compartment dimensions, cartridge physical mass/volume, spare
provision, upgrades and other journey limits remain open. This decision does not set
the compact ship's oxygen capacity or initial reserves.

The compact ship's primary purpose is unspecified, and trade is not its primary
requirement. It can carry some cargo, up to four IBC units, but is not the lone-trader
starter. Do not assign it a scout, courier or other specialised role without a later
product decision.

## Modular oxygen storage

Canonical specification for [D-050](decision-ledger.md#d-050), refining D-045's starter
provisioning without changing its selected total oxygen reserve or cargo separation.
The user replaced the single-tank interpretation with independently serviceable tanks.

| Starter specification | User-selected design target |
| --- | --- |
| Installed oxygen tanks | 3 |
| Oxygen capacity per tank | 10 kg |
| Total installed operating oxygen capacity | Derived from installed tank capacities |
| Initial fill | Every installed tank full |

These are pending authoring targets for handoff T4, not loaded configuration. Under the
planning-only policy, the implementer owns their authored data and validation.

Each tank can be refilled or replaced independently before the bank is empty. Remaining
usable tanks can continue supplying life support while another is serviced; neither the
whole bank nor the tank being topped up has to be fully depleted first. Track capacity
and actual remaining oxygen per tank, with the bank total derived from those records.
An aggregate reserve alone is insufficient to represent changeover and partial refills.
Replacement cannot silently replenish or discard oxygen remaining in the removed tank;
define its destination and ownership with the eventual exchange procedure.

Larger ships use the same modular arrangement with proportionally greater installed
oxygen storage. Per-hull tank counts/capacities and the scaling basis remain open; do not
infer them from cargo volume or assign every hull the starter's tank sizes. The compact
ship's capacity and initial provisioning remain open. Tank/container definitions, dry
mass, packed volume, consumption order, isolation/changeover, access, transfer duration,
automation and in-flight servicing conditions also remain open.

Delivery is selected but deferred: revisit after the first playable loop's stages 1–4
acceptance criteria pass. This requirement does not displace Stage 1. Manifest owns hull
definitions, tank state, consumption and servicing rules. Public pinned SDK
`SimWorld::RegisterComponent`, `RegisterSystem`, `Save`, `Load` and `StateHash` provide
generic state/system/persistence facilities; use Jettison's existing definition/archive
pipeline for authoring. No additional generic SDK gap has been demonstrated for tank
accounting. The current checkout's `docs/game-integration.md` supplies boundary guidance;
that document is absent from the selected SDK commit. API provision remains conditional
on a demonstrated gap during integration.

Implementer acceptance evidence:

- Validate the authored tank bank against the selected targets above, deriving its total
  from individual tanks and preserving dedicated service storage versus cargo stock.
- Top up a partly filled tank and replace an eligible tank while other tanks retain
  oxygen, without requiring complete bank depletion or interrupting those usable tanks.
- Preserve oxygen mass, ownership and tank identity through consumption, partial refill,
  exchange, save/load and retry; reject over-capacity transfers without losing source stock.
- Demonstrate independently authored larger-ship banks under the selected scaling rule
  once that rule is specified, without cloning the starter's limits onto every hull.

Tank rules, data and tests remain unimplemented. No runtime or gate result is claimed.

## Compact ship and matching dock requirements

Add a small ship class with room for one occupant on a long journey. Its accommodation
should resemble the tiny living cabin of a modern truck: compact living/sleeping space
for its sole occupant, rather than a large crew compartment. Exact interior arrangement,
dimensions and journey endurance remain to be designed. This requirement does not itself
select individual crew simulation or a walkable interior.

The ship carries at most four IBC units of cargo. Manifest's existing [unit contract](model.md)
defines an IBC cube as one cubic metre, so its cargo capacity is 4 m³. Cabin, life-support,
propulsion and other ship equipment need their own space outside that usable cargo volume.
Cargo mass depends on the goods; four IBC units do not mean four tonnes or four units of
every commodity. Preserve fractional commodity quantities and the shared packed-volume
conversion in the [physical trading plan](stage-4-trading-plan.md#cargo-capacity-and-physical-travel).
Literal indivisible container objects are not selected by this requirement.

Add a small dock/berth sized for this class. It must be physically appropriate for the
compact hull and its approach, clearance and cargo access, rather than just relabelling a
large pad. An oversized hull must not be admitted to the smaller berth. Existing station
access, booking and ownership rules still apply; the berth is not a separate market merely
because it has a new size.

The four-IBC limit is a defining requirement of this class. Existing cargo effects must
not silently turn it into a larger freighter; resolve allowable fittings/upgrades against
that limit before implementation. The class can provide the normal fleet market observation
when docked, with no additional remote-data privilege. Its price and whether it is
especially cheap to operate have not been selected. The 20-IBC starter's berth requirements
must be resolved from its own hull dimensions; cargo capacity alone does not establish
that it fits the compact ship's dock.

## Ship size classes and the resupply platform

Settled by the user on 2026-11-18, in Jettison's pre-review (finding F10). Manifest's pad classes
are named after historical ship classifications, largest to smallest: heavy destroyer,
dreadnought, cruiser, destroyer, frigate, corvette, skiff. "Heavy destroyer" is a name made up for
the game, and the spelling is British ("dreadnought"). The names label sizes, not historical
dimensions. Each is a `Dock::PadClassDefinition` by its kebab-case name (`heavy-destroyer`).

| Class | Rank | Largest hull, L x B x H (m) | Pad, W x L (m) | Docks at |
| --- | --- | --- | --- | --- |
| skiff | 0 | set by the compact ship above | 8 x 10 | a standard station |
| corvette | 1 | 25 x 20 x 8 | 35 x 40 | a standard station |
| frigate | 2 | 60 x 30 x 15 | 50 x 90 | a standard station |
| destroyer | 3 | 140 x 50 x 30 | 80 x 180 | a standard station, the largest it takes |
| cruiser | 4 | 300 x 80 x 60 | 120 x 360 | a dry dock, the smaller size |
| dreadnought | 5 | 450 x 120 x 90 (proposed) | 180 x 540 (proposed) | the heavy destroyer's dry dock |
| heavy-destroyer | 6 | 600 x 160 x 120 (proposed) | 240 x 720 (proposed) | a dry dock, the largest size |

Corvette to cruiser are the Jettison sample's four classes at its sizes. The two largest rows are
proposals, not selected values. A dreadnought docks because its class ranks below the heavy
destroyer's and its largest hull fits that pad (Jettison's `Docks::Fits` checks both).

Dry docks come in two sizes, the cruiser's and the heavy destroyer's. A dry dock is not a naval
one: it is a large resupply platform, shaped like an oil rig hung in space, with crew and
technicians living aboard (it supports 500 people long term), storage hangars, and shuttle pads
with shuttles on hand. Its layout has the two large pads and a range of smaller pads, skiff to
destroyer, and every pad offers dry dock (a feature by name on the pad, `Dock::PadLayout::features`,
in Jettison and in pin `e53681e`). Smaller craft may use it. The engine's layout, pads, bays and parked
ships express it with no engine edit; the people, what the hangars hold, what resupply and repair
do and cost, and which systems have a platform are Manifest's, and not yet designed.

Not yet authored: these definitions wait for the hosted content pipeline (C02 to C04) and the
compact berth (W05).

## Capability map and demonstrated gap

| Required mechanic | Existing public evidence | Owner and required work |
| --- | --- | --- |
| Distinct Manifest hulls and stats | `ShipClassSpec`, `ShipEffectEngine::BuildShip` and `LongHaulTrader` exist; the existing long-haul hull has `CargoVolume` 900 and `LifeSupportDays` 45 | Manifest defines the compact class's 4 m³ limit and one-occupant accommodation, and the separate lone-trader starter's 20 m³ hold. Existing long-haul values are not their defaults. Load hosted content through the planned Jettison definition/archive pipeline, not another custom loader. |
| Physical hull and docking | Pinned `Dock::HullDefinition` provides dimensions, hatch, model path, pad class and cruise data; `Docks::DefineHull`, `DefineLayout`, `Park`, `RequestPad` and `IsDocked` provide public integration points | Manifest supplies hull geometry, station layout and gameplay policy using public SDK APIs. Art and collision dimensions must agree with the authored definition. |
| Smaller berth geometry | At `f850920`, `PadClass` was a closed `S/M/L/XL` enumeration. Provisioned in Jettison W01 (pin `fafa758`): a pad class is a definition, `Dock::PadClassDefinition`. See [D-044](decision-ledger.md) and Jettison's [history](../../jettison/docs/history.md#neutral-sdk-and-manifest-integration) | Manifest authors its compact berth and the hulls' classes as data, and owns this ship's size and station choices. |
| Cargo, permissions and recovery | Existing effects expose `CargoVolume`; hosted ownership, packed-volume transfer, travel and persistence remain planned in stages 3–5 | Manifest enforces one authoritative cargo inventory with distinct 4 m³ compact and 20 m³ starter capacities across loading, trading, effects and recovery. Resolve starter upgrades separately. Do not use integer `Dock::Carry` crates as a second inventory or infer an unauthored mass limit. |

Evidence: `source/game/effects/include/manifest/effects/ShipEffectEngine.h`,
`source/game/effects/src/ShipEffectEngine.cpp`,
`source/game/ecs/include/manifest/ecs/Components.h`; pinned Jettison
`source/engine/dock/include/jettison/dock/Definitions.h` and `Docks.h`.
The generic engine change must not hard-code Manifest's four-IBC rule or cabin design.
Commit any required engine provision before updating Manifest's immutable SDK pin and
matching environment. No SDK change has been made by this planning work.

## Implementer handoff and acceptance evidence

1. Select hull dimensions around the single-occupant long-journey accommodation, equipment
   and four-IBC cargo space; select compatible berth dimensions and safety clearances.
   Define endurance, fittings, price and operating costs separately. Define the lone-trader
   starter with its 20-IBC hold and resolve its own hull dimensions, berth fit and upgrades.
   The compact ship's primary purpose remains unspecified.
2. Done in Jettison (W01, 2026-11-18): pad classes are data. A hull uses a larger compatible
   berth by rank (`Docks::Fits`). Manifest decides which of its classes a station offers.
3. Author Manifest definitions through the stage 2 registry/VFS/archive path. Keep the
   existing long-haul hull and authored IDs stable. Add the new hull/layout references to
   appropriate scenarios without assuming every station receives the new berth.
4. Demonstrate legal loads totalling 4 m³, rejection above capacity, mixed goods, partial
   loads and packing conversions without duplicated cargo or credit changes on rejection.
   Show that fittings/technology cannot silently exceed the class limit. Cabin/equipment
   accommodation must not consume the same space counted as usable cargo. Separately show
   a new lone trader receiving the 20-IBC starter, legal starter loads totalling 20 m³ and
   rejection above its current capacity. Verify that onboarding selects the starter hull,
   not the compact hull or existing 900 m³ long-haul default.
   Verify initial starter oxygen of 30 kg and one fresh installed cartridge with 30
   occupant-days, one-time provisioning and retained partial reserves through retries,
   recovery and transfers. At one occupant relying on onboard service, each supply covers
   30 simulation days; station service pauses depletion without replenishment. Validate
   physical accommodation once storage choices are authored, enforcing the starter's
   selected operating oxygen bank specification in D-050. Show all initial tanks are full,
   partial tanks retain their actual mass and cargo oxygen cannot bypass installed limits.
   Demonstrate the starter's operating reserves and installed cartridge do not reduce its
   20 m³ cargo capacity; cargo supplies/spares do occupy their authored packed volume.
   Transfers into service storage must debit their source once, preserve ownership and
   reject destination overflow without duplicating or losing stock. Preserve location and
   remaining capacity through retry/recovery once transfer rules and other storage limits are set.
5. Demonstrate berth fit, oversized-hull rejection, approach/clearance, queue/occupancy,
   docking, cargo transfer and departure. Verify station market coverage only on confirmed
   docking, and the selected observation boundary when departing or changing ownership.
6. Verify definition directory/pack equivalence and saved class/layout references, cargo,
   docking and travel through load/replay. The implementer runs relevant presets and the
   required local gate at the machine's concurrency ([D-051](decision-ledger.md#d-051)); local
   clang-tidy stays opt-in.

This plan establishes requirements and a public API gap, not build validity, playable
integration, economic viability, endurance or performance evidence.
