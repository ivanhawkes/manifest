#include "ManifestConsole.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include <jettison/host/WorldContent.h>
#include <jettison/net/Protocol.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/company/Company.h>
#include <manifest/host/Locations.h>
#include <manifest/host/Start.h>
#include <manifest/policy/Policy.h>
#include <manifest/procurement/Procurement.h>
#include <manifest/query/Query.h>
#include <manifest/remote/Keys.h>
#include <manifest/trade/Trade.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/State.h>

namespace Manifest::Tools
{
namespace sim = Manifest::Core;
using Jettison::DevTools::Console;

namespace
{
// The mode the development commands play.
constexpr std::string_view kGameMode = "manifest";


unsigned ParseCount(
    std::span<const std::string> arguments, std::size_t index, unsigned fallback, unsigned minimum,
    unsigned maximum)
{
	if (index >= arguments.size())
		return fallback;
	const std::string &text = arguments[index];
	unsigned value{};
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum)
		throw std::runtime_error{
		    std::format("expected a whole number from {} to {}, got '{}'", minimum, maximum, text)};
	return value;
}


// Whether a connection that closed so is dialled again: the server gone quiet or restarting. A
// credential, key, protocol, content or message it refused stays refused, and a full server is left
// for the player to try again, so no refusal is dialled in a loop.
bool Redials(Jettison::Net::Reason reason)
{
	using Jettison::Net::Reason;
	return reason == Reason::Closed || reason == Reason::HandshakeTimeout || reason == Reason::Shutdown;
}


std::string PadRight(const std::string &text, std::size_t width)
{
	return text.size() >= width ? text : text + std::string(width - text.size(), ' ');
}


template <typename Enum> std::string_view NameOf(Enum value)
{
	return Jettison::Reflect::EnumName(entt::meta_any{value});
}


std::string_view ResultName(Command::Result result)
{
	return NameOf(result);
}

// The hosted world's step (D-086), which a snapshot's steps are counted in.
constexpr double kStepSeconds = 1.0 / 60.0;
constexpr double kDaySeconds = 86400.0;

// The longest step asks for, in simulation seconds: a cruise between worlds takes days (D-090).
constexpr double kMostSeconds = 60 * kDaySeconds;

// How long a run of steps holds the frontend at a time: its input is read between slices.
constexpr std::chrono::milliseconds kSlice{50};


double Days(std::uint64_t steps)
{
	return static_cast<double>(steps) * kStepSeconds / kDaySeconds;
}


std::string Price(std::int64_t raw)
{
	return sim::Decimal::FromRaw(raw).ToString();
}


// The most a count argument takes; the world's own rules bound it further.
constexpr unsigned kMostCount = std::numeric_limits<unsigned>::max();

// A thrust's percent of its cruise, in basis points.
constexpr std::uint32_t kBpsPerPercent = World::kFullThrustBps / 100;
constexpr unsigned kPercent = 100;


// A whole number of credits, 0 or more, as the world's fees, budgets and totals are.
std::int64_t ParseCredits(const std::string &text)
{
	std::int64_t value{};
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size() || value < 0)
		throw std::runtime_error{
		    std::format("expected a whole number of credits, 0 or more, got '{}'", text)};
	return value;
}


bool ParseSwitch(const std::string &text)
{
	if (text != "on" && text != "off")
		throw std::runtime_error{std::format("expected on or off, got '{}'", text)};
	return text == "on";
}


template <typename Payload> std::vector<std::byte> Encode(const Payload &payload, std::uint32_t version)
{
	return Jettison::Serialise::BinaryArchive::Write(
	    payload, Jettison::Serialise::FormatVersion{version});
}


// A count of steps, or a span of simulation time such as 90s, 30m, 6h or 4d, as whole steps.
std::uint64_t ParseSteps(std::span<const std::string> arguments)
{
	if (arguments.empty())
		return 1;
	const std::string &text = arguments[0];
	constexpr std::string_view kUnits = "smhd";
	constexpr std::array<double, 4> kSeconds{1.0, 60.0, 3600.0, kDaySeconds};
	const std::size_t unit = text.empty() ? std::string_view::npos : kUnits.find(text.back());
	const std::size_t digits = unit == std::string_view::npos ? text.size() : text.size() - 1;
	std::uint64_t value{};
	const auto [end, error] = std::from_chars(text.data(), text.data() + digits, value);
	const double seconds =
	    static_cast<double>(value) * (unit == std::string_view::npos ? kStepSeconds : kSeconds[unit]);
	if (digits == 0 || error != std::errc{} || end != text.data() + digits || value == 0 ||
	    seconds > kMostSeconds)
		throw std::runtime_error{std::format(
		    "expected a count of steps or a time such as 90s, 30m, 6h or 4d, at most 60d, got '{}'",
		    text)};
	return unit == std::string_view::npos
	           ? value
	           : static_cast<std::uint64_t>(std::llround(seconds / kStepSeconds));
}


std::string Describe(const World::Journey &journey, std::uint64_t step)
{
	if (journey.leg == World::Leg::Failed)
		return std::format(", journey to {} failed: {}", journey.venueId, NameOf(journey.failure));
	if (journey.Ended())
		return "";
	return std::format(
	    ", {} to {}, {:.2f} days out", NameOf(journey.leg), journey.venueId,
	    Days(step - journey.started));
}


// A span of steps in simulation minutes, the unit a report's age is shown in.
std::string Minutes(std::uint64_t steps)
{
	return std::format("{:.1f} simulation minutes", static_cast<double>(steps) * kStepSeconds / 60.0);
}


// A side of a report's book: its best price, the totes resting at it, and how many prices it keeps.
std::string BookSide(std::string_view side, const std::vector<World::Level> &levels)
{
	if (levels.empty())
		return std::format("no {}", side);
	return std::format(
	    "{} {} ({} totes{})", side, Price(levels.front().price), levels.front().totes,
	    levels.size() > 1 ? std::format(", {} prices", levels.size()) : std::string{});
}


// A venue's posted fees: each pad class's docking and hourly berth fee, the hourly fee for each
// occupant connected to its life support, and the yearly rate its arrears accrue at.
std::string Describe(const World::Fees &fees)
{
	std::string text = "\n    fees:";
	for (const World::ClassFees &fee : fees.classes)
		text +=
		    std::format(" {} {} to dock and {} an hour,", fee.padClass, fee.docking, fee.berthPerHour);
	return text + std::format(
	                  " {} an hour for each connected occupant, arrears at {} bps a year",
	                  fees.berthPerOccupantHour, fees.arrearsRateBps);
}


std::string Describe(const Query::MarketView &market, std::uint64_t step)
{
	const World::Report &report = market.report;
	std::string text = std::format(
	    "\n  {}: {}, sales over the {} before", report.venueId,
	    market.live ? std::string{"live"}
	                : std::format(
	                      "stale{}, observed {} ago",
	                      report.source == World::Source::Starting ? " starting report" : "",
	                      Minutes(step - report.observed)),
	    Minutes(report.observed - report.since));
	text += Describe(market.fees);
	for (const World::Line &line : report.lines)
		text += std::format(
		    "\n    {}: {}, {}, {} totes sold, {}", line.itemId, BookSide("bid", line.bids),
		    BookSide("ask", line.asks), line.sold,
		    line.lastPrice == 0 ? std::string{"never sold"}
		                        : std::format(
		                              "last sold at {} {} before", Price(line.lastPrice),
		                              Minutes(report.observed - line.lastStep)));
	return text;
}


// The administrator, as the trusted harness makes it; a principal's ID never names it.
const Command::Actor kHost{Command::ActorKind::Administrator, "Host"};


std::string_view OnOff(bool on)
{
	return on ? "on" : "off";
}


std::string Describe(const World::Fuel &fuel, const World::Fuel &tanks)
{
	return std::format(
	    "\n    fuel: {:.3f} of {:.3f} kg of reaction mass, {:.3f} of {:.3f} kg of pellets",
	    fuel.reactionMass, tanks.reactionMass, fuel.pellets, tanks.pellets);
}


std::string Describe(const World::Policy &policy)
{
	return std::format(
	    "\n    policy: {}, target {} credits, loss allowance {} credits, {}, contracts {}",
	    OnOff(policy.enabled), policy.targetProfit, policy.lossAllowance,
	    policy.thrustBps == 0
	        ? std::string{"thrust chosen with each trade"}
	        : std::format("thrust {:g}%", static_cast<double>(policy.thrustBps) / kBpsPerPercent),
	    OnOff(policy.contracts));
}


std::string Describe(const World::Job &job)
{
	std::string text = std::format("\n    job: {}", NameOf(job.phase));
	if (!job.tenderId.empty())
		text += std::format(" on tender {}", job.tenderId);
	if (!job.itemId.empty())
		text += std::format(", {} from {} to {}", job.itemId, job.sourceId, job.destinationId);
	if (job.totes > 0)
		text += std::format(
		    ", {} totes aboard costing {} with {} in charges, {} of its allowance spent", job.totes,
		    job.cost, job.charges, job.allowanceSpent);
	text += std::format(
	    ", estimated at {} credits, thrust {:g}%", job.estimate,
	    static_cast<double>(job.thrustBps) / kBpsPerPercent);
	if (job.blockage != World::Blockage::None)
		text += std::format(", blocked by {} since step {}", NameOf(job.blockage), job.blockedSince);
	if (job.blockage != World::Blockage::None || job.phase == World::Phase::Bidding)
		text += std::format(", retrying at step {}", job.retry);
	return text + std::format(
	                  "\n      drawn with the lot: {:.3f} kg of oxygen, {:.3f} occupant-days of "
	                  "cartridges, {:.3f} kg of reaction mass, {:.3f} kg of pellets",
	                  job.oxygenKilograms, job.cartridgeDays, job.reactionMassKilograms,
	                  job.pelletKilograms);
}


std::string Describe(const Query::BidView &view)
{
	const World::Bid &bid = view.bid;
	return std::format(
	    "\n  {}: a sealed bid of {} credits on {}, revision {}, accepted at step {} in order {}",
	    view.bidId, bid.total, bid.tenderId, bid.revision, bid.step, bid.order);
}


std::string Describe(const Query::TenderView &view)
{
	const World::Tender &tender = view.tender;
	std::string text = std::format(
	    "\n  {}: {} for {}, {} totes of {} at {}, budget {} credits, {}, published at step {}, "
	    "bids close at step {}, delivery in {} days",
	    view.tenderId, tender.regionId,
	    tender.projectId.empty() ? "relief of " + tender.needId : tender.projectId, tender.totes,
	    tender.itemId, tender.venueId, tender.budget, NameOf(tender.state), tender.published,
	    tender.bidClose, tender.deliveryDays);
	if (tender.award != World::Award::None)
		text += std::format(", award {}", NameOf(tender.award));
	if (!tender.companyId.empty())
		text += std::format(
		    " to {} for {} credits, bond {}, {} of {} totes delivered, delivery closes at step {}",
		    tender.companyId, tender.price, tender.bond, tender.delivered, tender.totes,
		    tender.deliveryClose);
	if (tender.closed != 0)
		text += std::format(", closed at step {}", tender.closed);
	return text;
}


std::string Describe(const Query::RegionView &region)
{
	std::string text = std::format(
	    "\n  {} on {}: {}, population {:.0f}, health {:.1f}, happiness {:.1f}, wage tax {} bps",
	    region.regionId, region.planetId,
	    region.governorId.empty() ? std::string{"open to founding"}
	                              : std::format(
	                                    "governed by {} ({}){}", region.governorName, region.governorId,
	                                    region.playerGoverned ? ", a player's" : ""),
	    region.population, region.health, region.happiness, region.wageTaxBps);
	if (region.treasury)
		text += std::format(", treasury {} credits", *region.treasury);
	if (region.development)
		text += std::format(
		    "\n    development policy: tender {}, limit {} credits{}",
		    OnOff(region.development->tender), region.development->limit,
		    region.development->wait == World::DevelopmentWait::None
		        ? std::string{}
		        : std::format(", waiting: {}", NameOf(region.development->wait)));
	for (const Query::ProjectView &view : region.queue)
	{
		const World::Project &project = view.project;
		text += std::format(
		    "\n    {}: {}, queued {}, worked {} steps", project.projectId, NameOf(project.phase),
		    project.queued, project.worked);
		for (const World::Holding &holding : view.stock)
			text += std::format(
			    "\n      at {}: {} {} totes available, {} reserved", holding.venueId, holding.itemId,
			    holding.available, holding.reserved);
	}
	return text;
}


std::string Describe(const Query::Snapshot &snapshot, std::string_view as)
{
	std::ostringstream view;
	view << "step " << snapshot.step << ", as " << as;
	if (snapshot.companies.empty())
		view << ": no company";
	for (const Query::CompanyView &company : snapshot.companies)
	{
		view << "\n"
		     << company.companyId << " '" << company.displayName << "': " << company.credits
		     << " credits";
		for (const World::Holding &holding : company.holdings)
			view << "\n  at " << holding.venueId << ": " << holding.itemId << " " << holding.available
			     << " totes available, " << holding.reserved << " reserved";
		for (const Query::ShipView &ship : company.ships)
		{
			view << "\n  " << ship.shipId << " '" << ship.displayName << "' " << ship.hullId << ", "
			     << (ship.dockedAt.empty() ? std::string{"undocked"} : "docked at " + ship.dockedAt)
			     << (ship.journey ? Describe(*ship.journey, snapshot.step) : "");
			view << "\n    hold of " << ship.hold << " totes";
			for (const World::Fitting &fitting : ship.systems)
				view << ", " << fitting.skillId << " tier " << fitting.tier;
			view << Describe(ship.fuel, ship.fuelTanks);
			if (ship.policy)
				view << Describe(*ship.policy);
			if (ship.job)
				view << Describe(*ship.job);
			for (const World::Lot &lot : ship.cargo)
				view << "\n    " << lot.itemId << " " << lot.totes << " totes";
			for (const World::Opened &opened : ship.opened)
				view << "\n    " << opened.itemId << " " << std::format("{:.3f}", opened.quantity)
				     << " left in an opened tote";
			const World::LifeSupport &lifeSupport = ship.lifeSupport;
			double oxygen = 0.0;
			for (const double tank : lifeSupport.tanks)
				oxygen += tank;
			view << std::format(
			    "\n    life support: {:.3f} kg of oxygen in {} tanks, cartridges of", oxygen,
			    lifeSupport.tanks.size());
			for (const double cartridge : lifeSupport.cartridges)
				view << std::format(" {:.3f}", cartridge);
			view << " occupant-days, "
			     << (lifeSupport.connectedTo.empty() ? std::string{"on its own"}
			                                         : "connected at " + lifeSupport.connectedTo);
		}
		for (const auto &[orderId, order] : company.orders)
			view << "\n  " << orderId << ": " << (order.side == World::Side::Buy ? "buy " : "sell ")
			     << order.itemId << " at " << order.venueId << ", " << order.remaining << " of "
			     << order.totes << " totes left at " << Price(order.limit);
		for (const Query::BidView &bid : company.bids)
			view << Describe(bid);
	}
	if (!snapshot.markets.empty())
		view << "\nmarkets (any other is unknown):";
	for (const Query::MarketView &market : snapshot.markets)
		view << Describe(market, snapshot.step);
	if (!snapshot.tenders.empty())
		view << "\ntenders:";
	for (const Query::TenderView &tender : snapshot.tenders)
		view << Describe(tender);
	if (!snapshot.regions.empty())
		view << "\nregions:";
	for (const Query::RegionView &region : snapshot.regions)
		view << Describe(region);
	return view.str();
}
} // namespace


ManifestConsole::ManifestConsole(std::string contentSource, std::filesystem::path servers)
    : m_contentSource{std::move(contentSource)}, m_servers{std::move(servers)}
{
	// A connected console writes these payloads with no hosted world to have registered them.
	Trade::RegisterTypes();
	Travel::RegisterTypes();
	Policy::RegisterTypes();
	Procurement::RegisterTypes();
	RegisterCommands();
	m_console.CaptureLog();
}


void ManifestConsole::RegisterCommand(
    std::string name, std::string help, std::size_t minimum, std::size_t maximum, Handler run)
{
	m_console.RegisterCommand(
	    std::move(name), help,
	    [minimum, maximum, help, run = std::move(run)](Console &console, Arguments arguments)
	    {
		    if (arguments.size() < minimum || arguments.size() > maximum)
			    throw std::runtime_error{std::format("usage: {}", help)};
		    console.Print(run(arguments));
	    });
}


void ManifestConsole::RegisterCommands()
{
	RegisterCommand(
	    "version", "version: frontend version", 0, 0, [this](Arguments) { return CommandVersion(); });
	RegisterCommand(
	    "status", "status: configuration, universe and skill tree state", 0, 0,
	    [this](Arguments) { return CommandStatus(); });
	RegisterCommand(
	    "load", "load [source]: loads the content of a package directory or pack", 0, 1,
	    [this](Arguments arguments) { return CommandLoad(arguments.empty() ? "" : arguments[0]); });
	RegisterCommand(
	    "generate", "generate [count] [seed]: generates worlds (default 8, seed 1)", 0, 2,
	    [this](Arguments arguments) { return CommandGenerate(arguments); });
	RegisterCommand(
	    "list", "list: lists every civilisation", 0, 0, [this](Arguments) { return CommandList(); });
	RegisterCommand(
	    "show", "show <id or name>: describes a civilisation; quote names with spaces", 1, 1,
	    [this](Arguments arguments) { return CommandShow(arguments[0]); });
	RegisterCommand(
	    "prices", "prices [count]: most overpriced goods (default 10, maximum 1000)", 0, 1,
	    [this](Arguments arguments) { return CommandPrices(arguments); });
	RegisterCommand(
	    "plan", "plan [count]: best trade routes (default 10, maximum 100)", 0, 1,
	    [this](Arguments arguments) { return CommandPlan(arguments); });
	RegisterCommand(
	    "tick", "tick [count]: advances the simulation (default 1, maximum 1000)", 0, 1,
	    [this](Arguments arguments) { return CommandTick(arguments); });
	RegisterCommand(
	    "skills", "skills: skill tiers and costs", 0, 0, [this](Arguments) { return CommandSkills(); });
	RegisterCommand(
	    "upgrade", "upgrade <skillId>: buys one tier", 1, 1,
	    [this](Arguments arguments) { return CommandUpgrade(arguments[0]); });
	RegisterCommand(
	    "host", "host <scenario>: starts the hosted world from the content, replacing the last", 1, 1,
	    [this](Arguments arguments) { return CommandHost(arguments[0]); });
	RegisterCommand(
	    "observe", "observe [principal]: what a player sees of the hosted world (default: the host)", 0,
	    1, [this](Arguments arguments) { return CommandObserve(arguments); });
	RegisterCommand(
	    "provision",
	    "provision <principal> <company name> [ship name]: queues a player's start, as the host, the "
	    "ship "
	    "named the company's name and 1 when no name is given",
	    2, 3,
	    [this](Arguments arguments)
	    {
		    return CommandProvision(
		        arguments[0], arguments[1],
		        arguments.size() > 2 ? arguments[2] : Company::DefaultShipName(arguments[1], 1));
	    });
	RegisterCommand(
	    "cargo",
	    "cargo load|unload [principal] <ship> <venue> <item> <totes>: queues a player's move; the "
	    "principal only when not connected",
	    5, 6, [this](Arguments arguments) { return CommandCargo(arguments); });
	RegisterCommand(
	    "trade",
	    "trade buy|sell [principal] <venue> <item> <totes> <price>: queues a player's limit order, the "
	    "price per the item's unit; the principal only when not connected",
	    5, 6, [this](Arguments arguments) { return CommandTrade(arguments); });
	RegisterCommand(
	    "cancel",
	    "cancel [principal] <order>: queues a player's cancel; the principal only when not connected",
	    1, 2, [this](Arguments arguments) { return CommandCancel(arguments); });
	RegisterCommand(
	    "travel",
	    "travel [principal] <ship> <venue> [thrust%] [g]: queues a player's journey, the engines "
	    "thrusting for that share of its cruise, half at each end (100 by default), at that many g in "
	    "tenths (1 by default); the principal only when not connected",
	    2, 5, [this](Arguments arguments) { return CommandTravel(arguments); });
	RegisterCommand(
	    "service",
	    "service refill|replace [principal] <ship>: queues a player's oxygen refill or cartridge "
	    "replacement from the ship's hold; the principal only when not connected",
	    2, 3, [this](Arguments arguments) { return CommandService(arguments); });
	RegisterCommand(
	    "fit",
	    "fit [principal] <ship> <offer> <tier>: queues a player's purchase of an offer's tier for "
	    "their "
	    "docked ship with their company's credits; the principal only when not connected",
	    3, 4, [this](Arguments arguments) { return CommandFit(arguments); });
	RegisterCommand(
	    "policy",
	    "policy [principal] <ship> on|off <target> <allowance> [thrust%] [on|off]: queues a player's "
	    "standing trade policy for their ship: the credits a trade must make above its cost, the "
	    "credits a sale may lose, the thrust of every leg (0, the default, for the job to choose) and "
	    "whether it bids on tenders (off by default); the principal only when not connected",
	    4, 7, [this](Arguments arguments) { return CommandPolicy(arguments); });
	RegisterCommand(
	    "refuel",
	    "refuel [principal] <ship> on|off: queues whether a journey from a venue first refuels the "
	    "player's ship; the principal only when not connected",
	    2, 3, [this](Arguments arguments) { return CommandRefuel(arguments); });
	RegisterCommand(
	    "fees",
	    "fees [principal] <venue> <occupant fee> <arrears rate bps> <class> <docking> <berth> "
	    "[<class> <docking> <berth>]...: queues a venue operator's whole fees, every pad class of "
	    "the venue in name order, in whole credits; the principal only when not connected",
	    6, std::numeric_limits<std::size_t>::max(),
	    [this](Arguments arguments) { return CommandFees(arguments); });
	RegisterCommand(
	    "found",
	    "found [principal] <region> <company> <name>: queues a player's company founding a region "
	    "no one governs; the principal only when not connected",
	    3, 4, [this](Arguments arguments) { return CommandFound(arguments); });
	RegisterCommand(
	    "tender",
	    "tender publish|cancel|bid|withdraw|deliver|abandon [principal] <operands>: queues a "
	    "region's tender (publish <region> <project> <item> <venue> <totes> <budget> <bid days> "
	    "<delivery days>; cancel <tender>) or a company's bid, withdrawal, delivery or abandoned "
	    "contract (bid <tender> <company> <total> [revision]; withdraw <tender> <company>; deliver "
	    "<tender> <ship>; abandon <tender>); the principal only when not connected",
	    2, 10, [this](Arguments arguments) { return CommandTender(arguments); });
	RegisterCommand(
	    "project",
	    "project queue|cancel [principal] <region> <project>: queues a governor's project onto a "
	    "region's queue or out of it; the principal only when not connected",
	    3, 4, [this](Arguments arguments) { return CommandProject(arguments); });
	RegisterCommand(
	    "development",
	    "development [principal] <region> on|off <limit>: queues a region's standing development "
	    "policy, whether the AI issuer tenders its head project's inputs within a limit of credits "
	    "of live budgets; the principal only when not connected",
	    3, 4, [this](Arguments arguments) { return CommandDevelopment(arguments); });
	RegisterCommand(
	    "tax",
	    "tax [principal] <region> <rate bps>: queues a governor's wage-tax rate for its region, in "
	    "basis points of the wages paid, withheld from the next payroll; the principal only when not "
	    "connected",
	    2, 3, [this](Arguments arguments) { return CommandTax(arguments); });
	RegisterCommand(
	    "book", "book <venue> [item]: the resting orders at a venue, as the host sees them", 1, 2,
	    [this](Arguments arguments) { return CommandBook(arguments); });
	RegisterCommand(
	    "step",
	    "step [count|time]: advances the hosted world, by steps or a time such as 6h or 4d, stopping "
	    "when a journey ends, and reports what ran (default 1); a long run goes on beside the "
	    "console until it ends or stop",
	    0, 1, [this](Arguments arguments) { return CommandStep(arguments); });
	RegisterCommand(
	    "stop", "stop: ends the run of steps under way at the step it has reached, and reports it", 0,
	    0, [this](Arguments) { return CommandStop(); });
	RegisterCommand(
	    "connect",
	    "connect <host:port> <token>: plays on a server as the token's actor, the server's key "
	    "pinned from the server list",
	    2, 2, [this](Arguments arguments) { return CommandConnect(arguments[0], arguments[1]); });
	RegisterCommand(
	    "disconnect", "disconnect: leaves the server; the world goes on without you", 0, 0,
	    [this](Arguments) { return CommandDisconnect(); });
	const auto quit = [this](Arguments)
	{
		m_quitRequested = true;
		return std::string{"Exiting."};
	};
	RegisterCommand("quit", "quit: ends the session (Ctrl-D works too)", 0, 0, quit);
	RegisterCommand("exit", "exit: alias for quit", 0, 0, quit);
}


std::string ManifestConsole::CommandVersion()
{
	std::ostringstream line;
	line << "Manifest core development front-end, C++" << __cplusplus / 100;

	return line.str();
}


std::string ManifestConsole::CommandStatus()
{
	std::ostringstream status;
	status << "content " << m_contentSource << (m_configLoaded ? " (loaded)" : " (not loaded)") << "\n"
	       << "ages " << m_content.ages.size() << ", categories " << m_content.categories.size()
	       << ", items " << m_content.gameMode.items.size() << ", skills "
	       << m_content.gameMode.skills.size() << ", names " << m_content.names.NameCount() << " in "
	       << m_content.names.PoolCount() << " pools\n"
	       << "sandbox: "
	       << (m_configLoaded ? m_simulation.StatusLine() : std::string("no universe: run generate"))
	       << "\n"
	       << "sandbox skill tree: " << m_progression.Summary() << "\n"
	       << "hosted: "
	       << (m_pHosted ? std::format("step {}", *HostedStep()) : std::string("none: run host"))
	       << "\nconnected: "
	       << (!m_pRemote ? std::string{"no: run connect"}
	           : m_pRemote->Welcomed()
	               ? std::format("to {} as {}", m_link.endpoint, m_pRemote->Welcomed()->actorId)
	               : std::format(
	                     "{} to {}", m_nextSequence ? "reconnecting" : "connecting", m_link.endpoint));

	return status.str();
}


std::string ManifestConsole::CommandLoad(const std::string &source)
{
	const std::string target = source.empty() ? m_contentSource : source;
	const Jettison::Vfs::Mount mount{"manifest", target};
	Content::ContentSet content = Content::LoadContent({&mount, 1}, kGameMode);
	sim::ProgressionManager progression;
	std::string errorMessage;
	if (!progression.Build(content.gameMode.skills, errorMessage))
		throw std::runtime_error{errorMessage};

	// A failed reload preserves the working session. A successful one replaces all
	// configuration together and discards the universe generated from the previous data.
	progression.SetResources(20);
	m_content = std::move(content);
	m_progression = std::move(progression);
	m_simulation = {};
	m_contentSource = target;
	m_configLoaded = true;

	return std::format(
	    "Loaded {}: ages {}, categories {}, items {}, skills {}, names {} in {} pools, content "
	    "{:016x}.",
	    target, m_content.ages.size(), m_content.categories.size(), m_content.gameMode.items.size(),
	    m_content.gameMode.skills.size(), m_content.names.NameCount(), m_content.names.PoolCount(),
	    m_content.identity.fingerprint);
}


std::string ManifestConsole::CommandGenerate(Arguments arguments)
{
	if (!m_configLoaded)
	{
		throw std::runtime_error{"Run load first: the configuration is not loaded."};
	}

	sim::GenerateOptions options;
	options.civilisationCount = ParseCount(arguments, 0, 8, 1, 500);
	options.seed = ParseCount(arguments, 1, 1, 0, std::numeric_limits<unsigned>::max());

	std::string errorMessage;
	if (!m_simulation.Generate(
	        m_content.ages, m_content.categories, m_content.gameMode.items, m_content.names, options,
	        errorMessage))
	{
		throw std::runtime_error{errorMessage};
	}

	std::ostringstream result;
	result << "Generated " << m_simulation.GetCivilisations().size() << " civilisations with seed "
	       << options.seed << ", " << m_content.gameMode.items.size() << " goods.\n"
	       << m_simulation.StatusLine();

	return result.str();
}


std::string ManifestConsole::CommandList()
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	std::ostringstream list;
	list << "id      name                      pool     age                   pop      "
	        "goods treasury\n";
	for (const auto &civ : m_simulation.GetCivilisations())
	{
		list << PadRight(civ.id, 8) << PadRight(civ.name, 24) << PadRight(civ.pool, 8)
		     << PadRight(civ.ageId, 22) << "pop " << PadRight(sim::FormatNumber(civ.population), 7)
		     << " goods " << PadRight(std::to_string(civ.production.size()), 4) << " treasury "
		     << civ.treasury << "\n";
	}

	list << "population is in world units: demand and production are units of goods per day.\n";

	return list.str();
}


std::string ManifestConsole::CommandShow(const std::string &argument)
{
	if (argument.empty())
	{
		throw std::runtime_error{"Usage: show <Civ-01>"};
	}

	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	if (!m_simulation.FindCivilisation(argument))
	{
		throw std::runtime_error{std::format("no civilisation '{}'", argument)};
	}

	return m_simulation.DescribeCivilisation(argument);
}


std::string ManifestConsole::CommandPrices(Arguments arguments)
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	struct Priced
	{
		double ratio;
		std::string itemId;
		sim::Decimal basePrice;
		sim::Decimal price;
	};

	std::vector<Priced> priced;
	for (const auto &item : m_simulation.GetItems())
	{
		const sim::Decimal basePrice = sim::Simulation::BasePrice(item);
		if (basePrice <= 0)
		{
			continue;
		}

		const sim::Decimal price = m_simulation.GetPrice(item.itemId);

		priced.push_back({price.ToDouble() / basePrice.ToDouble(), item.itemId, basePrice, price});
	}

	std::sort(
	    priced.begin(), priced.end(),
	    [](const Priced &left, const Priced &right)
	    {
		    if (left.ratio != right.ratio)
		    {
			    return left.ratio > right.ratio;
		    }

		    return left.itemId < right.itemId;
	    });

	const std::size_t limit = ParseCount(arguments, 0, 10, 1, 1000);

	std::ostringstream table;
	table << "goods by market price over base price\n";
	for (std::size_t index = 0; index < std::min(limit, priced.size()); ++index)
	{
		const Priced &entry = priced[index];
		table << "  " << PadRight(entry.itemId, 26) << "base "
		      << PadRight(sim::FormatNumber(entry.basePrice), 12) << "market "
		      << PadRight(sim::FormatNumber(entry.price), 14) << "x" << sim::FormatNumber(entry.ratio)
		      << "\n";
	}

	// The scarce half of the table alone reads as if everything were unobtainium: the
	// cheap end is the other half of the market.
	if (priced.size() > limit + 5)
	{
		table << "  ... and the cheapest in demand\n";
		for (std::size_t index = priced.size() - 5; index < priced.size(); ++index)
		{
			const Priced &entry = priced[index];
			table << "  " << PadRight(entry.itemId, 26) << "base "
			      << PadRight(sim::FormatNumber(entry.basePrice), 12) << "market "
			      << PadRight(sim::FormatNumber(entry.price), 14) << "x"
			      << sim::FormatNumber(entry.ratio) << "\n";
		}
	}

	return table.str();
}


std::string ManifestConsole::CommandPlan(Arguments arguments)
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	const std::size_t limit = ParseCount(arguments, 0, 10, 1, 100);
	const std::vector<sim::TradeRoute> routes = m_simulation.PlanTrade(static_cast<unsigned>(limit));

	if (routes.empty())
	{
		return "No route pays its freight: every market is flat. Run tick to move goods.";
	}

	std::ostringstream plan;
	plan << "best routes on tick " << m_simulation.GetTick() << "\n"
	     << "  a merchant buys where the good is cheap and sells where it is scarce\n  "
	     << PadRight("good", 24) << PadRight("seller", 9) << PadRight("buyer", 9) << PadRight("qty", 9)
	     << PadRight("buy at", 12) << PadRight("sell at", 12) << PadRight("freight", 11) << "profit\n";
	for (const auto &route : routes)
	{
		plan << "  " << PadRight(route.itemId, 24) << PadRight(route.sellerId, 9)
		     << PadRight(route.buyerId, 9) << PadRight(sim::FormatNumber(route.quantity), 9)
		     << PadRight(sim::FormatNumber(route.unitSellPrice), 12)
		     << PadRight(sim::FormatNumber(route.unitBuyPrice), 12)
		     << PadRight(sim::FormatNumber(route.freight), 11) << sim::FormatNumber(route.profit)
		     << "\n";
	}

	return plan.str();
}


std::string ManifestConsole::CommandTick(Arguments arguments)
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	const unsigned ticks = ParseCount(arguments, 0, 1, 1, 1000);

	std::string errorMessage;
	for (unsigned index = 0; index < ticks; ++index)
	{
		if (!m_simulation.AdvanceTick(errorMessage))
		{
			throw std::runtime_error{errorMessage};
		}
	}

	std::ostringstream result;
	result << "Advanced " << ticks << " tick" << (ticks == 1 ? "" : "s") << ".\n"
	       << m_simulation.StatusLine();

	return result.str();
}


std::string ManifestConsole::CommandSkills()
{
	if (!m_configLoaded)
	{
		throw std::runtime_error{"Run load first: the configuration is not loaded."};
	}

	std::ostringstream skills;
	skills << m_progression.Summary() << "\n";
	for (const auto &entry : m_progression.GetRegistry())
	{
		skills << "  " << m_progression.Describe(entry.first) << "\n";
	}

	return skills.str();
}


std::string ManifestConsole::CommandUpgrade(const std::string &argument)
{
	if (argument.empty())
	{
		throw std::runtime_error{"Usage: upgrade <skillId>"};
	}

	std::string errorMessage;
	if (!m_progression.TryUpgrade(argument, errorMessage))
	{
		throw std::runtime_error{errorMessage};
	}

	return "Upgraded " + argument + " to tier " +
	       std::to_string(m_progression.GetCurrentTier(argument)) + "/" +
	       std::to_string(m_progression.GetMaxTiers(argument)) + ", " +
	       std::to_string(m_progression.GetResources()) + " resources left.";
}


std::optional<std::uint64_t> ManifestConsole::HostedStep() const
{
	if (!m_pHosted)
		return std::nullopt;
	return m_pHosted->World().Clock().Step();
}


Jettison::Host::Session &ManifestConsole::Hosted()
{
	if (!m_pHosted)
		throw std::runtime_error{"No hosted world: run host first."};
	return *m_pHosted;
}


std::string ManifestConsole::CommandHost(const std::string &scenario)
{
	if (m_run)
		throw std::runtime_error{"Stepping: stop first."};

	// A failed start keeps the hosted world there was; the replaced one shuts down with its requests.
	std::unique_ptr<Jettison::Host::Session> hosted = Host::Start({m_contentSource, {}, scenario});
	m_pHosted = std::move(hosted);
	m_unreported.clear();
	return std::format("Hosting {} from {}, at step {}.", scenario, m_contentSource, *HostedStep());
}


std::string ManifestConsole::CommandObserve(Arguments arguments)
{
	if (m_pRemote)
	{
		if (!arguments.empty())
			throw std::runtime_error{"Connected: observe shows only your own view; disconnect first."};
		if (!m_pRemote->Latest())
			return "No snapshot yet: the server sends one after its next step.";
		return Describe(*m_pRemote->Latest(), m_pRemote->Welcomed()->actorId);
	}
	Jettison::Host::Session &hosted = Hosted();
	const Command::Actor actor =
	    arguments.empty() ? kHost : Command::Actor{Command::ActorKind::Principal, arguments[0]};
	return Describe(
	    Query::Observe(hosted.World(), Host::HostedServices(hosted).identities, actor), actor.id);
}


std::string ManifestConsole::Admit(
    const Command::Actor &actor, std::string_view command, std::uint32_t version,
    std::span<const std::byte> payload)
{
	Jettison::Host::Session &hosted = Hosted();
	Command::Requests &requests = Host::HostedServices(hosted).requests;
	const std::uint64_t sequence = requests.NextSequence(actor);
	const Command::Receipt receipt =
	    requests.Admit(hosted.World(), actor, sequence, std::string{command}, version, payload);
	if (receipt.status == Command::Receipt::Status::Refused)
		return std::format("Refused: {}.", ResultName(receipt.result));
	m_unreported.emplace_back(actor, sequence);
	return std::format(
	    "Queued {}'s request {} in order {}: it runs at the next step.", actor.id, sequence,
	    receipt.order);
}


std::string ManifestConsole::Send(
    std::string_view command, std::uint32_t version, std::span<const std::byte> payload)
{
	if (!m_nextSequence)
		throw std::runtime_error{"Not connected yet: wait for the welcome."};

	// Kept until heard complete or refused. One that cannot be sent now, the connection lost, is sent
	// once reconnected.
	const std::uint64_t sequence = (*m_nextSequence)++;
	Unsettled &request = m_unsettled.emplace_back(
	    Unsettled{sequence, std::string{command}, version, {payload.begin(), payload.end()}});
	request.sent = m_pRemote->Retry(sequence, command, version, payload);
	return request.sent ? std::format("Sent request {} to the server.", sequence)
	                    : std::format("Kept request {}: it is sent once reconnected.", sequence);
}


std::string ManifestConsole::CommandProvision(
    const std::string &principalId, const std::string &companyName, const std::string &shipName)
{
	using Jettison::Serialise::BinaryArchive;
	const std::vector<std::byte> payload = BinaryArchive::Write(
	    Company::Provision{principalId, principalId, companyName, shipName},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
	return m_pRemote ? Send(Company::kProvision, Company::kVersion, payload)
	                 : Admit(kHost, Company::kProvision, Company::kVersion, payload);
}


ManifestConsole::Arguments ManifestConsole::Operands(
    Arguments arguments, std::size_t first, std::size_t count, std::string_view usage) const
{
	const std::size_t start = m_pRemote ? first : first + 1;
	if (arguments.size() != start + count)
		throw std::runtime_error{std::format(
		    "usage: {}", m_pRemote ? std::format("{}, as you, when connected", usage) : usage)};
	return arguments.subspan(start);
}


ManifestConsole::Arguments ManifestConsole::Operands(
    Arguments arguments, std::size_t first, std::string_view head,
    std::span<const std::string_view> names, std::size_t optional) const
{
	const std::size_t start = m_pRemote ? first : first + 1;
	const std::size_t required = names.size() - optional;
	std::string usage{head};
	if (!m_pRemote)
		usage += " <principal>";
	for (std::size_t index = 0; index < names.size(); ++index)
		usage +=
		    index < required ? std::format(" <{}>", names[index]) : std::format(" [{}]", names[index]);
	const std::size_t given = arguments.size() > start ? arguments.size() - start : 0;
	return Operands(arguments, first, std::clamp(given, required, names.size()), usage);
}


std::string ManifestConsole::Queue(
    Arguments arguments, std::size_t first, std::string_view command, std::uint32_t version,
    std::span<const std::byte> payload)
{
	return m_pRemote
	           ? Send(command, version, payload)
	           : Admit({Command::ActorKind::Principal, arguments[first]}, command, version, payload);
}


std::string ManifestConsole::CommandCargo(Arguments arguments)
{
	if (arguments[0] != "load" && arguments[0] != "unload")
		throw std::runtime_error{std::format("expected load or unload, got '{}'", arguments[0])};
	const Arguments operands = Operands(
	    arguments, 1, 4,
	    m_pRemote ? "cargo load|unload <ship> <venue> <item> <totes>"
	              : "cargo load|unload <principal> <ship> <venue> <item> <totes>");
	using Jettison::Serialise::BinaryArchive;
	const std::string_view command = arguments[0] == "load" ? Company::kLoad : Company::kUnload;
	const std::vector<std::byte> payload = BinaryArchive::Write(
	    Company::Move{
	        operands[0], operands[1], operands[2],
	        ParseCount(operands, 3, 0, 1, std::numeric_limits<unsigned>::max())},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
	return Queue(arguments, 1, command, Company::kVersion, payload);
}


std::string ManifestConsole::CommandTrade(Arguments arguments)
{
	if (arguments[0] != "buy" && arguments[0] != "sell")
		throw std::runtime_error{std::format("expected buy or sell, got '{}'", arguments[0])};
	const Arguments operands = Operands(
	    arguments, 1, 4,
	    m_pRemote ? "trade buy|sell <venue> <item> <totes> <price>"
	              : "trade buy|sell <principal> <venue> <item> <totes> <price>");
	const std::optional<sim::Decimal> price = sim::Decimal::TryParse(operands[3]);
	if (!price)
		throw std::runtime_error{std::format("expected a price such as 12.5, got '{}'", operands[3])};
	using Jettison::Serialise::BinaryArchive;
	const std::vector<std::byte> payload = BinaryArchive::Write(
	    Trade::Place{
	        operands[0], operands[1], arguments[0] == "buy" ? World::Side::Buy : World::Side::Sell,
	        ParseCount(operands, 2, 0, 1, std::numeric_limits<unsigned>::max()), price->Raw()},
	    Jettison::Serialise::FormatVersion{Trade::kVersion});
	return Queue(arguments, 1, Trade::kPlace, Trade::kVersion, payload);
}


std::string ManifestConsole::CommandCancel(Arguments arguments)
{
	const Arguments operands =
	    Operands(arguments, 0, 1, m_pRemote ? "cancel <order>" : "cancel <principal> <order>");
	using Jettison::Serialise::BinaryArchive;
	const std::vector<std::byte> payload = BinaryArchive::Write(
	    Trade::Cancel{operands[0]}, Jettison::Serialise::FormatVersion{Trade::kVersion});
	return Queue(arguments, 0, Trade::kCancel, Trade::kVersion, payload);
}


std::string ManifestConsole::CommandTravel(Arguments arguments)
{
	constexpr std::size_t kLeast = 2;
	constexpr std::size_t kMost = 4;

	// A tenth of g, in the Decimal's raw millionths.
	constexpr std::int64_t kRawTenthG = 100000;
	constexpr std::int64_t kRawPerMilliG = 1000;
	const std::size_t given = arguments.size() - (m_pRemote ? 0 : 1);
	const Arguments operands = Operands(
	    arguments, 0, std::clamp(given, kLeast, kMost),
	    m_pRemote ? "travel <ship> <venue> [thrust%] [g]"
	              : "travel <principal> <ship> <venue> [thrust%] [g]");
	Travel::Travel travel{operands[0], operands[1]};
	travel.thrustBps =
	    ParseCount(operands, 2, kPercent, 1, kPercent) * (Travel::kFullThrustBps / kPercent);
	if (operands.size() > 3)
	{
		const std::optional<sim::Decimal> g = sim::Decimal::TryParse(operands[3]);
		if (!g || g->Raw() <= 0 || g->Raw() % kRawTenthG != 0 ||
		    g->Raw() / kRawPerMilliG > std::numeric_limits<std::uint32_t>::max())
			throw std::runtime_error{std::format(
			    "expected an acceleration in tenths of g such as 0.5, got '{}'", operands[3])};
		travel.accelerationMilliG = static_cast<std::uint32_t>(g->Raw() / kRawPerMilliG);
	}
	using Jettison::Serialise::BinaryArchive;
	const std::vector<std::byte> payload =
	    BinaryArchive::Write(travel, Jettison::Serialise::FormatVersion{Travel::kVersion});
	return Queue(arguments, 0, Travel::kTravel, Travel::kVersion, payload);
}


std::string ManifestConsole::CommandService(Arguments arguments)
{
	if (arguments[0] != "refill" && arguments[0] != "replace")
		throw std::runtime_error{std::format("expected refill or replace, got '{}'", arguments[0])};
	const Arguments operands = Operands(
	    arguments, 1, 1,
	    m_pRemote ? "service refill|replace <ship>" : "service refill|replace <principal> <ship>");
	using Jettison::Serialise::BinaryArchive;
	const std::vector<std::byte> payload = BinaryArchive::Write(
	    Company::Service{operands[0]}, Jettison::Serialise::FormatVersion{Company::kVersion});
	return Queue(
	    arguments, 1, arguments[0] == "refill" ? Company::kRefill : Company::kReplace,
	    Company::kVersion, payload);
}


std::string ManifestConsole::CommandFit(Arguments arguments)
{
	const Arguments operands = Operands(
	    arguments, 0, 3,
	    m_pRemote ? "fit <ship> <offer> <tier>" : "fit <principal> <ship> <offer> <tier>");
	using Jettison::Serialise::BinaryArchive;
	const std::vector<std::byte> payload = BinaryArchive::Write(
	    Company::Upgrade{
	        operands[0], operands[1],
	        ParseCount(operands, 2, 0, 1, std::numeric_limits<unsigned>::max())},
	    Jettison::Serialise::FormatVersion{Company::kVersion});
	return Queue(arguments, 0, Company::kUpgrade, Company::kVersion, payload);
}


std::string ManifestConsole::CommandPolicy(Arguments arguments)
{
	constexpr std::array<std::string_view, 6> kNames{"ship",      "on|off",  "target",
	                                                 "allowance", "thrust%", "on|off"};
	constexpr std::size_t kOptional = 2;
	const Arguments operands = Operands(arguments, 0, "policy", kNames, kOptional);
	World::Policy policy;
	policy.enabled = ParseSwitch(operands[1]);
	policy.targetProfit = ParseCredits(operands[2]);
	policy.lossAllowance = ParseCredits(operands[3]);
	policy.thrustBps = ParseCount(operands, 4, 0, 0, kPercent) * kBpsPerPercent;
	policy.contracts = operands.size() > 5 && ParseSwitch(operands[5]);
	return Queue(
	    arguments, 0, Policy::kSet, Policy::kVersion,
	    Encode(
	        Policy::Set{
	            operands[0], policy.enabled, policy.targetProfit, policy.lossAllowance,
	            policy.thrustBps, policy.contracts},
	        Policy::kVersion));
}


std::string ManifestConsole::CommandRefuel(Arguments arguments)
{
	constexpr std::array<std::string_view, 2> kNames{"ship", "on|off"};
	const Arguments operands = Operands(arguments, 0, "refuel", kNames, 0);
	return Queue(
	    arguments, 0, Company::kSetRefuel, Company::kVersion,
	    Encode(Company::SetRefuel{operands[0], ParseSwitch(operands[1])}, Company::kVersion));
}


std::string ManifestConsole::CommandFees(Arguments arguments)
{
	constexpr std::array<std::string_view, 3> kHead{"venue", "occupant fee", "arrears rate bps"};
	constexpr std::array<std::string_view, 3> kClass{"class", "docking", "berth"};
	const std::size_t given = arguments.size() - (m_pRemote ? 0 : 1);
	const std::size_t classes = given > kHead.size() ? (given - kHead.size()) / kClass.size() : 0;
	std::vector<std::string_view> names{kHead.begin(), kHead.end()};
	for (std::size_t each = 0; each < std::max<std::size_t>(classes, 1); ++each)
		names.insert(names.end(), kClass.begin(), kClass.end());

	// A count that is not the head and whole classes is the usage, not a guess.
	const Arguments operands = Operands(arguments, 0, "fees", names, 0);
	World::Fees fees;
	fees.berthPerOccupantHour = ParseCredits(operands[1]);
	fees.arrearsRateBps = static_cast<int>(
	    ParseCount(operands, 2, 0, 0, static_cast<unsigned>(std::numeric_limits<int>::max())));
	for (std::size_t at = kHead.size(); at < operands.size(); at += kClass.size())
		fees.classes.push_back(
		    {operands[at], ParseCredits(operands[at + 1]), ParseCredits(operands[at + 2])});
	return Queue(
	    arguments, 0, Company::kSetFees, Company::kVersion,
	    Encode(Company::SetFees{operands[0], std::move(fees)}, Company::kVersion));
}


std::string ManifestConsole::CommandFound(Arguments arguments)
{
	constexpr std::array<std::string_view, 3> kNames{"region", "company", "name"};
	const Arguments operands = Operands(arguments, 0, "found", kNames, 0);
	return Queue(
	    arguments, 0, Company::kFound, Company::kVersion,
	    Encode(Company::Found{operands[0], operands[1], operands[2]}, Company::kVersion));
}


std::string ManifestConsole::CommandTender(Arguments arguments)
{
	namespace Pro = Procurement;
	const std::string &verb = arguments[0];
	const auto operands =
	    [&](std::string_view named, std::span<const std::string_view> names, std::size_t optional = 0)
	{ return Operands(arguments, 1, std::format("tender {}", named), names, optional); };
	const auto queue = [&](std::string_view command, const auto &payload)
	{ return Queue(arguments, 1, command, Pro::kVersion, Encode(payload, Pro::kVersion)); };
	if (verb == "publish")
	{
		constexpr std::array<std::string_view, 8> kNames{
		    "region", "project", "item", "venue", "totes", "budget", "bid days", "delivery days"};
		const Arguments o = operands(verb, kNames);
		return queue(
		    Pro::kPublish,
		    Pro::Publish{
		        o[0], o[1], o[2], o[3], ParseCount(o, 4, 0, 1, kMostCount), ParseCredits(o[5]),
		        ParseCount(o, 6, 0, 1, kMostCount), ParseCount(o, 7, 0, 1, kMostCount)});
	}
	if (verb == "cancel" || verb == "abandon")
	{
		constexpr std::array<std::string_view, 1> kNames{"tender"};
		const Arguments o = operands(verb, kNames);
		return verb == "cancel" ? queue(Pro::kCancel, Pro::Cancel{o[0]})
		                        : queue(Pro::kAbandon, Pro::Abandon{o[0]});
	}
	if (verb == "bid")
	{
		constexpr std::array<std::string_view, 4> kNames{"tender", "company", "total", "revision"};
		const Arguments o = operands(verb, kNames, 1);
		return queue(
		    Pro::kBid, Pro::Bid{o[0], o[1], ParseCredits(o[2]), ParseCount(o, 3, 0, 0, kMostCount)});
	}
	if (verb == "withdraw")
	{
		constexpr std::array<std::string_view, 2> kNames{"tender", "company"};
		const Arguments o = operands(verb, kNames);
		return queue(Pro::kWithdraw, Pro::Withdraw{o[0], o[1]});
	}
	if (verb == "deliver")
	{
		constexpr std::array<std::string_view, 2> kNames{"tender", "ship"};
		const Arguments o = operands(verb, kNames);
		return queue(Pro::kDeliver, Pro::Deliver{o[0], o[1]});
	}
	throw std::runtime_error{
	    std::format("expected publish, cancel, bid, withdraw, deliver or abandon, got '{}'", verb)};
}


std::string ManifestConsole::CommandProject(Arguments arguments)
{
	namespace Pro = Procurement;
	if (arguments[0] != "queue" && arguments[0] != "cancel")
		throw std::runtime_error{std::format("expected queue or cancel, got '{}'", arguments[0])};
	constexpr std::array<std::string_view, 2> kNames{"region", "project"};
	const Arguments o = Operands(arguments, 1, std::format("project {}", arguments[0]), kNames, 0);
	return arguments[0] == "queue" ? Queue(
	                                     arguments, 1, Pro::kQueue, Pro::kVersion,
	                                     Encode(Pro::Queue{o[0], o[1]}, Pro::kVersion))
	                               : Queue(
	                                     arguments, 1, Pro::kCancelProject, Pro::kVersion,
	                                     Encode(Pro::CancelProject{o[0], o[1]}, Pro::kVersion));
}


std::string ManifestConsole::CommandDevelopment(Arguments arguments)
{
	constexpr std::array<std::string_view, 3> kNames{"region", "on|off", "limit"};
	const Arguments o = Operands(arguments, 0, "development", kNames, 0);
	return Queue(
	    arguments, 0, Procurement::kSetDevelopment, Procurement::kVersion,
	    Encode(
	        Procurement::SetDevelopment{o[0], ParseSwitch(o[1]), ParseCredits(o[2])},
	        Procurement::kVersion));
}


std::string ManifestConsole::CommandTax(Arguments arguments)
{
	constexpr std::array<std::string_view, 2> kNames{"region", "rate bps"};
	const Arguments o = Operands(arguments, 0, "tax", kNames, 0);
	return Queue(
	    arguments, 0, Trade::kSetWageTax, Trade::kVersion,
	    Encode(
	        Trade::SetWageTax{
	            o[0], static_cast<int>(ParseCount(
	                      o, 1, 0, 0, static_cast<unsigned>(std::numeric_limits<int>::max())))},
	        Trade::kVersion));
}


std::string ManifestConsole::CommandBook(Arguments arguments)
{
	if (m_pRemote)
		throw std::runtime_error{"Connected: book is the host's view; disconnect first."};
	Jettison::Host::Session &hosted = Hosted();
	const std::string &venueId = arguments[0];
	if (Host::HostedServices(hosted).identities.Find(World::Kind::VenueAccount, venueId) == entt::null)
		throw std::runtime_error{std::format("no venue '{}'", venueId)};

	std::vector<std::pair<std::string, World::Order>> orders;
	for (const auto &[entity, identity, order] :
	     hosted.World().Registry().view<const World::Identity, const World::Order>().each())
		if (order.venueId == venueId && (arguments.size() == 1 || order.itemId == arguments[1]))
			orders.emplace_back(identity.id, order);

	// By item, asks then bids, each best price first and oldest first within a price.
	std::ranges::sort(
	    orders,
	    [](const auto &left, const auto &right)
	    {
		    const World::Order &a = left.second;
		    const World::Order &b = right.second;
		    const auto price = [](const World::Order &order)
		    { return order.side == World::Side::Sell ? order.limit : -order.limit; };
		    return std::tuple{a.itemId, a.side != World::Side::Sell, price(a), a.sequence} <
		           std::tuple{b.itemId, b.side != World::Side::Sell, price(b), b.sequence};
	    });

	std::string book = std::format("{} at step {}:", venueId, *HostedStep());
	if (orders.empty())
		book += " no resting orders";
	for (const auto &[orderId, order] : orders)
		book += std::format(
		    "\n  {} {} {} totes at {}, {} by {} {}", order.itemId,
		    order.side == World::Side::Buy ? "bid" : "ask", order.remaining, Price(order.limit),
		    orderId, NameOf(order.ownerKind), order.ownerId);
	return book;
}


std::string ManifestConsole::CommandStep(Arguments arguments)
{
	if (m_pRemote)
		throw std::runtime_error{"Connected: the server steps its own world; disconnect first."};
	if (m_run)
		throw std::runtime_error{"Already stepping: stop first."};
	const std::uint64_t steps = ParseSteps(arguments);
	(void)Hosted();
	const std::uint64_t from = *HostedStep();
	m_run = Run{};
	m_run->steps = steps;
	try
	{
		if (Advance(kSlice))
			return StepReport(false);
	}
	catch (const std::exception &)
	{
		m_run.reset();
		throw;
	}
	return std::format(
	    "Stepping from step {} to step {}, or until a journey ends: stop ends it sooner.", from,
	    from + steps);
}


std::string ManifestConsole::CommandStop()
{
	if (!m_run)
		throw std::runtime_error{"Not stepping."};
	return StepReport(true);
}


bool ManifestConsole::Advance(std::chrono::milliseconds budget)
{
	Jettison::Sim::SimWorld &world = Hosted().World();
	Run &run = *m_run;
	const auto until = std::chrono::steady_clock::now() + budget;
	while (run.taken < run.steps && run.ended == entt::null && std::chrono::steady_clock::now() < until)
	{
		// Quiet steps passed over at once (Jettison::Sim::Quiet), to the state stepping leaves.
		run.taken += world.FastForward(world.Clock().Step() + (run.steps - run.taken));

		// What the engine raises is presentation's; the console reports from the world instead.
		(void)world.Events().Take();
		for (const auto &[entity, journey] : world.Registry().view<const World::Journey>().each())
		{
			const bool known = std::ranges::find(run.underWay, entity) != run.underWay.end();
			if (!journey.Ended() && !known)
				run.underWay.push_back(entity);
			else if (journey.Ended() && known)
				run.ended = entity;
		}
	}
	return run.taken == run.steps || run.ended != entt::null;
}


std::string ManifestConsole::StepReport(bool stopped)
{
	Jettison::Host::Session &hosted = Hosted();
	Jettison::Sim::SimWorld &world = hosted.World();
	const entt::entity ended = std::exchange(m_run, std::nullopt)->ended;
	const Command::Requests &requests = Host::HostedServices(hosted).requests;
	std::string report =
	    std::format("{} world at step {}.", stopped ? "Stopped: hosted" : "Hosted", *HostedStep());
	if (ended != entt::null)
	{
		const auto &journey = world.Registry().get<World::Journey>(ended);
		report += std::format(
		    "\n  {}'s journey to {}: {}{} at step {}, after {:.2f} days",
		    world.Registry().get<World::Identity>(ended).id, journey.venueId, NameOf(journey.leg),
		    journey.leg == World::Leg::Failed ? std::format(", {}", NameOf(journey.failure)) : "",
		    *HostedStep(), Days(*HostedStep() - journey.started));
	}
	std::vector<std::pair<Command::Actor, std::uint64_t>> unreported;
	for (auto &[actor, sequence] : m_unreported)
	{
		if (const std::optional<Command::Completed> outcome = requests.Outcome(actor, sequence))
			report += std::format(
			    "\n  {}'s request {}, {}: {} at step {}", actor.id, sequence, outcome->request.command,
			    ResultName(outcome->result), outcome->step);
		else
			unreported.emplace_back(std::move(actor), sequence);
	}
	m_unreported = std::move(unreported);
	return report;
}


std::optional<std::pair<std::uint64_t, std::uint64_t>> ManifestConsole::Stepping() const
{
	if (!m_run)
		return std::nullopt;
	return std::pair{m_run->taken, m_run->steps};
}


std::string ManifestConsole::CommandConnect(const std::string &endpoint, const std::string &token)
{
	if (m_run)
		throw std::runtime_error{"Stepping: stop first."};
	const std::size_t colon = endpoint.rfind(':');
	if (colon == std::string::npos || colon == 0)
		throw std::runtime_error{std::format("expected host:port, got '{}'", endpoint)};
	const std::array<std::string, 1> port{endpoint.substr(colon + 1)};
	const auto number = static_cast<std::uint16_t>(ParseCount(port, 0, 0, 1, 65535));
	const std::filesystem::path servers = m_servers.empty() ? Host::DefaultServers() : m_servers;
	std::optional<std::string> key = Remote::PinnedKey(Remote::ReadServers(servers), endpoint);
	if (!key)
		throw std::runtime_error{
		    std::format("The server list {} pins no key for {}.", servers.string(), endpoint)};

	// A failed connect keeps the connection there was; a replaced one is closed, and what it had not
	// heard complete is dropped with it.
	Link link{endpoint, endpoint.substr(0, colon), number, std::move(*key), token};
	auto pRemote = Dial(link);
	if (m_pRemote)
		m_pRemote->Disconnect();
	const std::string dropped = Abandon();
	m_pRemote = std::move(pRemote);
	m_link = std::move(link);
	return std::format("Connecting to {}.{}", endpoint, dropped);
}


std::unique_ptr<Remote::Client> ManifestConsole::Dial(const Link &link) const
{
	// What the server's world content is checked against: the console's own, its content source mounted
	// as the server mounts its package.
	Jettison::Vfs::FileSystem files;
	for (const Jettison::Vfs::Mount &mount : Host::Mounts({m_contentSource, {}, {}}))
		files.AddMount(mount);
	auto pRemote = std::make_unique<Remote::Client>(link.address, link.port, link.key, link.token);
	pRemote->Hold(Jettison::Host::DescribeWorldContent(files));
	return pRemote;
}


void ManifestConsole::TakeWelcome(std::vector<std::string> &lines)
{
	// After a reconnect, what was kept goes on from its own sequences, and a welcome naming a later
	// one, another session of the actor's having sent, goes on from that.
	const Remote::Welcome &welcome = *m_pRemote->Welcomed();
	m_nextSequence = std::max(
	    welcome.nextSequence,
	    m_unsettled.empty() ? welcome.nextSequence : m_unsettled.back().sequence + 1);

	// Nothing of a session comes before its welcome.
	lines.insert(
	    lines.begin(), std::format(
	                       "Connected to {} as {}; your next request is {}.", m_link.endpoint,
	                       welcome.actorId, *m_nextSequence));

	// The ledger answers a sequence it admitted as that request: completed, with its first result, or
	// still queued. One the server never kept is admitted now, in order.
	for (Unsettled &request : m_unsettled)
		if (m_pRemote->Retry(request.sequence, request.command, request.version, request.payload))
		{
			lines.push_back(
			    std::format(
			        "Sent request {} to the server{}.", request.sequence,
			        request.sent ? " again" : ""));
			request.sent = true;
		}
}


std::string ManifestConsole::Abandon()
{
	std::string dropped;
	for (const Unsettled &request : m_unsettled)
		dropped += std::format(
		    "{}{}", dropped.empty() ? " Requests not heard complete: " : ", ", request.sequence);
	m_unsettled.clear();
	m_nextSequence.reset();
	return dropped.empty() ? dropped : dropped + ".";
}


std::string ManifestConsole::CommandDisconnect()
{
	if (!m_pRemote)
		throw std::runtime_error{"Not connected."};
	m_pRemote->Disconnect();
	(void)m_pRemote->Poll(std::chrono::milliseconds{100});
	m_pRemote.reset();
	return std::format("Disconnected from {}.{}", m_link.endpoint, Abandon());
}


std::vector<std::string> ManifestConsole::Poll()
{
	std::vector<std::string> lines;
	if (m_run)
	{
		try
		{
			if (Advance(kSlice))
				lines.push_back(StepReport(false));
		}
		catch (const std::exception &error)
		{
			// As a step the command ran would: the error, the world where the step left it.
			m_run.reset();
			lines.push_back(std::format("Stepping ended at step {}: {}", *HostedStep(), error.what()));
		}
	}
	if (!m_pRemote)
		return lines;
	const bool welcomed = m_pRemote->Welcomed().has_value();
	std::vector<Remote::Reply> replies;
	try
	{
		replies = m_pRemote->Poll();
	}
	catch (const std::exception &error)
	{
		// A server that answers in another shape than its version's: nothing more of it is read.
		lines.push_back(
		    std::format("Disconnected from {}: {}.{}", m_link.endpoint, error.what(), Abandon()));
		m_pRemote.reset();
		return lines;
	}
	for (const Remote::Reply &reply : replies)
	{
		const Command::Receipt &receipt = reply.receipt;
		switch (receipt.status)
		{
			case Command::Receipt::Status::Queued:
				lines.push_back(
				    std::format(
				        "Request {} queued in order {}: it runs at the server's next step.",
				        reply.sequence, receipt.order));
				break;
			case Command::Receipt::Status::Completed:
				lines.push_back(
				    std::format(
				        "Request {}: {} at step {}.", reply.sequence, ResultName(receipt.result),
				        receipt.step));
				break;
			case Command::Receipt::Status::Refused:
				lines.push_back(
				    std::format("Request {} refused: {}.", reply.sequence, ResultName(receipt.result)));

				// A refusal consumes no sequence, nor any sent after it, and is not sent again.
				m_nextSequence = reply.nextSequence;
				break;
		}
		if (receipt.status != Command::Receipt::Status::Queued)
			std::erase_if(
			    m_unsettled,
			    [&](const Unsettled &request) { return request.sequence == reply.sequence; });
	}
	if (!welcomed && m_pRemote->Welcomed())
		TakeWelcome(lines);
	if (!m_pRemote->Closed())
		return lines;

	// A connection lost after a welcome on this link is dialled again, and what it had not heard
	// complete is sent again once welcomed; a dial that fails says nothing more.
	const std::optional<std::string> &refused = m_pRemote->Refused();
	const Jettison::Net::Reason reason = *m_pRemote->Closed();
	const std::string why = refused ? *refused : std::string{Jettison::Net::ReasonName(reason)};
	if (!refused && m_nextSequence && Redials(reason))
	{
		if (m_pRemote->Welcomed())
			lines.push_back(
			    std::format("Lost the connection to {}: {}; reconnecting.", m_link.endpoint, why));
		try
		{
			m_pRemote = Dial(m_link);
			return lines;
		}
		catch (const std::runtime_error &error)
		{
			lines.push_back(
			    std::format("Disconnected from {}: {}.{}", m_link.endpoint, error.what(), Abandon()));
			m_pRemote.reset();
			return lines;
		}
	}
	lines.push_back(std::format("Disconnected from {}: {}.{}", m_link.endpoint, why, Abandon()));
	m_pRemote.reset();
	return lines;
}
} // namespace Manifest::Tools
