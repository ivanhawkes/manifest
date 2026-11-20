#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>
#include <manifest/world/State.h>

namespace Manifest::Trade
{
// A player's limit order for their company at a venue, in totes at a price per the item's unit
// (docs/stage-4-trading-plan.md, "Order and settlement contract"). It reserves, then matches at once
// against the other side of the venue's book, best price first and oldest first within a price, the
// resting order setting the price, for at most kMaxFills fills; what is left rests as "Order<n>"
// until it fills, is cancelled or expires. Everything is checked and planned before anything is
// written: the sender's company (NotAuthorised), the venue and the scenario's item (UnknownTarget),
// totes above 0 and up to a warehouse's (InvalidQuantity), a limit above 0 whose reservation the
// ledger holds (InvalidPrice), for a sell the totes available at the venue (InsufficientStock), for a
// buy the credits its reservation takes (InsufficientFunds) and the warehouse's room for all it may
// fill (CapacityExceeded), every balance a fill pays into (InvalidQuantity), and room on its side of
// the book for what is left (BookFull). Too little on the other side is a success that rests: what
// filled is the order's totes less its remaining, found by the request's sequence (Order::request),
// and none rests when the whole filled.
struct Place
{
	std::string venueId;
	std::string itemId;
	World::Side side{World::Side::Buy};
	std::uint64_t totes{0};
	// A Core::Decimal's raw microcredits per the item's unit.
	std::int64_t limit{0};
};

// Takes the sender's company's resting order off the book and gives back what it reserves: one the
// book does not hold (UnknownTarget) or someone else's (NotAuthorised) is refused.
struct Cancel
{
	std::string orderId;
};

inline constexpr std::string_view kPlace = "Manifest.Trade.Place";
inline constexpr std::string_view kCancel = "Manifest.Trade.Cancel";
inline constexpr std::uint32_t kVersion = 1;

// One matching pass settles at most this many fills, and one side of a venue's book for one item
// holds at most this many resting orders, so a pass can settle every maker it crosses (D-085).
inline constexpr std::size_t kMaxFills = 64;
inline constexpr std::size_t kMaxRestingOrders = 64;

// The systems, by the names the step's table gives them.
inline constexpr std::string_view kExpiry = "manifest.expiry";
inline constexpr std::string_view kBrokers = "manifest.brokers";

// The payloads, with Reflect. Safe to call again.
void RegisterTypes();

// Registers the commands with the requests and the systems with the world: each step the expiry
// sweep, retiring in placement order every order whose expiresStep has come, then the regional economy
// (kEconomy, manifest/trade/Economy.h), then on the last step of each economic turn every broker's
// requote (Quote), all ahead of the movement Travel::Register attaches, which must be attached. The
// identities and the content must outlive the world. Throws std::invalid_argument for a content set
// with no scenario selected, or an order life or economic interval that is not a whole number of the
// world's steps.
void Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);

// Every broker's requote (D-084), venue by venue in the scenario's order: its resting orders
// cancelled, then for each good of the venue's market in its order, half the spread either side of
// the native price, an ask for all it holds and a bid for what its share of its funds covers, an
// equal share for each good still to quote. Each goes through the path a player's order takes, so it
// may fill, and a side it cannot fund or rest posts nothing. The initial state's, and each turn's.
// Throws as Register does.
void Quote(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content);
} // namespace Manifest::Trade
