#include "ManifestConsole.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>

#include <manifest/company/Company.h>
#include <manifest/host/Start.h>
#include <manifest/query/Query.h>

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

std::string PadRight(const std::string &text, std::size_t width)
{
	return text.size() >= width ? text : text + std::string(width - text.size(), ' ');
}


std::string_view ResultName(Command::Result result)
{
	return Jettison::Reflect::EnumName(entt::meta_any{result});
}


// The administrator, as the trusted harness makes it; a principal's ID never names it.
const Command::Actor kHost{Command::ActorKind::Administrator, "Host"};
} // namespace

ManifestConsole::ManifestConsole(std::string contentSource) : m_contentSource{std::move(contentSource)}
{
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
	    "provision", "provision <principal> <company name>: queues a player's start, as the host", 2, 2,
	    [this](Arguments arguments) { return CommandProvision(arguments[0], arguments[1]); });
	RegisterCommand(
	    "cargo", "cargo load|unload <principal> <ship> <venue> <item> <IBCs>: queues a player's move",
	    6, 6, [this](Arguments arguments) { return CommandCargo(arguments); });
	RegisterCommand(
	    "step", "step [count]: advances the hosted world and reports what ran (default 1)", 0, 1,
	    [this](Arguments arguments) { return CommandStep(arguments); });
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
	       << (m_pHosted ? std::format("step {}", *HostedStep()) : std::string("none: run host"));

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
	// A failed start keeps the hosted world there was; the replaced one shuts down with its requests.
	std::unique_ptr<Jettison::Host::Session> hosted = Host::Start({m_contentSource, {}, scenario});
	m_pHosted = std::move(hosted);
	m_unreported.clear();
	return std::format("Hosting {} from {}, at step {}.", scenario, m_contentSource, *HostedStep());
}


std::string ManifestConsole::CommandObserve(Arguments arguments)
{
	Jettison::Host::Session &hosted = Hosted();
	const Command::Actor actor =
	    arguments.empty() ? kHost : Command::Actor{Command::ActorKind::Principal, arguments[0]};
	const Query::Snapshot snapshot =
	    Query::Observe(hosted.World(), Host::HostedServices(hosted).identities, actor);

	std::ostringstream view;
	view << "step " << snapshot.step << ", as " << actor.id;
	if (snapshot.companies.empty())
		view << ": no company";
	for (const Query::CompanyView &company : snapshot.companies)
	{
		view << "\n"
		     << company.companyId << " '" << company.displayName << "': " << company.credits
		     << " credits";
		for (const World::Holding &holding : company.holdings)
			view << "\n  at " << holding.venueId << ": " << holding.itemId << " " << holding.available
			     << " IBC available, " << holding.reserved << " reserved";
		for (const Query::ShipView &ship : company.ships)
		{
			view << "\n  " << ship.shipId << " " << ship.hullId << ", "
			     << (ship.dockedAt.empty() ? std::string{"undocked"} : "docked at " + ship.dockedAt);
			for (const World::Lot &lot : ship.cargo)
				view << "\n    " << lot.itemId << " " << lot.containers << " IBC";
		}
	}
	return view.str();
}


std::string ManifestConsole::Admit(
    const Command::Actor &actor, std::string_view command, std::span<const std::byte> payload)
{
	Jettison::Host::Session &hosted = Hosted();
	Command::Requests &requests = Host::HostedServices(hosted).requests;
	const std::uint64_t sequence = requests.NextSequence(actor);
	const Command::Receipt receipt = requests.Admit(
	    hosted.World(), actor, sequence, std::string{command}, Company::kVersion, payload);
	if (receipt.status == Command::Receipt::Status::Refused)
		return std::format("Refused: {}.", ResultName(receipt.result));
	m_unreported.emplace_back(actor, sequence);
	return std::format(
	    "Queued {}'s request {} in order {}: it runs at the next step.", actor.id, sequence,
	    receipt.order);
}


std::string ManifestConsole::CommandProvision(
    const std::string &principalId, const std::string &companyName)
{
	using Jettison::Serialise::BinaryArchive;
	return Admit(
	    kHost, Company::kProvision,
	    BinaryArchive::Write(
	        Company::Provision{principalId, principalId, companyName},
	        Jettison::Serialise::FormatVersion{Company::kVersion}));
}


std::string ManifestConsole::CommandCargo(Arguments arguments)
{
	if (arguments[0] != "load" && arguments[0] != "unload")
		throw std::runtime_error{std::format("expected load or unload, got '{}'", arguments[0])};
	using Jettison::Serialise::BinaryArchive;
	return Admit(
	    {Command::ActorKind::Principal, arguments[1]},
	    arguments[0] == "load" ? Company::kLoad : Company::kUnload,
	    BinaryArchive::Write(
	        Company::Move{
	            arguments[2], arguments[3], arguments[4],
	            ParseCount(arguments, 5, 0, 1, std::numeric_limits<unsigned>::max())},
	        Jettison::Serialise::FormatVersion{Company::kVersion}));
}


std::string ManifestConsole::CommandStep(Arguments arguments)
{
	Jettison::Host::Session &hosted = Hosted();
	const unsigned steps = ParseCount(arguments, 0, 1, 1, 1000);
	for (unsigned index = 0; index < steps; ++index)
		hosted.World().Step();

	const Command::Requests &requests = Host::HostedServices(hosted).requests;
	std::string report = std::format("Hosted world at step {}.", *HostedStep());
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
} // namespace Manifest::Tools
