#pragma once

#include <entt/entity/registry.hpp>

#include <cstdint>
#include <optional>
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

// The system, by the name the step's table gives it (docs/stage-4-trading-plan.md, "W10 plan"): after
// manifest.brokers and before the movement, each ship whose policy is enabled, in entity order, takes
// its job's next decisions from its owner's reports, through the players' own commands sent as its
// owner (Command::Requests::Act): choosing a trade, or a destination for the lot aboard; flying to the
// source; buying there at the live ask, the rest cancelled, and loading what it bought; flying to the
// destination; and selling there at no less than the lot's floor, the rest cancelled and loaded back.
// A job with nothing it can do blocks, keeping its cargo, until the next economic turn's last step.
inline constexpr std::string_view kSystem = "manifest.policy";

// The system after manifest.berths that seizes cargo for arrears (D-100): as each simulation day's
// last step ends, each ship whose World::Arrears began charges.seizureGraceDays or more before, in
// entity order, has its cargo sold through its owner's commands (Company::Unload, Trade::Place): lot
// by lot in hold order, its opened life-support totes kept, a tote at a time at the venue's live bids,
// until the proceeds cover what is owed or the bids run out, the rest cancelled and loaded back. Its
// company's account then pays the arrears as far as it holds and the venue's has room. Seized totes
// of the job's lot leave it with their shares of its cost and charges, as a sale's do, and a job left
// with no lot chooses again.
inline constexpr std::string_view kSeizureSystem = "manifest.seizure";

// The charges a route is estimated at (D-097, D-100), in whole credits, from each venue's World::Fees
// as posted now for the ship's fee class (Content::FeeClass): the destination's docking fee unless
// the ship is docked there, and the source's too when there is one and the ship must fly there first;
// and the life support of each leg flown, its hull's occupants for the leg's Space::CruiseSeconds
// between where the ship is, or the station it is docked at, and each station now, at the company's
// last prices (World::Paid), each leg rounded up. The ship is a World::Ship with a Sim::Transform.
std::int64_t Charges(
    const entt::registry &registry, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity ship, std::string_view sourceId,
    std::string_view destinationId);

// The lowest sell limit, raw microcredits per the item's unit, at which every sale of n of the lot's
// totes nets at least ceil(n * floor / totes), the whole floor for all of them, however the book
// splits it into up to Trade::kMaxFills fills at or above the limit: each fill's gross is rounded to
// the nearest credit and its fee up, so a fill nets at most 1.5 credits less than its price times its
// units, less the fee's rate. At least 1; none for no totes, a fee that takes the whole gross, or a
// limit the ledger cannot hold.
std::optional<std::int64_t> SellLimit(
    std::int64_t floor, std::uint64_t totes, double unitsPerTote, int feeBps);

// Provisions the scenario's NPC traders (Content::TraderDefinition), in its order, as the initial
// state's last part, after the brokers' first quotes, as a player is provisioned: through
// Company::Provision sent by the host, each principal then marked the world's own (World::Principal's
// npc), and its ship's policy enabled through Set sent as it, its target and allowance 0. Throws
// std::runtime_error for a trader the starter venue cannot take, and std::invalid_argument for a
// content set with no scenario selected.
void ProvisionTraders(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);

// The payload, with Reflect. Safe to call again.
void RegisterTypes();

// Registers the command with the requests, the system with the world, after manifest.brokers and
// before the movement Travel::Register attaches, and the seizure after manifest.berths and before
// manifest.observe, all of which must be registered, as must the trade and company commands they send.
// The world, the requests, the identities and the content must outlive the world's last step.
void Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);
} // namespace Manifest::Policy
