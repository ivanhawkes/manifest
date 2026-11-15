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

The compact ship's primary purpose is unspecified, and trade is not its primary
requirement. It can carry some cargo, up to four IBC units, but is not the lone-trader
starter. Do not assign it a scout, courier or other specialised role without a later
product decision.

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

## Capability map and demonstrated gap

| Required mechanic | Existing public evidence | Owner and required work |
| --- | --- | --- |
| Distinct Manifest hulls and stats | `ShipClassSpec`, `ShipEffectEngine::BuildShip` and `LongHaulTrader` exist; the existing long-haul hull has `CargoVolume` 900 and `LifeSupportDays` 45 | Manifest defines the compact class's 4 m³ limit and one-occupant accommodation, and the separate lone-trader starter's 20 m³ hold. Existing long-haul values are not their defaults. Load hosted content through the planned Jettison definition/archive pipeline, not another custom loader. |
| Physical hull and docking | Pinned `Dock::HullDefinition` provides dimensions, hatch, model path, pad class and cruise data; `Docks::DefineHull`, `DefineLayout`, `Park`, `RequestPad` and `IsDocked` provide public integration points | Manifest supplies hull geometry, station layout and gameplay policy using public SDK APIs. Art and collision dimensions must agree with the authored definition. |
| Smaller berth geometry | Pinned `PadClass` is `S/M/L/XL`; `PadSize(S)` is fixed at 35 × 40 m and `LargestHull(S)` at 25 × 20 × 8 m. `PadLayout` selects a class without per-pad dimensions | Demonstrated SDK gap for a berth smaller than the fixed S pad. The implementer provisions a reusable smaller-pad capability in Jettison, with fit/clearance/validation and compatibility coverage. Manifest owns this ship's size and station choices. An added class versus configurable geometry remains an API design decision. |
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
2. Provision and validate the public smaller-berth capability in Jettison. Preserve existing
   authored class identifiers and saved meanings; do not renumber existing enum/save values
   accidentally. Retain existing pad behaviour and exercise the new capability in a separate
   engine fixture without Manifest content. Resolve use of larger compatible berths explicitly.
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
5. Demonstrate berth fit, oversized-hull rejection, approach/clearance, queue/occupancy,
   docking, cargo transfer and departure. Verify station market coverage only on confirmed
   docking, and the selected observation boundary when departing or changing ownership.
6. Verify definition directory/pack equivalence and saved class/layout references, cargo,
   docking and travel through load/replay. The implementer runs relevant presets and the
   required local gate with two compile jobs; local clang-tidy stays opt-in.

This plan establishes requirements and a public API gap, not build validity, playable
integration, economic viability, endurance or performance evidence.
