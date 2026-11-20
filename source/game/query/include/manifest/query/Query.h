#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <jettison/sim/SimWorld.h>

#include <manifest/command/Request.h>
#include <manifest/world/Identities.h>
#include <manifest/world/State.h>

namespace Manifest::Query
{
struct ShipView
{
	std::string shipId;
	std::string hullId;
	// Empty when it is not docked.
	std::string dockedAt;
	std::vector<World::Lot> cargo;
	// Its latest journey, under way or ended; none before its first.
	std::optional<World::Journey> journey{};
	// The totes opened for its life support, and its tanks, cartridges and connection (D-094).
	std::vector<World::Opened> opened{};
	World::LifeSupport lifeSupport{};

	bool operator==(const ShipView &) const = default;
};

struct OrderView
{
	std::string orderId;
	World::Order order;

	bool operator==(const OrderView &) const = default;
};

struct CompanyView
{
	std::string companyId;
	std::string displayName;
	std::int64_t credits{0};
	// Its goods at every venue, by (venue, item).
	std::vector<World::Holding> holdings;
	// By ID.
	std::vector<ShipView> ships;
	// Its resting orders, oldest first.
	std::vector<OrderView> orders{};

	bool operator==(const CompanyView &) const = default;
};

// A player's report of a venue's market: live while a ship of their fleet is docked there, so
// observed at the snapshot's own step, and otherwise stale, its age the steps since it was observed.
struct MarketView
{
	World::Report report;
	bool live{false};

	bool operator==(const MarketView &) const = default;
};

// What an actor may see of the hosted world, copied out at a completed step: nothing in it refers
// back into the world, so a later step changes the next snapshot and never this one. Its markets are
// the player's own reports alone (docs/market-information-plan.md): a venue it has none of is unknown
// to them, and no other company's stock, credits or orders are in it.
struct Snapshot
{
	// The clock's step: how many steps have completed.
	std::uint64_t step{0};
	// By ID: a player's own company, none before provisioning; every company for the Administrator.
	std::vector<CompanyView> companies;
	// By venue ID, a player's; none for the Administrator, which has no fleet to observe with.
	std::vector<MarketView> markets{};

	bool operator==(const Snapshot &) const = default;
};

// The actor's snapshot of the world. Called between steps, never from a command's handler, so it
// reads only completed state. A command's result is the requests' Outcome, which is the sender's
// alone in the same way.
Snapshot Observe(
    const Jettison::Sim::SimWorld &world, const World::Identities &identities,
    const Command::Actor &actor);
} // namespace Manifest::Query
