#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>
#include <manifest/world/State.h>

namespace Manifest::Trade
{
// A player's limit order for their company at a venue, in totes at a price per the item's unit
// (docs/trading.md, "How it works"). It reserves, then matches at once against the other side of the
// venue's book, best price first and oldest first within a price, the resting order setting the price,
// for at most kMaxFills fills; what is left rests as "Order<n>" until it fills, is cancelled or
// expires. Everything is checked and planned before anything is written: the sender's company
// (NotAuthorised), the venue and the scenario's item (UnknownTarget), totes above 0 and up to a
// warehouse's (InvalidQuantity), a limit above 0 whose reservation the ledger holds (InvalidPrice), for
// a sell the totes available at the venue (InsufficientStock), for a buy the credits its reservation
// takes (InsufficientFunds) and the warehouse's room for all it may fill (CapacityExceeded), every
// balance a fill pays into (InvalidQuantity), and room on its side of the book for what is left
// (BookFull). Too little on the other side is a success that rests: what filled is the order's totes
// less its remaining, found by the request's sequence (Order::request), and none rests when the whole
// filled.
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

// Raised (SimWorld::Events) as whole totes leave the world: a household's opened into its pantry, a
// project's inputs as its work starts, a hold's opened totes used up for its ship's life support or
// magazine, and reaction mass bought straight into a ship's tanks. Presentation's, and a ledger's that
// counts the world's goods; not state.
struct Consumed
{
	std::string itemId;
	std::uint64_t totes{0};
};

inline constexpr std::string_view kPlace = "Manifest.Trade.Place";
inline constexpr std::string_view kCancel = "Manifest.Trade.Cancel";
inline constexpr std::uint32_t kVersion = 1;

// One matching pass settles at most this many fills, and one side of a venue's book for one item
// holds at most this many resting orders, so a pass can settle every maker it crosses (D-085).
inline constexpr std::size_t kMaxFills = 64;
inline constexpr std::size_t kMaxRestingOrders = 64;

// A broker posts at most this many orders a side for each good, its ladder's rungs (D-102).
inline constexpr std::uint64_t kLevels = 4;

// The systems, by the names the step's table gives them.
inline constexpr std::string_view kExpiry = "manifest.expiry";
inline constexpr std::string_view kBrokers = "manifest.brokers";

// After manifest.travel, at the completed step's boundary, it refreshes each player's report of every
// venue a ship of their fleet is docked at (Ship::dockedAt), observed at the step the clock reaches as
// the step ends; a report no ship refreshes stays as it was (docs/trading.md, "Market observations").
inline constexpr std::string_view kObserve = "manifest.observe";

// The market price rules (docs/pricing.md, "From a price to credits"), in whole credits, none for an
// amount the ledger cannot hold. Fee is a gross's listing fee at the rate in basis points, rounded up;
// Cost a count of an item's units at a price, raw microcredits per unit, rounded up or to the nearest,
// as a fill's gross is; and PerTote what a buy order at the limit holds in escrow for each tote, a
// tote's cost rounded up and its fee.
std::optional<std::int64_t> Fee(std::int64_t gross, int feeBps);

std::optional<std::int64_t> Cost(std::int64_t price, double units, bool roundUp);

std::optional<std::int64_t> PerTote(std::int64_t limit, double unitsPerTote, int feeBps);

// The payloads, with Reflect. Safe to call again.
void RegisterTypes();

// The content's sales window in the world's steps. Throws std::invalid_argument for one that is not
// a whole number of them above 0.
std::uint64_t SalesWindowSteps(
    const Jettison::Sim::SimWorld &world, const Content::ContentSet &content);

// The venues a new player starting at the venue is given starting reports of (D-095): every other
// venue of its system and every venue of a system directly connected to it, either way, in the
// scenario's order. None beyond those: a neighbour's neighbour stays unknown.
std::vector<std::string> StartingVenues(
    const Content::ScenarioDefinition &scenario, std::string_view venueId);

// The report of the venue's market as the world holds it now, observed at the step: the venue's goods
// in its market's order, each side's best World::kReportLevels prices that still match after that step
// and the totes resting at each, the totes sold
// since the window began and its last sale. The world must hold the venue's Market.
World::Report Capture(
    const entt::registry &registry, const World::Identities &identities,
    const Content::VenueDefinition &venue, std::uint64_t observed, std::uint64_t windowSteps);

// Gives the principal, provisioned this step, their starting reports (StartingVenues of the scenario's
// starter venue), each observed at the clock's step, as their markets stand at their spawn, and stale
// from the first, since a starting report has no feed. Once, as the principal is made: nothing else
// reseeds them.
void Seed(
    Jettison::Sim::SimWorld &world, const World::Identities &identities,
    const Content::ContentSet &content, entt::entity principal, std::uint64_t windowSteps);

// Registers the commands with the requests and the systems with the world: each step the expiry
// sweep, retiring in placement order every order whose expiresStep has come, then the regional economy
// (kEconomy, manifest/trade/Economy.h), then on the last step of each economic turn every broker's
// requote (Quote), all ahead of the movement Travel::Register attaches, which must be attached, and
// after it the observation (kObserve). Every fill is recorded in its venue's Sales. The identities and
// the content must outlive the world. Throws std::invalid_argument for a content set with no scenario
// selected, or an order life, economic interval or sales window that is not a whole number of the
// world's steps.
void Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);

// Buys for the company at the venue at the asks resting on its book now, best price first and then
// oldest, as many of the totes as the budget, in whole credits, holds the reservation of a buy at the
// limit of the dearest ask it takes (PerTote): a buy that rests nothing, its fills in the company's
// holding there, recorded as any fill is. Gives the totes bought, none for a venue or item the
// scenario does not trade or nothing on offer. A ship's refuelling (W21). Made from the content once,
// as Register's rules are; throws as Register does.
using Buyer = std::function<std::uint64_t(
    Jettison::Sim::SimWorld &, const std::string &companyId, const std::string &venueId,
    const std::string &itemId, std::uint64_t totes, std::int64_t budget)>;

Buyer MakeBuyer(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content);

// Every broker's requote (D-084), venue by venue in the scenario's order: its resting orders
// cancelled, then for each good of the venue's market in its order, half the spread either side of
// the native price, an ask for all it holds and a bid for what its share of its funds covers, an
// equal share for each good still to quote. Each goes through the path a player's order takes, so it
// may fill, and a side it cannot fund or rest posts nothing. The initial state's, and each turn's.
// Throws as Register does.
void Quote(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content);

// ─── Prices by need, for W12's tenders (D-102) ───
// A need's pressure past this calls a relief tender, and the relief's urgency is its pressure times
// kReliefUrgency.
inline constexpr double kReliefPressure = 0.25;
inline constexpr double kReliefUrgency = 3.0;

// A tender's budget in whole credits: the units at the price, raw microcredits per unit, rounded up as
// a buy's cost is, times one and the urgency, rounded up. None for an urgency below 0 or a budget the
// ledger cannot hold.
std::optional<std::int64_t> Budget(std::int64_t price, double units, double urgency);

// What a tender at the venue prices the item at, raw microcredits per its unit: the lowest ask
// resting on the venue's book, or its broker's native price where nothing is offered. None for an
// item the venue's market does not price. Throws as Register does.
std::optional<std::int64_t> TenderPrice(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content,
    std::string_view venueId, std::string_view itemId);

// A relief tender a region's government calls for an essential its households are short of.
struct Relief
{
	std::string regionId;
	std::string needId;

	// The region's first venue, where its households keep their goods, and the cheapest of the need's
	// goods there by TenderPrice, the first in the need's order of those as cheap.
	std::string venueId;
	std::string itemId;

	// The day's shortfall, a day's requirement times the pressure, in whole totes rounded up.
	std::uint64_t totes{0};
	std::int64_t budget{0};
};

// Each relief tender called for now, by region in the scenario's order and need in the tuning's: one
// for each essential need whose pressure is past kReliefPressure, budgeted at its pressure times
// kReliefUrgency. None for a need whose goods its venue does not price, or whose budget the ledger
// cannot hold. Throws as Register does.
std::vector<Relief> Reliefs(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content);

// TenderPrice and Reliefs on the market's rules made once, as MakeBuyer's are, for a caller that asks
// them every turn (W12's issuer). Throws as Register does.
struct Pricer
{
	std::function<std::optional<std::int64_t>(
	    Jettison::Sim::SimWorld &, std::string_view venueId, std::string_view itemId)>
	    tenderPrice;
	std::function<std::vector<Relief>(Jettison::Sim::SimWorld &)> reliefs;
};

Pricer MakePricer(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content);
} // namespace Manifest::Trade
