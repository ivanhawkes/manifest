#include "ManifestConsole.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/core/Log.h>
#include <jettison/net/Token.h>

#include <manifest/host/Start.h>
#include <manifest/remote/Server.h>
#include <manifest/remote/Tokens.h>

#include "TestHelpers.h"

using Jettison::DevTools::ConsoleLine;
using Manifest::Tools::ManifestConsole;

namespace
{
ConsoleLine Execute(ManifestConsole &session, std::string_view command)
{
	session.GetConsole().Execute(command);
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
	CHECK(console.Complete("s") == std::vector<std::string>{"show", "skills", "status", "step"});
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
	std::ofstream{broken.Path() / "manifest/definitions/game-modes/manifest.yaml", std::ios::trunc}
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
	MANIFEST_CHECK_CONTAINS(Execute(session, "provision Ada \"Ada Co\"").text, "Host's request 1");
	MANIFEST_CHECK_CONTAINS(Execute(session, "observe Ada").text, "no company");
	MANIFEST_CHECK_CONTAINS(
	    Execute(session, "step").text, "Manifest.Company.Provision: Succeeded at step 0");
	const std::string adas = Execute(session, "observe Ada").text;
	MANIFEST_CHECK_CONTAINS(adas, "Company1 'Ada Co': 25000 credits");
	MANIFEST_CHECK_CONTAINS(adas, "Ship1 HullLoneTraderStarter, docked at VenueOreHighport");

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
	Manifest::Remote::Server server{*pHosted, tokens.Path(), "127.0.0.1", 0};
	const std::string endpoint = std::format("127.0.0.1:{}", server.Port());

	// Both ends polled, the world stepped when asked, until the console prints the line.
	ManifestConsole session{MANIFEST_MOD_DIRECTORY};
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

	Execute(session, "connect " + endpoint + " " + adminToken);
	CHECK(await("Connected") == "Connected to " + endpoint + " as Operator; your next request is 1.");
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
	CHECK(Execute(session, "observe").text.starts_with("step 2, as Ada\nCompany1 'Ada Freight'"));
	CHECK(
	    Execute(session, "status").text.find("connected: to " + endpoint + " as Ada") !=
	    std::string::npos);

	CHECK(Execute(session, "disconnect").text == "Disconnected from " + endpoint + ".");
	CHECK(session.Connected() == nullptr);
	CHECK(Execute(session, "disconnect").kind == ConsoleLine::Kind::Error);
	// Back to the local harness: observe needs a hosted world again.
	CHECK(Execute(session, "observe").kind == ConsoleLine::Kind::Error);
}
