#include "ManifestConsole.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/core/Log.h>
#include <jettison/net/Key.h>
#include <jettison/net/Token.h>

#include <manifest/host/Start.h>
#include <manifest/query/Query.h>
#include <manifest/remote/Keys.h>
#include <manifest/remote/Server.h>
#include <manifest/remote/Tokens.h>

#include "TestHelpers.h"

using Jettison::DevTools::ConsoleLine;
using Manifest::Tools::ManifestConsole;

namespace
{
// A run of steps the command started is carried to its end, as a frontend polling would.
ConsoleLine Execute(ManifestConsole &session, std::string_view command)
{
	session.GetConsole().Execute(command);
	while (session.Stepping())
		for (std::string &line : session.Poll())
			session.GetConsole().Print(std::move(line));
	const auto lines = session.GetConsole().Lines();
	REQUIRE_FALSE(lines.empty());
	return lines.back();
}

class ConfigDirectory
{
  public:
	ConfigDirectory() : m_path{m_marker.Path() + " config with spaces"}
	{
		std::filesystem::create_directory(m_path);
	}


	~ConfigDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_path, ignored);
	}


	ConfigDirectory(const ConfigDirectory &) = delete;


	ConfigDirectory &operator=(const ConfigDirectory &) = delete;


	const std::filesystem::path &Path() const
	{
		return m_path;
	}

  private:
	Manifest::Test::TempFile m_marker{""};
	std::filesystem::path m_path;
};
} // namespace


TEST_CASE("Manifest console: help, completion, history and clear use the shell's console")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	auto &console = session.GetConsole();
	CHECK(
	    console.Complete("s") ==
	    std::vector<std::string>{"service", "show", "skills", "status", "step", "stop"});
	CHECK(console.CompleteLine("gen") == "generate ");
	CHECK(console.CompleteLine("generate 2") == "generate 2");
	CHECK(Execute(session, "help generate").text.find("generate [count] [seed]") != std::string::npos);
	CHECK(Execute(session, "vars").kind == ConsoleLine::Kind::Input);

	Execute(session, "status");
	Execute(session, "status");
	CHECK(console.History() == std::vector<std::string>{"help generate", "vars", "status"});
	console.Execute("clear");
	CHECK(console.Lines().empty());
	CHECK(console.History().back() == "clear");
	CHECK(Execute(session, "version").kind == ConsoleLine::Kind::Output);
}


TEST_CASE(
    "Manifest console: malformed and unavailable commands are errors and leave the session usable")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	for (const std::string command :
	     {"unknown", "load \"unfinished", "generate", "tick", "list", "skills", "show", "upgrade",
	      "status extra", "quit extra"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
		CHECK_FALSE(session.IsQuitRequested());
	}
	CHECK(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	CHECK(session.IsConfigurationLoaded());
}


TEST_CASE("Manifest console: quoted paths and civilisation names reach the command as one argument")
{
	ConfigDirectory directory;
	std::filesystem::copy(
	    MANIFEST_MOD_DIRECTORY, directory.Path(), std::filesystem::copy_options::recursive);

	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	CHECK(
	    Execute(session, std::format("  load \"{}\"  ", directory.Path().string())).kind ==
	    ConsoleLine::Kind::Output);
	CHECK(session.ContentSource() == directory.Path().string());
	REQUIRE(Execute(session, "generate 3 42").kind == ConsoleLine::Kind::Output);
	const auto civilisation = session.GetSimulation().GetCivilisations().front();
	const auto shown = Execute(session, std::format("show \"{}\"", civilisation.name));
	CHECK(shown.kind == ConsoleLine::Kind::Output);
	CHECK(shown.text.find(civilisation.id + " " + civilisation.name) != std::string::npos);
	CHECK(Execute(session, "show nonexistent").kind == ConsoleLine::Kind::Error);
}


TEST_CASE("Manifest console: the directory and the pack load the same content")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	const std::string directory = Execute(session, "load").text;
	const std::string pack = Execute(session, std::format("load \"{}\"", MANIFEST_MOD_PACK)).text;
	CHECK(pack.substr(pack.find(": ")) == directory.substr(directory.find(": ")));
}


TEST_CASE("Manifest console: invalid numeric arguments never silently advance or regenerate")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	REQUIRE(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "generate 2 0").kind == ConsoleLine::Kind::Output);
	for (const std::string command :
	     {"tick -1", "tick 0", "tick 2junk", "tick 1 2", "tick 1001", "tick 99999999999999999999",
	      "tick \"\"", "generate 0", "generate 501", "generate 3 -1", "generate 3 4294967296",
	      "generate 3 1 extra", "prices nope", "plan 101"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
		CHECK(session.GetSimulation().GetTick() == 0);
		CHECK(session.GetSimulation().GetCivilisations().size() == 2);
	}
	CHECK(Execute(session, "tick 2").kind == ConsoleLine::Kind::Output);
	CHECK(session.GetSimulation().GetTick() == 2);
}


TEST_CASE(
    "Manifest console: the development workflow reports output and rejected upgrades report errors")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	for (const std::string command :
	     {"load", "generate", "list", "show Civ-01", "prices 3", "tick", "plan 3", "skills",
	      "upgrade SkillMining"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Output);
	}
	CHECK(Execute(session, "upgrade unknown").kind == ConsoleLine::Kind::Error);
	CHECK(Execute(session, "status").text.find("resources 19") != std::string::npos);
}


TEST_CASE(
    "Manifest console: a failed reload preserves the session and a successful reload resets its world")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	REQUIRE(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "generate 2 42").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "tick").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "upgrade SkillMining").kind == ConsoleLine::Kind::Output);
	const auto status = Execute(session, "status").text;

	// A late type fails: the ages, categories, items and skills before it read.
	ConfigDirectory broken;
	std::filesystem::copy(
	    MANIFEST_MOD_DIRECTORY, broken.Path(), std::filesystem::copy_options::recursive);
	std::ofstream{broken.Path() / "defs/game-modes/manifest.yaml", std::ios::trunc}
	    << "version: 1\nitems: [{itemId: ItemNowhere}]\n";
	const ConsoleLine failed = Execute(session, std::format("load \"{}\"", broken.Path().string()));
	CHECK(failed.kind == ConsoleLine::Kind::Error);
	MANIFEST_CHECK_CONTAINS(failed.text, "'ItemNowhere' names nothing");
	CHECK(session.IsConfigurationLoaded());
	CHECK(session.ContentSource() == MANIFEST_MOD_DIRECTORY);
	CHECK(Execute(session, "status").text == status);
	CHECK(session.GetSimulation().GetTick() == 1);

	CHECK(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	CHECK_FALSE(session.GetSimulation().IsGenerated());
	CHECK(Execute(session, "tick").kind == ConsoleLine::Kind::Error);
	CHECK(Execute(session, "generate").kind == ConsoleLine::Kind::Output);
}


TEST_CASE(
    "Manifest console: engine logs enter the shared scrollback and both exit names end the session")
{
	for (const std::string command : {"quit", "exit"})
	{
		ManifestConsole session{MANIFEST_MOD_DIRECTORY};
		constexpr Jettison::Core::Category kTest{"manifest_console"};
		Jettison::Core::Log(Jettison::Core::Severity::Warning, kTest, "test warning");
		const auto lines = session.GetConsole().Lines();
		REQUIRE(lines.size() == 1);
		CHECK(lines[0].kind == ConsoleLine::Kind::Log);
		CHECK(lines[0].severity == Jettison::Core::Severity::Warning);
		CHECK(lines[0].category == "manifest_console");
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Output);
		CHECK(session.IsQuitRequested());
	}
}


TEST_CASE("Manifest console: hosted commands go through admission and queries, apart from the sandbox")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	for (const std::string command :
	     {"observe", "provision Ada Co", "cargo load Ada Ship1 VenueOreHighport ItemCopper 1", "step"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}
	CHECK_FALSE(session.HostedStep());
	REQUIRE(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "generate 2 0").kind == ConsoleLine::Kind::Output);

	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	CHECK(session.HostedStep() == 0);
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "provision Ada \"Ada Co\" \"Kestrel\"").text, "Host's request 1");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, "no company");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step").text, "Manifest.Company.Provision: Succeeded at step 0");
	const std::string adas = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(adas, "Company1 'Ada Co': 25000 credits");
	MANIFEST_CHECK_CONTAINS(adas, "Ship1 'Kestrel' HullLoneTraderStarter, docked at VenueOreHighport");

	// The hosted command's own rules answer: the company has no copper at the venue.
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "cargo load Ada Ship1 VenueOreHighport ItemCopper 3").text, "Ada's request 1");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "InsufficientStock at step 1");

	// A principal the world does not have is refused at admission and queues nothing.
	CHECK(
	    Execute(session, "cargo load Bea Ship1 VenueOreHighport ItemCopper 3").text ==
	    "Refused: NotAuthorised.");
	CHECK(Execute(session, "step").text == "Hosted world at step 3.");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Bea").text, "no company");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe").text, "Company1 'Ada Co'");

	// The sandbox's tick and the hosted step move only their own clocks.
	REQUIRE(Execute(session, "tick 2").kind == ConsoleLine::Kind::Output);
	CHECK(session.HostedStep() == 3);
	CHECK(session.GetSimulation().GetTick() == 2);
	MANIFEST_CHECK_CONTAINS(Execute(session, "status").text, "hosted: step 3");
}


TEST_CASE("Manifest console: provision names the ship, the company's name and 1 by default")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	CHECK(Execute(session, "provision Ada Co Kestrel extra").kind == ConsoleLine::Kind::Error);
	REQUIRE(Execute(session, "provision Bea \"Bea Co\" \"   \"").kind == ConsoleLine::Kind::Output);
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Provision: NameEmpty");
	REQUIRE(Execute(session, "provision Cy \"Cy Co\"").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "step").kind == ConsoleLine::Kind::Output);
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Cy").text, "Ship1 'Cy Co 1' HullLoneTraderStarter");
}


TEST_CASE("Manifest console: host replaces the one hosted world, and a failed host keeps it")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	CHECK(Execute(session, "host nowhere").kind == ConsoleLine::Kind::Error);
	CHECK_FALSE(session.HostedStep());
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "provision Ada Co").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "step").kind == ConsoleLine::Kind::Output);
	for (const std::string command :
	     {"host nowhere", "cargo carry Ada Ship1 VenueOreHighport ItemCopper 1",
	      "cargo load Ada Ship1 VenueOreHighport ItemCopper lots", "step 0", "observe Ada extra"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
		CHECK(session.HostedStep() == 1);
	}
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, "Company1");

	// The replaced world's queued request goes with it: the new one runs nothing.
	REQUIRE(Execute(session, "provision Bea Co").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	CHECK(session.HostedStep() == 0);
	CHECK(Execute(session, "step").text == "Hosted world at step 1.");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe").text, "no company");
}


TEST_CASE("Manifest console: a long step goes on beside the console, which stop ends")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "provision Ada \"Ada Co\"").kind == ConsoleLine::Kind::Output);
	Execute(session, "step");
	CHECK(Execute(session, "stop").kind == ConsoleLine::Kind::Error);

	// A line as a frontend runs it, the run it starts left to go on.
	const auto run = [&](std::string_view command)
	{
		session.GetConsole().Execute(command);
		return session.GetConsole().Lines().back();
	};

	// Sixty days are far more than a slice: the command answers at once and the run goes on.
	CHECK(run("step 60d").text.starts_with("Stepping from step 1 to step 311040001"));
	REQUIRE(session.Stepping());
	CHECK(session.Stepping()->second == 311040000);
	(void)session.Poll();
	const std::uint64_t taken = session.Stepping()->first;
	CHECK(taken > 0);
	CHECK(session.HostedStep() == 1 + taken);

	// The console takes commands between slices; a request queued now runs in the next slice.
	MANIFEST_CHECK_CONTAINS(run("observe Ada").text, "Company1 'Ada Co'");
	run("cargo load Ada Ship1 VenueOreHighport ItemCopper 1");
	(void)session.Poll();
	for (const std::string command : {"step", "host trading", "connect 127.0.0.1:1 token"})
	{
		CAPTURE(command);
		CHECK(run(command).kind == ConsoleLine::Kind::Error);
	}
	const ConsoleLine stopped = run("stop");
	CHECK(stopped.text.starts_with(
	    std::format("Stopped: hosted world at step {}.", *session.HostedStep())));
	MANIFEST_CHECK_CONTAINS(stopped.text, "Manifest.Company.Load: InsufficientStock");
	CHECK_FALSE(session.Stepping());
	CHECK(run("stop").kind == ConsoleLine::Kind::Error);
	CHECK(
	    Execute(session, "step").text ==
	    std::format("Hosted world at step {}.", *session.HostedStep()));
}


TEST_CASE("Manifest console: connect plays on a server as the token's actor, and disconnect leaves it")
{
	const std::string adminToken = Jettison::Net::GenerateToken();
	const std::string adaToken = Jettison::Net::GenerateToken();
	const Manifest::Test::TempFile tokens{"", ".tokens.yaml"};
	using Manifest::Command::ActorKind;
	Manifest::Remote::WriteTokens(
	    tokens.Path(), {{{Jettison::Net::DigestToken(adminToken), ActorKind::Administrator, "Operator"},
	                     {Jettison::Net::DigestToken(adaToken), ActorKind::Principal, "Ada"}}});
	const auto pHosted = Manifest::Host::Start({MANIFEST_MOD_DIRECTORY, {}, "trading"});
	const Jettison::Net::ServerKey key = Jettison::Net::GenerateServerKey();
	const Manifest::Test::TempFile keyFile{"", ".key"};
	Manifest::Remote::WriteServerKey(keyFile.Path(), key.secretKey);
	Manifest::Remote::Server server{*pHosted, tokens.Path(), keyFile.Path(), "127.0.0.1", 0};
	const std::string endpoint = std::format("127.0.0.1:{}", server.Port());

	// The same server by another name, pinned to another server's key.
	const std::string impostor = std::format("localhost:{}", server.Port());
	const Manifest::Test::TempFile servers{std::format(
	    "version: 1\nservers:\n  - endpoint: {}\n    key: {}\n  - endpoint: {}\n    key: {}\n",
	    endpoint, key.publicKey, impostor, Jettison::Net::GenerateServerKey().publicKey)};

	// Both ends polled, the world stepped when asked, until the console prints the line.
	ManifestConsole session{MANIFEST_MOD_DIRECTORY, servers.Path()};
	const auto await = [&](std::string_view expected, bool step = false)
	{
		if (step)
		{
			pHosted->World().Step();
			server.Publish();
		}
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
		while (std::chrono::steady_clock::now() < deadline)
		{
			server.Poll(std::chrono::milliseconds{1});
			for (const std::string &line : session.Poll())
				if (line.find(expected) != std::string::npos)
					return line;
		}
		return std::string{"nothing like: "} + std::string{expected};
	};

	CHECK(
	    Execute(session, "connect " + endpoint + " " + Jettison::Net::GenerateToken()).text ==
	    "Connecting to " + endpoint + ".");
	CHECK(await("Disconnected") == "Disconnected from " + endpoint + ": bad credential.");

	// A server the list does not name is not connected to, and one pinned to another key is left.
	CHECK(
	    Execute(session, "connect 127.0.0.1:1 " + adminToken).text ==
	    std::format("connect: The server list {} pins no key for 127.0.0.1:1.", servers.Path()));
	CHECK(session.Connected() == nullptr);
	Execute(session, "connect " + impostor + " " + adminToken);
	CHECK(await("Disconnected") == "Disconnected from " + impostor + ": key mismatch.");

	// A console whose content is not the server's world is refused, the package named.
	const ConfigDirectory modded;
	std::filesystem::copy(
	    MANIFEST_MOD_DIRECTORY, modded.Path(), std::filesystem::copy_options::recursive);
	std::ofstream{modded.Path() / "defs" / "items" / "item-copper.patch.yaml"}
	    << "version: 1\nbasePrice: \"12.25\"\n";
	ManifestConsole elsewhere{modded.Path().string(), servers.Path()};
	Execute(elsewhere, "connect " + endpoint + " " + adaToken);
	std::string refused = "nothing";
	for (const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
	     refused == "nothing" && std::chrono::steady_clock::now() < deadline;)
	{
		server.Poll(std::chrono::milliseconds{1});
		for (const std::string &line : elsewhere.Poll())
			refused = line;
	}
	CHECK(
	    refused ==
	    "Disconnected from " + endpoint +
	        ": This client's 'manifest' 0.0.1 is not the server's: its world content differs.");
	CHECK(elsewhere.Connected() == nullptr);

	Execute(session, "connect " + endpoint + " " + adminToken);
	CHECK(await("Connected") == "Connected to " + endpoint + " as Operator; your next request is 1.");

	// The server steps its own world: no one connected steps it, an administrator included.
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "the server steps its own world");
	CHECK(Execute(session, "provision Ada \"Ada Freight\"").text == "Sent request 1 to the server.");
	CHECK(await("queued").starts_with("Request 1 queued in order 1"));
	CHECK(await("Request 1:", true) == "Request 1: Succeeded at step 0.");

	// A player's connection replaces the administrator's: cargo needs no principal, and observe
	// shows the server's view of the player, never another's.
	Execute(session, "connect " + endpoint + " " + adaToken);
	CHECK(await("Connected") == "Connected to " + endpoint + " as Ada; your next request is 1.");
	CHECK(
	    Execute(session, "cargo load Ada Ship1 VenueOreHighport ItemCopper 1").kind ==
	    ConsoleLine::Kind::Error);
	CHECK(Execute(session, "observe Bea").kind == ConsoleLine::Kind::Error);
	Execute(session, "cargo load Ship1 VenueOreHighport ItemCopper 1");
	CHECK(await("queued").starts_with("Request 1 queued in order 2"));
	CHECK(await("Request 1:", true) == "Request 1: InsufficientStock at step 1.");
	Execute(session, "trade buy VenueOreHighport ItemFurs 1 0.5");
	CHECK(await("queued").starts_with("Request 2 queued"));
	CHECK(await("Request 2:", true) == "Request 2: Succeeded at step 2.");
	CHECK(Execute(session, "book VenueOreHighport").kind == ConsoleLine::Kind::Error);

	// Snapshots go out at most once an interval, so the step's may follow its reply.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
	while ((!session.Connected()->Latest() || session.Connected()->Latest()->step != 3) &&
	       std::chrono::steady_clock::now() < deadline)
	{
		server.Publish();
		server.Poll(std::chrono::milliseconds{1});
		(void)session.Poll();
	}
	const std::string seen = Execute(session, "observe").text;
	CHECK(seen.starts_with("step 3, as Ada\nCompany1 'Ada Freight'"));
	MANIFEST_CHECK_CONTAINS(seen, ": buy ItemFurs at VenueOreHighport, 1 of 1 totes left at 0.5");
	CHECK(
	    Execute(session, "status").text.find("connected: to " + endpoint + " as Ada") !=
	    std::string::npos);

	// The new commands send as the token's actor too, with no principal.
	CHECK(Execute(session, "refuel Ada Ship1 off").kind == ConsoleLine::Kind::Error);
	CHECK(Execute(session, "refuel Ship1 off").text == "Sent request 3 to the server.");
	CHECK(await("queued").starts_with("Request 3 queued"));
	CHECK(await("Request 3:", true) == "Request 3: Succeeded at step 3.");
	CHECK(Execute(session, "policy Ship1 on 0 0 50 on").text == "Sent request 4 to the server.");
	CHECK(Execute(session, "tender bid T Company1").kind == ConsoleLine::Kind::Error);

	// The last request was not heard complete: it is named as it is dropped.
	CHECK(
	    Execute(session, "disconnect").text ==
	    "Disconnected from " + endpoint + ". Requests not heard complete: 4.");
	CHECK(session.Connected() == nullptr);
	CHECK(Execute(session, "disconnect").kind == ConsoleLine::Kind::Error);

	// Back to the local harness: observe needs a hosted world again.
	CHECK(Execute(session, "observe").kind == ConsoleLine::Kind::Error);
}


TEST_CASE(
    "Manifest console: a request not heard complete is sent again once reconnected, and runs once")
{
	const std::string adaToken = Jettison::Net::GenerateToken();
	const Manifest::Test::TempFile tokens{"", ".tokens.yaml"};
	using Manifest::Command::ActorKind;
	Manifest::Remote::WriteTokens(
	    tokens.Path(), {{{Jettison::Net::DigestToken(adaToken), ActorKind::Principal, "Ada"}}});
	const auto pHosted = Manifest::Host::Start({MANIFEST_MOD_DIRECTORY, {}, "trading"});
	const Jettison::Net::ServerKey key = Jettison::Net::GenerateServerKey();
	const Manifest::Test::TempFile keyFile{"", ".key"};
	Manifest::Remote::WriteServerKey(keyFile.Path(), key.secretKey);
	std::optional<Manifest::Remote::Server> server;
	server.emplace(*pHosted, tokens.Path(), keyFile.Path(), "127.0.0.1", 0);
	const std::uint16_t port = server->Port();
	const std::string endpoint = std::format("127.0.0.1:{}", port);
	const Manifest::Test::TempFile servers{
	    std::format("version: 1\nservers:\n  - endpoint: {}\n    key: {}\n", endpoint, key.publicKey)};

	// Both ends polled until the console prints the line, every line it printed kept and read in order.
	ManifestConsole session{MANIFEST_MOD_DIRECTORY, servers.Path()};
	std::vector<std::string> printed;
	std::size_t read = 0;
	const auto await = [&](std::string_view expected, bool step = false)
	{
		if (step)
		{
			pHosted->World().Step();
			server->Publish();
		}
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
		do
		{
			for (; read < printed.size(); ++read)
				if (printed[read].find(expected) != std::string::npos)
					return printed[read++];
			if (server)
				server->Poll(std::chrono::milliseconds{1});
			std::ranges::move(session.Poll(), std::back_inserter(printed));
		} while (std::chrono::steady_clock::now() < deadline);
		return std::string{"nothing like: "} + std::string{expected};
	};

	// A request the server refuses is dropped: Ada has no company yet.
	Execute(session, "connect " + endpoint + " " + adaToken);
	CHECK(await("Connected") == "Connected to " + endpoint + " as Ada; your next request is 1.");
	CHECK(
	    Execute(session, "cargo load Ship1 VenueOreHighport ItemCopper 1").text ==
	    "Sent request 1 to the server.");
	CHECK(await("refused") == "Request 1 refused: NotAuthorised.");
	Execute(session, "provision Ada \"Ada Freight\"");
	CHECK(await("queued").starts_with("Request 1 queued in order 1"));
	CHECK(await("Request 1:", true) == "Request 1: Succeeded at step 0.");

	// A buy that has run and one still queued, neither heard complete when the server goes.
	CHECK(
	    Execute(session, "trade buy VenueOreHighport ItemFurs 1 0.5").text ==
	    "Sent request 2 to the server.");
	CHECK(await("queued").starts_with("Request 2 queued in order 2"));
	pHosted->World().Step();
	const Manifest::Host::Services services = Manifest::Host::HostedServices(*pHosted);
	const auto company = [&]
	{
		return Manifest::Query::Observe(
		           pHosted->World(), services.identities, {ActorKind::Principal, "Ada"})
		    .companies.at(0);
	};
	const Manifest::Query::CompanyView bought = company();
	REQUIRE(bought.orders.size() == 1);
	Execute(session, "refuel Ship1 off");
	CHECK(await("queued").starts_with("Request 3 queued in order 3"));
	server.reset();
	CHECK(await("Lost") == "Lost the connection to " + endpoint + ": shutdown; reconnecting.");
	CHECK(session.Connected() != nullptr);
	MANIFEST_CHECK_CONTAINS(Execute(session, "status").text, "connected: reconnecting to " + endpoint);

	// One made while the connection is lost is kept, and sent once it is back.
	CHECK(Execute(session, "refuel Ship1 on").text == "Kept request 4: it is sent once reconnected.");
	server.emplace(*pHosted, tokens.Path(), keyFile.Path(), "127.0.0.1", port);
	const std::size_t before = printed.size();
	CHECK(await("Connected") == "Connected to " + endpoint + " as Ada; your next request is 5.");
	CHECK(await("Request 2:") == "Request 2: Succeeded at step 1.");
	CHECK(await("Request 4 queued").starts_with("Request 4 queued in order 4"));
	CHECK(await("Request 4:", true) == "Request 4: Succeeded at step 2.");
	const std::vector<std::string> after{
	    printed.begin() + static_cast<std::ptrdiff_t>(before), printed.end()};
	CHECK(std::ranges::count(after, "Sent request 2 to the server again.") == 1);
	CHECK(std::ranges::count(after, "Sent request 3 to the server again.") == 1);
	CHECK(std::ranges::count(after, "Sent request 4 to the server.") == 1);
	CHECK(std::ranges::count(after, "Request 3: Succeeded at step 2.") == 1);
	CHECK(
	    std::ranges::none_of(
	        after, [](const std::string &line) { return line.find("refused") != std::string::npos; }));

	// The buy ran once: one order resting, its credits held once, and the ledger at the next request.
	CHECK(company().orders.size() == 1);
	CHECK(company().credits == bought.credits);
	CHECK(services.requests.NextSequence({ActorKind::Principal, "Ada"}) == 5);
	CHECK(Execute(session, "disconnect").text == "Disconnected from " + endpoint + ".");
}


namespace
{
// The staged content with the Manifest mode's cruise taken out, so its drives keep the power law
// Astralis cruises by (D-088): Ore to Brine in minutes of simulation rather than days (D-090).
class CompressedContent
{
  public:
	CompressedContent()
	{
		std::filesystem::copy(
		    MANIFEST_MOD_DIRECTORY, m_directory.Path(), std::filesystem::copy_options::recursive);
		const std::filesystem::path mode = m_directory.Path() / "defs" / "game-modes" / "manifest.yaml";
		std::ifstream in{mode};
		std::string kept;
		bool cruise = false;
		for (std::string line; std::getline(in, line);)
		{
			cruise = line.starts_with("cruise:") || (cruise && line.starts_with(' '));
			if (!cruise)
				kept += line + "\n";
		}
		in.close();
		std::ofstream{mode} << kept;
	}


	std::string Path() const
	{
		return m_directory.Path().string();
	}

  private:
	ConfigDirectory m_directory;
};


// The price of the book's best order on a side, as book prints it.
std::string BestPrice(const std::string &book, std::string_view side)
{
	const std::size_t line = book.find(std::format(" {} ", side));
	REQUIRE(line != std::string::npos);
	const std::size_t at = book.find(" at ", line) + 4;
	return book.substr(at, book.find(',', at) - at);
}


// The best price on a side of a good in the player's report of a venue, as observe shows it.
std::string Reported(
    const std::string &view, std::string_view venue, std::string_view item, std::string_view side)
{
	const std::size_t report = view.find(std::format("\n  {}: ", venue));
	REQUIRE(report != std::string::npos);
	const std::size_t line = view.find(std::format("\n    {}: ", item), report);
	REQUIRE(line != std::string::npos);
	const std::size_t at = view.find(std::format(" {} ", side), line) + side.size() + 2;
	return view.substr(at, view.find(' ', at) - at);
}
} // namespace


TEST_CASE("Manifest console: the vertical slice, furs bought at Ore, flown to Brine and sold there")
{
	const CompressedContent content;
	ManifestConsole session{content.Path()};
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "provision Ada \"Ada Co\"").kind == ConsoleLine::Kind::Output);
	Execute(session, "step");
	for (const std::string command : {"step 0", "step 61d", "step 6x", "step d", "step -1"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}
	CHECK(Execute(session, "book VenueNowhere").kind == ConsoleLine::Kind::Error);
	CHECK(
	    Execute(session, "trade buy Ada VenueOreHighport ItemFurs 2 cheap").kind ==
	    ConsoleLine::Kind::Error);
	CHECK(session.HostedStep() == 1);

	// Ada's markets: Ore live where her ship is docked, Brine her starting report.
	const std::string spawned = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(spawned, "markets (any other is unknown):");
	MANIFEST_CHECK_CONTAINS(spawned, "\n  VenueOreHighport: live");
	MANIFEST_CHECK_CONTAINS(spawned, "\n    fees: battlecruiser 77000 to dock and 15400 an hour,");
	MANIFEST_CHECK_CONTAINS(
	    spawned, ", titan 495000 to dock and 99000 an hour, 2 an hour for each connected occupant, "
	             "arrears at 1000 bps a year\n    ItemCopper: ");
	MANIFEST_CHECK_CONTAINS(
	    spawned, "VenueBrineHighport: stale starting report, observed 0.0 simulation minutes ago");
	CHECK(Execute(session, "observe").text.find("markets") == std::string::npos);

	// Bought at the ask her live report shows, the broker's, then loaded: the goods are the
	// company's at Ore, then the ship's.
	const std::string ore = Execute(session, "book VenueOreHighport ItemFurs").text;
	MANIFEST_CHECK_CONTAINS(ore, "by Broker VenueOreHighport");
	CHECK(Reported(spawned, "VenueOreHighport", "ItemFurs", "ask") == BestPrice(ore, "ask"));
	Execute(
	    session, "trade buy Ada VenueOreHighport ItemFurs 2 " +
	                 Reported(spawned, "VenueOreHighport", "ItemFurs", "ask"));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Trade.Place: Succeeded");
	const std::string bought = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(bought, "Company1 'Ada Co': 22538 credits");
	MANIFEST_CHECK_CONTAINS(bought, "at VenueOreHighport: ItemFurs 2 totes available");
	Execute(session, "cargo load Ada Ship1 VenueOreHighport ItemFurs 2");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Load: Succeeded");

	// The journey shows in the snapshot as it goes, and stepping stops as it ends.
	Execute(session, "travel Ada Ship1 VenueBrineHighport");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step 2m").text, "Manifest.Travel.Travel: Succeeded at step 3");
	const std::string cruising = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(
	    cruising, "Ship1 'Ada Co 1' HullLoneTraderStarter, undocked, Cruising to VenueBrineHighport");
	MANIFEST_CHECK_CONTAINS(cruising, "ItemFurs 2 totes");
	CHECK(
	    Execute(session, "cargo unload Ada Ship1 VenueBrineHighport ItemFurs 2").kind ==
	    ConsoleLine::Kind::Output);
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Unload: NotDocked");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step 1d").text, "Ship1's journey to VenueBrineHighport: Arrived at step");
	CHECK(session.HostedStep() < 7204 + 60 * 60 * 24 * 60);
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Ada").text,
	    "Ship1 'Ada Co 1' HullLoneTraderStarter, docked at VenueBrineHighport\n");

	// Unloaded and sold a tote at a time at the bid her report of Brine, live now, shows, the
	// broker's ladder two totes a level there, both at its first, 1,949 credits each less 8 in
	// fees; Ore's is stale.
	Execute(session, "cargo unload Ada Ship1 VenueBrineHighport ItemFurs 2");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Unload: Succeeded");
	const std::string arrived = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(arrived, "VenueBrineHighport: live");
	MANIFEST_CHECK_CONTAINS(arrived, "VenueOreHighport: stale, observed");

	// Its households bought a tote over the cruise, and its bids rose with it.
	MANIFEST_CHECK_CONTAINS(arrived, "ItemFurs: bid 103.971304 (2 totes, 4 prices)");
	for (int tote = 0; tote < 2; ++tote)
	{
		Execute(
		    session,
		    "trade sell Ada VenueBrineHighport ItemFurs 1 " +
		        Reported(
		            Execute(session, "observe Ada").text, "VenueBrineHighport", "ItemFurs", "bid"));
		MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Trade.Place: Succeeded");
	}
	const std::string sold = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(sold, "Company1 'Ada Co': 26320 credits");
	CHECK(sold.substr(0, sold.find("\nmarkets")).find("ItemFurs") == std::string::npos);
	MANIFEST_CHECK_CONTAINS(sold, "ItemFurs: bid");
	MANIFEST_CHECK_CONTAINS(sold, "totes sold, last sold at");

	// A bid under the market rests in the player's view until its cancel.
	Execute(session, "trade buy Ada VenueBrineHighport ItemFurs 1 1");
	Execute(session, "step");
	const std::string resting = Execute(session, "observe Ada").text;
	const std::size_t order = resting.find("\n  Order");
	REQUIRE(order != std::string::npos);
	MANIFEST_CHECK_CONTAINS(resting, ": buy ItemFurs at VenueBrineHighport, 1 of 1 totes left at 1");
	const std::string orderId = resting.substr(order + 3, resting.find(':', order) - order - 3);
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "book VenueBrineHighport ItemFurs").text, orderId + " by Company Company1");
	Execute(session, "cancel Ada " + orderId);
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Trade.Cancel: Succeeded");
	const std::string cancelled = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(cancelled, "26320 credits");
	CHECK(cancelled.find("Order") == std::string::npos);
}


TEST_CASE("Manifest console: a ship's life support in the view, and its resupply sent as service")
{
	const CompressedContent content;
	ManifestConsole session{content.Path()};
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "provision Ada \"Ada Co\"").kind == ConsoleLine::Kind::Output);
	Execute(session, "step");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Ada").text,
	    "life support: 30.000 kg of oxygen in 3 tanks, cartridges of 30.000 occupant-days, "
	    "connected at VenueOreHighport");
	for (const std::string command : {"service fill Ada Ship1", "service refill", "service refill Ada"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}
	Execute(session, "service refill Ada Ship1");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Refill: CapacityExceeded");
	Execute(session, "service replace Ada Ship1");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step").text, "Manifest.Company.Replace: InsufficientStock");
}


TEST_CASE("Manifest console: a ship's hold and systems in the view, and an upgrade sent as fit")
{
	const CompressedContent content;
	ManifestConsole session{content.Path()};
	REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "provision Ada \"Ada Co\"").kind == ConsoleLine::Kind::Output);
	Execute(session, "step");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, "hold of 160 totes");
	for (const std::string command :
	     {"fit Ada Ship1 OfferCargoHolds", "fit Ada Ship1 OfferCargoHolds none", "fit Ada"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}

	// A new company's credits are short of the offer's (D-096).
	Execute(session, "fit Ada Ship1 OfferCargoHolds 1");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step").text, "Manifest.Company.Upgrade: InsufficientFunds");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, "Company1 'Ada Co': 25000 credits");
}


namespace
{
// The staged content with a founding a new company can pay, its price 20,000 credits for the 25,000
// it starts with, so that a test founds a region through the console.
class CheapFoundingContent
{
  public:
	CheapFoundingContent()
	{
		std::filesystem::copy(
		    MANIFEST_MOD_DIRECTORY, m_directory.Path(), std::filesystem::copy_options::recursive);
		const std::filesystem::path tuning = m_directory.Path() / "defs" / "tuning" / "balance.yaml";
		std::ifstream in{tuning};
		std::string text{std::istreambuf_iterator<char>{in}, {}};
		in.close();
		const std::string price = "foundingPrice: 200000";
		REQUIRE(text.find(price) != std::string::npos);
		text.replace(text.find(price), price.size(), "foundingPrice: 20000");
		std::ofstream{tuning} << text;
	}


	std::string Path() const
	{
		return m_directory.Path().string();
	}

  private:
	ConfigDirectory m_directory;
};


// A hosted governance world with Ada's company provisioned and stepped once.
void HostGovernance(ManifestConsole &session)
{
	REQUIRE(Execute(session, "host governance").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "provision Ada \"Ada Co\"").kind == ConsoleLine::Kind::Output);
	Execute(session, "step");
}


// The ID of the tender the snapshot lists whose ID starts with the key.
std::string TenderFor(const std::string &view, std::string_view key)
{
	const std::size_t start = view.find(std::format("\n  {}", key));
	REQUIRE(start != std::string::npos);
	return view.substr(start + 3, view.find(": ", start) - start - 3);
}
} // namespace


TEST_CASE("Manifest console: a ship's policy and refuelling sent, its fuel, policy and job in the view")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	HostGovernance(session);
	const std::string before = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(before, "fuel: 4000.000 of 4000.000 kg of reaction mass");
	MANIFEST_CHECK_CONTAINS(before, "kg of pellets");
	CHECK(before.find("policy:") == std::string::npos);
	CHECK(before.find("job:") == std::string::npos);
	for (const std::string command :
	     {"policy", "policy Ada Ship1 on", "policy Ada Ship1 maybe 0 0", "policy Ada Ship1 on -1 0",
	      "policy Ada Ship1 on 0 0 101", "policy Ada Ship1 on 0 0 50 perhaps",
	      "policy Ada Ship1 on 0 0 50 on 1", "refuel Ada Ship1", "refuel Ada Ship1 sometimes"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "policy Ada Ship1 on").text, "usage: policy [principal] <ship>");

	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "policy Ada Ship1 on 100 50 80 on").text, "Ada's request 1");
	MANIFEST_CHECK_CONTAINS(Execute(session, "refuel Ada Ship1 off").text, "Ada's request 2");
	const std::string report = Execute(session, "step").text;
	MANIFEST_CHECK_CONTAINS(report, "Manifest.Policy.Set: Succeeded");
	MANIFEST_CHECK_CONTAINS(report, "Manifest.Company.SetRefuel: Succeeded");
	const std::string after = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(
	    after, "policy: on, target 100 credits, loss allowance 50 credits, thrust 80%, contracts on");
	MANIFEST_CHECK_CONTAINS(after, "job: ");
	MANIFEST_CHECK_CONTAINS(after, "estimated at ");
	MANIFEST_CHECK_CONTAINS(after, "drawn with the lot: ");

	// Another company's ship is not theirs to set.
	Execute(session, "provision Bea \"Bea Co\"");
	Execute(session, "step");
	Execute(session, "refuel Bea Ship1 on");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.SetRefuel: NotAuthorised");
	Execute(session, "policy Bea NoShip on 0 0");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Policy.Set: UnknownTarget");
}


TEST_CASE("Manifest console: a governor founds a region, queues a project and tenders for it")
{
	const CheapFoundingContent content;
	ManifestConsole session{content.Path()};
	HostGovernance(session);
	const std::string open = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(open, "\n  RegionForgeNorth on PlanetForge: open to founding, population ");
	for (const std::string command :
	     {"found Ada RegionForgeNorth",
	      "found Ada RegionForgeNorth Company1 a b",
	      "project Ada RegionForgeNorth ProjectSmeltersGuild",
	      "project dig Ada RegionForgeNorth X",
	      "development Ada RegionForgeNorth on",
	      "development Ada RegionForgeNorth maybe 1",
	      "development Ada RegionForgeNorth on -1",
	      "tax Ada RegionForgeNorth",
	      "tax Ada RegionForgeNorth 12.5",
	      "tax Ada RegionForgeNorth -1",
	      "tender",
	      "tender swap Ada X",
	      "tender cancel Ada",
	      "tender publish Ada RegionForgeNorth ProjectSmeltersGuild ItemCopper VenueForgeHighport "
	      "1",
	      "tender publish Ada R P I V 0 100 2 3",
	      "tender publish Ada R P I V 1 -5 2 3",
	      "tender publish Ada R P I V 1 100 0 3",
	      "tender bid Ada T Company1",
	      "tender bid Ada T C 5 x",
	      "tender bid Ada T C 5 1 2",
	      "tender withdraw Ada T",
	      "tender deliver Ada T",
	      "tender abandon"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}

	// Founding pays the price from the company's credits and makes the player the region's
	// governor.
	Execute(session, "found Ada RegionForgeNorth Company1 \"Northern Republic\"");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Found: Succeeded");
	const std::string founded = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(founded, "Company1 'Ada Co': 5000 credits");
	MANIFEST_CHECK_CONTAINS(
	    founded, "RegionForgeNorth on PlanetForge: governed by Northern Republic (Organisation1), a "
	             "player's");
	MANIFEST_CHECK_CONTAINS(founded, ", treasury ");
	Execute(session, "found Ada RegionForgeNorth Company1 Again");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.Found: Governed");

	Execute(session, "project queue Ada RegionForgeNorth ProjectSmeltersGuild");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Queue: Succeeded");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Ada").text,
	    "\n    ProjectSmeltersGuild: Tendering, queued 1, worked 0 steps");

	Execute(
	    session,
	    "tender publish Ada RegionForgeNorth ProjectSmeltersGuild ItemCopper VenueForgeHighport 1 "
	    "6000 2 3");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Publish: Succeeded");
	const std::string tendered = Execute(session, "observe Ada").text;
	const std::string tender = TenderFor(tendered, "RegionForgeNorth:ProjectSmeltersGuild:ItemCopper:");
	CHECK(tender.starts_with("RegionForgeNorth:ProjectSmeltersGuild:ItemCopper:"));
	MANIFEST_CHECK_CONTAINS(
	    tendered, std::format(
	                  "{}: RegionForgeNorth for ProjectSmeltersGuild, 1 totes of ItemCopper at "
	                  "VenueForgeHighport, budget 6000 credits, Open",
	                  tender));
	Execute(session, "tender cancel Ada " + tender);
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Cancel: Succeeded");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, tender + ": RegionForgeNorth for");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Ada").text,
	    "totes of ItemCopper at VenueForgeHighport, budget 6000 credits, Cancelled");

	// Her region's wage tax, posted with its condition; above the tuning's half it is refused.
	MANIFEST_CHECK_CONTAINS(founded, ", wage tax 1000 bps, treasury ");
	Execute(session, "tax Ada RegionForgeNorth 1500");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Trade.SetWageTax: Succeeded");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, ", wage tax 1500 bps");
	Execute(session, "tax Ada RegionForgeNorth 5001");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Trade.SetWageTax: RateTooHigh");

	Execute(session, "development Ada RegionForgeNorth on 150000");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step").text, "Manifest.Procurement.SetDevelopment: Succeeded");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Ada").text, "development policy: tender on, limit 150000 credits");

	Execute(session, "project cancel Ada RegionForgeNorth ProjectSmeltersGuild");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step").text, "Manifest.Procurement.CancelProject: Succeeded");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, "ProjectSmeltersGuild: Cancelled");
}


TEST_CASE("Manifest console: a company bids on a tender, sealed in its view, and withdraws")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	HostGovernance(session);
	const std::string tender = TenderFor(
	    Execute(session, "observe Ada").text, "RegionForgeEast:ProjectBronzeMetallurgy:ItemCopper:");
	CHECK(tender.starts_with("RegionForgeEast:ProjectBronzeMetallurgy:ItemCopper:"));

	// A new bid is revision 0 and shows from 1; a replacement names the revision it replaces.
	Execute(session, std::format("tender bid Ada {} Company1 50000", tender));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Bid: Succeeded");
	const std::string bid =
	    std::format(": a sealed bid of 50000 credits on {}, revision 1, accepted at step ", tender);
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, bid);
	Execute(session, std::format("tender bid Ada {} Company1 40000 1", tender));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Bid: Succeeded");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "observe Ada").text,
	    std::format(": a sealed bid of 40000 credits on {}, revision 2,", tender));
	Execute(session, std::format("tender bid Ada {} Company1 30000 1", tender));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Bid: StaleRevision");

	// Another player does not see it, and the host sees every company's.
	Execute(session, "provision Bea \"Bea Co\"");
	Execute(session, "step");
	CHECK(Execute(session, "observe Bea").text.find("sealed bid") == std::string::npos);
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe").text, "a sealed bid of 40000 credits");

	// A contract is the award's: nothing here is awarded, so delivering and abandoning are closed.
	Execute(session, std::format("tender deliver Ada {} Ship1", tender));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Deliver: Closed");
	Execute(session, std::format("tender abandon Ada {}", tender));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Abandon: Closed");

	Execute(session, std::format("tender withdraw Ada {} Company1", tender));
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Procurement.Withdraw: Succeeded");
	CHECK(Execute(session, "observe Ada").text.find("sealed bid") == std::string::npos);
}


TEST_CASE(
    "Manifest console: a venue's fees sent as its whole posted classes, refused to one who does "
    "not "
    "operate it")
{
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
	HostGovernance(session);
	for (const std::string command :
	     {"fees", "fees Ada VenueForgeHighport 2 100", "fees Ada VenueForgeHighport 2 100 shuttle 60",
	      "fees Ada VenueForgeHighport 2 100 shuttle 60 12 titan",
	      "fees Ada VenueForgeHighport x 100 shuttle 60 12",
	      "fees Ada VenueForgeHighport 2 100 shuttle 60 -12",
	      "fees Ada VenueForgeHighport 2 3000000000 shuttle 60 12"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
	}
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "fees Ada VenueForgeHighport 2 100 shuttle 60").text,
	    "usage: fees <principal> <venue> <occupant fee> <arrears rate bps> <class> <docking> "
	    "<berth>");

	// The AI runs the venue's operator, so no principal's fees are authorised.
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "fees Ada VenueForgeHighport 2 100 shuttle 60 12 titan 495000 99000").text,
	    "Ada's request 1");
	MANIFEST_CHECK_CONTAINS(Execute(session, "step").text, "Manifest.Company.SetFees: NotAuthorised");
}


namespace
{
#if defined(NDEBUG) && !__has_feature(address_sanitizer)
constexpr bool kOptimised = true;
#else
constexpr bool kOptimised = false;
#endif
// Ore to Brine on the manned profile through the console's long step, its quiet steps passed over
// (Jettison::Sim::Quiet), in seconds: the best of three in Lythir's release build, measured again
// 2026-11-23 at W12's slice 3 (3.48 to 3.52 s over five runs). L27's 2.37 s had grown to 3.46 s
// with the economy built since, leaving under 3% before the limit. Stepped, it took 4 min 50 s.
constexpr double kJourneySeconds = 3.50;
} // namespace


// A journey dearer than half as much again as it was fails. Timed only in an optimised build
// without sanitisers, alone (its ctest runs serially), and not on CI, whose machines are not
// Lythir.
TEST_CASE("Manifest console: Ore to Brine within its time budget" * doctest::skip())
{
	if (!kOptimised || std::getenv("CI") != nullptr)
	{
		MESSAGE("untimed: not an optimised build without sanitisers, or on CI");
		return;
	}
	double best = std::numeric_limits<double>::infinity();
	for (int run = 0; run < 3; ++run)
	{
		ManifestConsole session{MANIFEST_MOD_DIRECTORY};
		REQUIRE(Execute(session, "host trading").kind == ConsoleLine::Kind::Output);
		REQUIRE(Execute(session, "provision Ada \"Ada Co\"").kind == ConsoleLine::Kind::Output);
		Execute(session, "step");
		Execute(session, "travel Ada Ship1 VenueBrineHighport");
		const auto start = std::chrono::steady_clock::now();
		MANIFEST_CHECK_CONTAINS(Execute(session, "step 5d").text, "Arrived");
		const std::chrono::duration<double> taken = std::chrono::steady_clock::now() - start;
		best = std::min(best, taken.count());
	}
	MESSAGE(std::format("{:.2f} s from Ore to Brine, against {:.2f}", best, kJourneySeconds));
	CHECK(best <= 1.5 * kJourneySeconds);
}
