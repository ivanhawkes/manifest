#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>

namespace Manifest::Company
{
// The administrative command that provisions a player: their Principal, a company with the
// configured starting credits, the first free player's home region, governed by a new Polity
// organisation of theirs, and one ship of the scenario's starter hull, docked at that region's home
// venue with an empty hold. Rejected AlreadyProvisioned for a principal the world has, NoHomeRegion
// with none free, and MalformedPayload for an empty principal ID.
struct Provision
{
	std::string principalId;
	std::string displayName;
	std::string companyName;
};

// The players' commands that move goods between their company's stock at the venue a ship is docked
// at and the ship's hold, the one way and the other, neither changing their owner. The venue is the
// one the sender believes the ship is at. Each checks everything before it writes anything: the
// ship (UnknownTarget), that the sender's company owns it (NotAuthorised), that it is docked
// (NotDocked) at the venue (WrongVenue), the scenario's item (UnknownTarget), a count above 0
// (InvalidQuantity), enough available to move, reserved goods not counted (InsufficientStock), and
// room for them: the hull's hold on loading, the warehouse's kMaxWarehouseTotes on unloading, less
// what the company's resting buy orders there may still fill (CapacityExceeded).
struct Move
{
	std::string shipId;
	std::string venueId;
	std::string itemId;
	std::uint64_t totes{0};
};

inline constexpr std::string_view kProvision = "Manifest.Company.Provision";
inline constexpr std::string_view kLoad = "Manifest.Company.Load";
inline constexpr std::string_view kUnload = "Manifest.Company.Unload";
inline constexpr std::uint32_t kVersion = 1;

// The payloads, with Reflect, and the command types. Safe to call again.
void RegisterTypes();

// Registers the commands with the requests. The identities and the content, whose scenario they
// run on, must outlive the world. Throws std::invalid_argument for a content set with no scenario
// selected.
void RegisterCommands(
    Command::Requests &requests, World::Identities &identities, const Content::ContentSet &content);
} // namespace Manifest::Company
