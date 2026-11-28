#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/app/RenderApplication.h>
#include <jettison/host/Session.h>

#include <manifest/client/ServerPicker.h>
#include <manifest/host/Locations.h>
#include <manifest/host/Start.h>
#include <manifest/remote/Client.h>

#include "ClientGame.h"

// The graphical client (docs/client.md): Manifest's game started by Jettison's shell through a session
// of its own, over the engine's assets, with Manifest's content mounted as the shell's; or, with
// --connect or picked from the servers the player saved, a dedicated server's world played over a
// sealed connection: with --connect the shell holds none of the game's, and with a pick it holds the
// local world, left unplayed.

namespace
{
// The scenario the first loop is played in, unless --scenario names another.
constexpr std::string_view kDefaultScenario = "trading";

// The client's own options, which the shell does not know and would refuse, each with its value.
struct Option
{
	std::string_view name;
	std::string_view needs;
	std::optional<std::string> value;
};

constexpr std::size_t kScenario = 0;
constexpr std::size_t kConnect = 1;
constexpr std::size_t kTokenFile = 2;

// How long the client waits, as it ends, for its leaving to reach the server.
constexpr std::chrono::milliseconds kFarewell{100};


// The token, from a file so it is in no process list, its trailing white space taken off. Throws
// std::runtime_error naming the file when it cannot be read or holds none.
std::string ReadToken(const std::string &path)
{
	std::ifstream in{path};
	std::ostringstream text;
	text << in.rdbuf();
	if (!in)
		throw std::runtime_error{std::format("the token file {} cannot be read", path)};
	std::string token = text.str();
	token.erase(token.find_last_not_of(" \t\r\n") + 1);
	if (token.empty())
		throw std::runtime_error{std::format("the token file {} holds no token", path)};
	return token;
}


// The server at host:port, its key pinned from the server list (Host::DefaultServers) as the console
// pins it. Throws Client::Refused for an endpoint that is not one or that the list does not name.
std::unique_ptr<Manifest::Remote::Client> Connect(const std::string &endpoint, const std::string &token)
{
	return Manifest::Client::Dial(Manifest::Host::DefaultServers(), endpoint, token);
}


// The client's leaving told to the server it is connected to, if any.
void Leave(Manifest::Remote::Client *pRemote, const char *program)
{
	if (pRemote == nullptr || pRemote->Closed())
		return;
	pRemote->Disconnect();
	try
	{
		(void)pRemote->Poll(kFarewell);
	}
	catch (const std::exception &error)
	{
		std::cerr << std::format("{}: the server's last message: {}\n", program, error.what());
	}
}
} // namespace


int main(int argc, char **argv)
{
	// The client's options are taken out before the shell reads the rest.
	std::array<Option, 3> options{
	    {{"--scenario", "a scenario's name", {}},
	     {"--connect", "a server's host:port", {}},
	     {"--token-file", "a file holding the token", {}}}};
	std::vector<char *> shell{argv, argv + argc};
	for (auto at = shell.begin() + 1; at != shell.end();)
	{
		const auto option = std::ranges::find(options, std::string_view{*at}, &Option::name);
		if (option == options.end())
		{
			++at;
			continue;
		}
		if (at + 1 == shell.end())
		{
			std::cerr << std::format("{}: {} needs {}\n", argv[0], option->name, option->needs);
			return EXIT_FAILURE;
		}
		option->value = *(at + 1);
		at = shell.erase(at, at + 2);
	}
	const std::optional<std::string> &connect = options[kConnect].value;
	if (connect.has_value() != options[kTokenFile].value.has_value() ||
	    (connect && options[kScenario].value))
	{
		std::cerr << std::format(
		    "{}: --connect needs --token-file, and the scenario is then the server's\n", argv[0]);
		return EXIT_FAILURE;
	}

	const auto run =
	    [&](std::unique_ptr<Jettison::Host::Game> world, Manifest::Client::ClientGame &game)
	{
		return Jettison::App::RunRenderApplication(
		    static_cast<int>(shell.size()), shell.data(), "Manifest", JETTISON_ASSET_SUBDIRECTORY,
		    std::move(world), &game, MANIFEST_CONTENT_SUBDIRECTORY);
	};
	if (!connect)
	{
		// With no server and no scenario given, the player picks one or plays here (D-127).
		std::optional<Manifest::Client::ServerPicker> picker;
		try
		{
			if (!options[kScenario].value)
				picker.emplace(Manifest::Host::PlayerServers(), Manifest::Host::ShippedServers());
		}
		catch (const std::exception &error)
		{
			std::cerr << std::format("{}: {}\n", argv[0], error.what());
			return EXIT_FAILURE;
		}
		std::unique_ptr<Jettison::Host::Game> world =
		    Manifest::Host::MakeGame(options[kScenario].value.value_or(std::string{kDefaultScenario}));
		Manifest::Client::ClientGame game{*world, std::move(picker)};
		const int status = run(std::move(world), game);
		Leave(game.Model() != nullptr ? game.Model()->Connection() : nullptr, argv[0]);
		return status;
	}

	std::string token;
	std::unique_ptr<Manifest::Remote::Client> pRemote;
	try
	{
		token = ReadToken(*options[kTokenFile].value);
		pRemote = Connect(*connect, token);
	}
	catch (const std::exception &error)
	{
		std::cerr << std::format("{}: {}\n", argv[0], error.what());
		return EXIT_FAILURE;
	}

	// Connected, the shell holds no world of the game's: the server steps its own.
	// A lost connection is dialled again with the same token, its key pinned from the list again.
	Manifest::Client::ClientGame game{*pRemote, [&] { return Connect(*connect, token); }};
	const int status = run(nullptr, game);
	Leave(game.Model() != nullptr ? game.Model()->Connection() : pRemote.get(), argv[0]);
	return status;
}
