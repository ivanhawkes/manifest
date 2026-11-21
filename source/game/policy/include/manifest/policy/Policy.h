#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>

namespace Manifest::Policy
{
// The players' command that sets their company's ship's standing trade policy (D-097), which its job
// reads at its next decision, a lot aboard included; allowance the lot has spent stays spent. A ship
// with no job is given one, choosing. Refused, writing nothing, for a ship the world does not have
// (UnknownTarget), one of another company (NotAuthorised), and a target or an allowance below 0
// (InvalidQuantity).
struct Set
{
	std::string shipId;
	bool enabled{false};
	std::int64_t targetProfit{0};
	std::int64_t lossAllowance{0};
};

inline constexpr std::string_view kSet = "Manifest.Policy.Set";
inline constexpr std::uint32_t kVersion = 1;

// The payload, with Reflect. Safe to call again.
void RegisterTypes();

// Registers the command with the requests. The identities must outlive the world.
void Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);
} // namespace Manifest::Policy
