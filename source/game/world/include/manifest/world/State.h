#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <manifest/content/Definitions.h>

namespace Manifest::World
{
// What an entity of the hosted world is. The kind is part of its identity, so a company and a
// region that share a textual ID are two entities, and so are a region's public and household
// accounts, which both carry the region's ID.
enum class Kind
{
	Principal,
	Company,
	Organisation,
	Region,
	CompanyAccount,
	PublicAccount,
	HouseholdAccount,
	Ship,
	// A venue's broker and its account, each by the venue's ID (D-084).
	Broker,
	BrokerAccount,
	// What a venue takes in fees, by the venue's ID: its one credited balance.
	VenueAccount,
	Order
};

// Every hosted entity has one, given when it is created and never changed (Identities::Create).
struct Identity
{
	Kind kind{Kind::Principal};
	std::string id;
};

// A player's control identity in the world. Credentials and connections are not world state.
struct Principal
{
	std::string displayName;
};

struct Company
{
	std::string principalId;
	std::string displayName;
};

struct Organisation
{
	Content::OrganisationKind kind{Content::OrganisationKind::Polity};
	std::string displayName;
	// The player who directs it; empty for one the AI runs.
	std::string principalId;
};

// The one owner of a population. Its money is in its PublicAccount and HouseholdAccount.
struct Region
{
	std::string planetId;
	std::string civilisationId;
	// An Organisation; empty only for a player's home not yet allocated.
	std::string governorId;
	bool playerHome{false};
	double population{0.0};
	double health{0.0};
	double happiness{0.0};
};

// The owner is the account entity's Identity: its kind and the owner's ID.
struct Account
{
	// Whole credits.
	std::int64_t balance{0};
};

// A company's ship: its hull, a Manifest.Hull by ID, and the venue it is docked at, empty when it
// is not: set where provisioning parks it, cleared as a journey leaves its pad and set as one docks
// (Journey), and by nothing else.
struct Ship
{
	std::string companyId;
	std::string hullId;
	std::string dockedAt;

	bool operator==(const Ship &) const = default;
};

// How far a ship's journey has got (docs/stage-4-trading-plan.md, "Cargo capacity and physical
// travel"). Arrived and Failed are where it ends; each other leg is waited out on the engine's state.
enum class Leg
{
	// Its pad being let go of, or its place in a queue given up.
	Departing,
	// Flown out of the reach of the station it left, where a cruise does not engage.
	Clearing,
	Cruising,
	// Its pad asked for at the destination: waiting in its queue, or flown in and coming down.
	Arriving,
	Arrived,
	Failed
};

// Why a journey failed, its ship left where it is and its cargo aboard.
enum class Failure
{
	None,
	// The pad would not let the ship go.
	DepartureRefused,
	CruiseRefused,
	// The destination station had no pad the hull fits, or refused the ship.
	PadRefused,
	// The pad given, or the place in the queue, was lost before the ship docked.
	PadLost
};

// A ship's journey to a venue, from the Travel command that started it, on the ship's entity. One is
// under way until its leg is Arrived or Failed, which a new journey replaces.
struct Journey
{
	std::string venueId;
	Leg leg{Leg::Departing};
	Failure failure{Failure::None};
	// The clock's step the command ran at the start of.
	std::uint64_t started{0};

	bool operator==(const Journey &) const = default;

	bool Ended() const
	{
		return leg == Leg::Arrived || leg == Leg::Failed;
	}
};

// An owner's goods at one venue's warehouse, in totes (D-087), their total at most kMaxWarehouseTotes.
// Reserved totes back an order and cannot move; the total held is the sum.
struct Holding
{
	std::string venueId;
	std::string itemId;
	std::uint64_t available{0};
	std::uint64_t reserved{0};

	bool operator==(const Holding &) const = default;
};

// Every holding of the owner whose entity carries it, by (venue, item), none of them empty. The
// owner is the entity's Identity, so equal IDs of two kinds hold apart.
struct Stock
{
	std::vector<Holding> holdings;

	bool operator==(const Stock &) const = default;
};

// The totes of one item in a ship's hold, at most kMaxHoldTotes.
struct Lot
{
	std::string itemId;
	std::uint64_t totes{0};

	bool operator==(const Lot &) const = default;
};

// What a ship carries, by item, each lot above 0. Its owner's, at no venue.
struct Cargo
{
	std::vector<Lot> lots;

	bool operator==(const Cargo &) const = default;
};

// A venue's broker (D-084): a trading principal of the venue's operator, its ID the venue's, with
// its money in its BrokerAccount and its goods in its Stock.
struct Broker
{
	std::string operatorId;
};

enum class Side
{
	Buy,
	Sell
};

// An order resting in a venue's book, "Order<n>" by ID. A sell order's totes still to fill are
// reserved in its owner's Holding there; a buy order holds the credits it may still spend in its
// escrow, which goes back to its owner's account when the order leaves the book.
struct Order
{
	// A Company or a Broker, by ID; its Stock is on that entity and its Account is the kind's own.
	Kind ownerKind{Kind::Company};
	std::string ownerId;
	std::string venueId;
	std::string itemId;
	Side side{Side::Buy};
	// The worst price it trades at, a Core::Decimal's raw microcredits per the item's unit.
	std::int64_t limit{0};
	std::uint64_t totes{0};
	std::uint64_t remaining{0};
	// Whole credits, a buy order's alone.
	std::int64_t escrow{0};
	// The venue's listing fee as it was placed, which its escrow was reserved at.
	int feeBps{0};
	// Its place in time priority: the n of its ID.
	std::uint64_t sequence{0};
	// The step from which it no longer matches and leaves the book; 0 for none.
	std::uint64_t expiresStep{0};
	// The sequence of its owner's principal's request that placed it; 0 for a broker's.
	std::uint64_t request{0};

	bool operator==(const Order &) const = default;
};

// Why a Stock or a Cargo breaks the rules above, for a load to refuse: none when it keeps them.
std::optional<std::string> Broken(const Stock &stock);

std::optional<std::string> Broken(const Cargo &cargo);

std::optional<std::string> Broken(const Order &order);

// The components and their service's record, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::World
