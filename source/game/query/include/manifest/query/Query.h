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

	// The totes its hold carries, its hull's with its systems' (Company::Upgrade), and the systems
	// its company has bought for it.
	std::uint64_t hold{0};
	std::vector<World::Fitting> systems{};

	// Its standing policy and its job (D-097), none before its owner first sets one.
	std::optional<World::Policy> policy{};
	std::optional<World::Job> job{};

	// Its fuel and the most it holds, its hull's with its systems' (D-107, D-108).
	World::Fuel fuel{};
	World::Fuel fuelTanks{};

	bool operator==(const ShipView &) const = default;
};

struct OrderView
{
	std::string orderId;
	World::Order order;

	bool operator==(const OrderView &) const = default;
};

struct BidView
{
	std::string bidId;
	World::Bid bid;

	bool operator==(const BidView &) const = default;
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

	// Its sealed bids, by ID, which no other player sees.
	std::vector<BidView> bids{};

	// The polities licensing it to buy their venues' licensed goods, in the order granted (W22b).
	std::vector<std::string> licences{};

	bool operator==(const CompanyView &) const = default;
};

// A tender's terms, as posted to every player (W12).
struct TenderView
{
	std::string tenderId;
	World::Tender tender;

	bool operator==(const TenderView &) const = default;
};

// A region's project as its governor sees it (D-110): its phase, work and place in the queue, and what
// it holds, by (venue, item).
struct ProjectView
{
	World::Project project;
	std::vector<World::Holding> stock;

	bool operator==(const ProjectView &) const = default;
};

// A region as every player sees it (D-110): its governor, none for one open to founding, whether a
// player directs it, and its condition. Its treasury, queue and development policy are its governor's:
// shown to the principal directing it and the Administrator, and otherwise empty.
struct RegionView
{
	std::string regionId;
	std::string planetId;
	std::string governorId;
	std::string governorName;
	bool playerGoverned{false};
	double population{0.0};
	double health{0.0};
	double happiness{0.0};
	std::optional<std::int64_t> treasury{};

	// Its projects in its queue's order, done and cancelled ones among them.
	std::vector<ProjectView> queue{};
	std::optional<World::Development> development{};

	bool operator==(const RegionView &) const = default;
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
// the player's own reports alone (docs/trading.md, "Market observations"): a venue it has none of is
// unknown to them, and no other company's stock, credits or orders are in it.
struct Snapshot
{
	// The clock's step: how many steps have completed.
	std::uint64_t step{0};

	// By ID: a player's own company, none before provisioning; every company for the Administrator.
	std::vector<CompanyView> companies;

	// By venue ID, a player's; none for the Administrator, which has no fleet to observe with.
	std::vector<MarketView> markets{};

	// Every tender the world holds, open and kept closed, by ID.
	std::vector<TenderView> tenders{};

	// Every region, by ID.
	std::vector<RegionView> regions{};

	bool operator==(const Snapshot &) const = default;
};

// The actor's snapshot of the world. Called between steps, never from a command's handler, so it
// reads only completed state. A command's result is the requests' Outcome, which is the sender's
// alone in the same way.
Snapshot Observe(
    const Jettison::Sim::SimWorld &world, const World::Identities &identities,
    const Command::Actor &actor);
} // namespace Manifest::Query
