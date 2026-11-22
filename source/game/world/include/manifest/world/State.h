#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
	Order,
	// A region's employer and its account, and its households' goods, each by the region's ID (W04):
	// the households' money is the region's HouseholdAccount.
	Employer,
	EmployerAccount,
	Household,
	// A venue's market, by the venue's ID: its Sales (W07).
	Market
};

// The account of an owner of goods: a Company's, a Broker's, an Employer's or a Household's.
Kind AccountOf(Kind owner);

// Every hosted entity has one, given when it is created and never changed (Identities::Create).
struct Identity
{
	Kind kind{Kind::Principal};
	std::string id;
};

// A player's control identity in the world. Credentials and connections are not world state. An NPC's
// is the world's own (D-097): its jobs act for it, and no client may connect as it.
struct Principal
{
	std::string displayName;
	bool npc{false};
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
	// An Organisation; empty for one no one governs yet.
	std::string governorId;
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

// What is left of an opened tote, in the item's unit: the households', or a ship's for its life
// support.
struct Opened
{
	std::string itemId;
	double quantity{0.0};

	bool operator==(const Opened &) const = default;
};

// The totes of one item in a ship's hold, at most kMaxHoldTotes.
struct Lot
{
	std::string itemId;
	std::uint64_t totes{0};

	bool operator==(const Lot &) const = default;
};

// What a ship carries, by item, each lot above 0, and the totes opened for its life support (D-094),
// by item, at most one an item, each still taking its whole tote of the hold until it is used up and
// none sold or unloaded. Its owner's, at no venue.
struct Cargo
{
	std::vector<Lot> lots;
	std::vector<Opened> opened{};

	bool operator==(const Cargo &) const = default;
};

// A tier of a ship system its company bought for it (W08), by its Skill node's ID.
struct Fitting
{
	std::string skillId;
	int tier{0};

	bool operator==(const Fitting &) const = default;
};

// The ship systems its company has bought for a ship, by skill ID, each at a tier of at least 1: the
// company's own progression, apart from any civilisation's technology. A ship that has bought none has
// no Systems.
struct Systems
{
	std::vector<Fitting> fitted;

	bool operator==(const Systems &) const = default;
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

// A batch of an employer's production (docs/economy-and-development-plan.md, "Production-batch payroll
// funding"): whole totes of one good made at a venue over its duration of work, its gross wage bill
// reserved from the employer's account as it started and paid as the work is done.
struct Batch
{
	std::string venueId;
	std::string itemId;
	std::uint64_t totes{0};
	// Simulation seconds of work at the full rate, and those done, the rate scaled by its region's
	// condition (WorkRate).
	double duration{0.0};
	double progress{0.0};
	// Whole credits: the gross bill, and what of it has been paid. The rest is reserved.
	std::int64_t wages{0};
	std::int64_t paid{0};

	bool operator==(const Batch &) const = default;
};

// An employer's batches under way, at most one a market row it makes, in (venue, item) order; and its
// withholding's carry, the gross paid toward the next withheld credit, shared by its batches.
struct Production
{
	std::vector<Batch> batches;
	std::int64_t taxCarry{0};

	bool operator==(const Production &) const = default;
};

// What a need has gone without, in the need's unit, over every interval.
struct Unmet
{
	std::string needId;
	double quantity{0.0};
	// Its unmet share smoothed over an hour, 0 to 1 (D-102).
	double pressure{0.0};

	bool operator==(const Unmet &) const = default;
};

// A region's households' goods beyond whole totes, and their shortages: the opened totes by item, each
// above 0; each need's unmet total and pressure, from its first short interval on; the last
// interval's severity, 0 to 1; and the intervals a need went short in.
struct Pantry
{
	std::vector<Opened> opened;
	std::vector<Unmet> unmet;
	double severity{0.0};
	std::uint64_t shortIntervals{0};

	bool operator==(const Pantry &) const = default;
};

// A ship's own life support (D-045, D-046, D-094): the oxygen left in each tank, in kilograms, and the
// occupant-days left in each installed scrubber cartridge, in its hull's order; and the venue whose
// station life support it is connected to, empty while it draws on its own. Provisioning fills it once;
// only Refill and Replace add to it.
struct LifeSupport
{
	std::vector<double> tanks;
	std::vector<double> cartridges;
	std::string connectedTo;

	bool operator==(const LifeSupport &) const = default;
};

// A ship's fuel (D-107, D-108), in kilograms, apart from its hold: the reaction mass in its tanks
// and the fusion pellets in its drive's magazine, each at most what its hull and the systems fitted
// to it hold (Identities::FuelTanks). Provisioning fills both once.
struct Fuel
{
	double reactionMass{0.0};
	double pellets{0.0};

	bool operator==(const Fuel &) const = default;
};

// Whether a journey from a venue refuels the ship first (Travel::Fleet::Refuel), set by its owner's
// Company::SetRefuel; a ship without one does.
struct Refuel
{
	bool enabled{true};

	bool operator==(const Refuel &) const = default;
};

// The totes of an item a venue's book filled in one step, buyer and seller counted once.
struct Sold
{
	std::uint64_t step{0};
	std::string itemId;
	std::uint64_t totes{0};

	bool operator==(const Sold &) const = default;
};

// An item's latest fill at a venue: the step it settled in and its price, a Core::Decimal's raw
// microcredits per the item's unit.
struct LastSale
{
	std::string itemId;
	std::uint64_t step{0};
	std::int64_t price{0};

	bool operator==(const LastSale &) const = default;
};

// What a venue's market has sold (docs/market-information-plan.md, "Sales reporting"), on its Market:
// the fills of at least the last sales window, by step then item, oldest first, and each item's last
// sale, by item, however long ago.
struct Sales
{
	std::vector<Sold> recent;
	std::vector<LastSale> last;

	bool operator==(const Sales &) const = default;
};

// How a player came by a market report: a ship of theirs docked there, or their starting reports.
enum class Source
{
	Docked,
	Starting
};

// A price a side of a report's book rests at, a Core::Decimal's raw microcredits per the item's unit,
// above 0, and the totes resting at it, at least one.
struct Level
{
	std::int64_t price{0};
	std::uint64_t totes{0};

	bool operator==(const Level &) const = default;
};

// The prices a report keeps of each side of a good's book (D-102): a broker's ladder.
inline constexpr std::size_t kReportLevels = 4;

// One good of a market report: each side's best kReportLevels prices resting, best first, the bids
// highest and the asks lowest; the totes sold over the report's window; and the good's last sale, its
// step and price, 0 and 0 for none.
struct Line
{
	std::string itemId;
	std::vector<Level> bids;
	std::vector<Level> asks;
	std::uint64_t sold{0};
	std::uint64_t lastStep{0};
	std::int64_t lastPrice{0};

	// A side's best price, 0 for none.
	std::int64_t Bid() const
	{
		return bids.empty() ? 0 : bids.front().price;
	}
	std::int64_t Ask() const
	{
		return asks.empty() ? 0 : asks.front().price;
	}

	bool operator==(const Line &) const = default;
};

// What a player last saw of a venue's market (docs/market-information-plan.md): at the completed step
// it was observed at, the clock's step then; its sales window, from the step since to the observation,
// shorter for a market younger than the window; and its goods in the market's order. It is live while
// a ship of the player's fleet stays docked there, and otherwise as it was when the last one left.
struct Report
{
	std::string venueId;
	Source source{Source::Docked};
	std::uint64_t observed{0};
	std::uint64_t since{0};
	std::vector<Line> lines;

	bool operator==(const Report &) const = default;
};

// A player's market reports, on their Principal, by venue ID: theirs, not a ship's, so they outlive
// the fleet that observed them. A venue with none is unknown to them.
struct Reports
{
	std::vector<Report> reports;

	bool operator==(const Reports &) const = default;
};

// A yearly rate in basis points over a day of it: interest on arrears is owed * rate / kRateYear a day.
inline constexpr std::int64_t kRateYear = 10'000LL * 365;

// What a venue charges ships of one fee class (Content::FeeClass), in whole credits, each at least 0.
struct ClassFees
{
	// The Dock.BerthClassDefinition, by record name.
	std::string padClass;
	// Paid to the venue as a journey to it starts (Travel::Travel).
	std::int64_t docking{0};
	// Paid by each ship docked at the venue as each simulation hour ends.
	std::int64_t berthPerHour{0};

	bool operator==(const ClassFees &) const = default;
};

// A venue's posted charges (D-097, D-098, D-100), on its Market, in whole credits, each at least 0: its
// operator's to set, each venue's the tuning's charges in the initial state. A change applies to the
// next charge, never to one paid.
struct Fees
{
	// By pad class, in name order, each named once.
	std::vector<ClassFees> classes;
	// Paid with the hour's berth fee for each occupant of a docked ship connected to the venue's life
	// support (D-047).
	std::int64_t berthPerOccupantHour{0};
	// The yearly interest on arrears owed to the venue, in basis points, compounded daily, at most
	// the tuning's usury limit as it is charged.
	int arrearsRateBps{0};

	bool operator==(const Fees &) const = default;
};

// A venue's fees as a share of the tuning's defaults, in basis points, on its Market beside its Fees:
// kDefaultLevelBps in the initial state, moved each day by manifest.pricing while no player directs
// the venue's operator (D-100).
struct Pricing
{
	int levelBps{0};

	bool operator==(const Pricing &) const = default;
};

inline constexpr int kDefaultLevelBps = 10'000;

// What a docked ship's company could not pay of its berth fees, in whole credits above 0, owed to the
// venue the ship is docked at, its interest added to it as each day ends: paid first as the next hour
// ends, and all of it before the ship leaves (Travel::Travel), so a ship that is not docked owes none.
// Past the grace, its cargo is seized for it (Policy::kSeizureSystem).
struct Arrears
{
	std::int64_t owed{0};
	// The part of a credit of interest accrued and not yet added to what is owed, in units of
	// 1 / kRateYear credits, below kRateYear, so a small debt accrues exactly over the days.
	std::int64_t carry{0};
	// The step whose berth fee began them, kept until they are paid in full.
	std::uint64_t since{0};

	bool operator==(const Arrears &) const = default;
};

// A journey's thrust when its engines thrust to the midpoint, in basis points of its cruise's time.
inline constexpr std::uint32_t kFullThrustBps = 10000;

// A ship's standing trade policy (D-097), which its owner sets: whether its job runs, the whole credits
// a trade's estimate must be above and a sale must recover beyond its lot's cost and charges, and the
// whole credits below that the owner allows a lot to sell for, each at least 0; and the thrust every
// leg flies (D-107), at most kFullThrustBps, 0 for the job to choose one with each trade.
struct Policy
{
	bool enabled{false};
	std::int64_t targetProfit{0};
	std::int64_t lossAllowance{0};
	std::uint32_t thrustBps{0};

	bool operator==(const Policy &) const = default;
};

// Where a ship's job has got (docs/stage-4-trading-plan.md, "W10 plan").
enum class Phase
{
	// Choosing a trade from its owner's reports: a source and a destination, or with a lot aboard a
	// destination for it.
	Choosing,
	ToSource,
	Buying,
	ToDestination,
	Selling,
	Blocked
};

enum class Blockage
{
	None,
	// No known trade estimates above the target, or none for the lot aboard clears its floor.
	NoTrade,
	// A journey of the job's failed, its ship left where it is with its cargo.
	JourneyFailed,
	// A command of the job's was refused: a journey its company could not pay for, or goods it could
	// not move.
	Refused
};

// A ship's standing job, beside its Policy: what it trades, from where to where, and the lot it
// carries, its totes, what they cost (their fills' credits, fees included), the charges attributed to
// them and the allowance the lot has spent, each whole credits, all 0 with no lot. A blocked job keeps
// its lot and reconsiders at its retry step.
struct Job
{
	Phase phase{Phase::Choosing};
	std::string itemId;
	std::string sourceId;
	std::string destinationId;
	std::uint64_t totes{0};
	std::int64_t cost{0};
	std::int64_t charges{0};
	std::int64_t allowanceSpent{0};
	// What its trade was estimated to make after charges when it was chosen, whole credits; 0 before.
	std::int64_t estimate{0};
	Blockage blockage{Blockage::None};
	// The step it first blocked at, of this blockage, and the step it reconsiders at.
	std::uint64_t blockedSince{0};
	std::uint64_t retry{0};
	// What its ship has drawn of its own life support while the lot is aboard and its job has not yet
	// charged to the lot (D-100), each finite and at least 0: oxygen in kilograms, cartridges in
	// occupant-days.
	double oxygenKilograms{0.0};
	double cartridgeDays{0.0};
	// The thrust its trade was chosen at, which its legs fly, above 0 and at most kFullThrustBps; and
	// the fuel its ship's journeys have burned while the lot is aboard and its job has not yet charged
	// to the lot (W21), in kilograms, each finite and at least 0.
	std::uint32_t thrustBps{kFullThrustBps};
	double reactionMassKilograms{0.0};
	double pelletKilograms{0.0};

	bool operator==(const Job &) const = default;
};

// The last price a company paid for an item, a Core::Decimal's raw microcredits per the item's unit,
// above 0.
struct PaidPrice
{
	std::string itemId;
	std::int64_t price{0};

	bool operator==(const PaidPrice &) const = default;
};

// A company's, on its Company, by item in name order, each once: set as its buys fill, the price the
// life support its ships draw in transit is charged to a lot at (D-100). An item it never bought has
// none.
struct Paid
{
	std::vector<PaidPrice> prices;

	bool operator==(const Paid &) const = default;
};

// Why a Stock or a Cargo breaks the rules above, for a load to refuse: none when it keeps them.
std::optional<std::string> Broken(const Stock &stock);

std::optional<std::string> Broken(const Cargo &cargo);

std::optional<std::string> Broken(const Order &order);

std::optional<std::string> Broken(const Systems &systems);

std::optional<std::string> Broken(const Production &production);

std::optional<std::string> Broken(const Pantry &pantry);

std::optional<std::string> Broken(const LifeSupport &lifeSupport);

std::optional<std::string> Broken(const Fuel &fuel);

std::optional<std::string> Broken(const Sales &sales);

std::optional<std::string> Broken(const Reports &reports);

std::optional<std::string> Broken(const Fees &fees);

// The venue's fees for the fee class; null for a class it posts none for.
const ClassFees *Find(const Fees &fees, std::string_view padClass);

std::optional<std::string> Broken(const Arrears &arrears);

std::optional<std::string> Broken(const Paid &paid);

// The company's last price for the item, or null for an item it never bought.
const PaidPrice *Find(const Paid &paid, std::string_view itemId);

std::optional<std::string> Broken(const Policy &policy);

std::optional<std::string> Broken(const Job &job);

// The components and their service's record, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::World
