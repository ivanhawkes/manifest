#pragma once

#include <cstdint>

namespace Manifest::Space
{
// Why lane travel, a cruise or a gate refuses a ship, or a gate lets one pass through unsent
// (Jettison::Sim::Reason), each keyed reason.space.<name> for the string tables that word it; the
// arguments, where there are any, follow. Registered with Reflect as "Space.Reason" by RegisterTypes.
enum class Reason : std::uint8_t
{
	NoSuchEntity,
	OnLane,
	InNoSystem,
	NoGalaxy,
	SystemNotInGalaxy,
	// The system's name and the jump point's.
	NoLane,
	// The system's name.
	ArrivalUndefined,
	NotController,
	AlreadyCruising,
	CannotCruise,
	SystemUndefined,
	// The system's name and the body's.
	NoSuchBody,
	// The station's name.
	NearStation,
	// The body's name.
	AlreadyThere,
	NoCourse,
	NotAGate,
	CannotTransit,
	// The gate's jump point, for each of these.
	NotAtGate,
	AlreadyCleared,
	AlreadyQueued,
	LeadsNowhere,
	NotCleared,
	// The gate's jump point, and its ring's radius in metres.
	HullTooLarge,
	// The body's name: a cruise whose drive still thrusts within a keepout once it is settled.
	InsideRadius,
	// A cruise's target ship that is the ship itself or not placed in its system.
	NoSuchTarget,
	// A cruise broken off of a ship not cruising.
	NotCruising
};

// Registers the module's types with Reflect: its reasons, lane travel's (Lanes.h), the gates'
// (Gates.h) and cruising's (Cruise.h), under the module's own names ("Space.OnLane"), and the AI's
// (Jettison::Ai::RegisterTypes). Idempotent; LaneTravel::Attach, Gates::Attach and CruiseTravel::Attach
// call it.
void RegisterTypes();
} // namespace Manifest::Space
