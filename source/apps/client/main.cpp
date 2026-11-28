#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
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
#include <system_error>
#include <utility>
#include <vector>

#include <jettison/app/RenderApplication.h>
#include <jettison/host/Session.h>

#include <manifest/host/Locations.h>
#include <manifest/host/Start.h>
#include <manifest/remote/Client.h>
#include <manifest/remote/Keys.h>

#include "ClientGame.h"

// The graphical client (docs/client.md): Manifest's game started by Jettison's shell through a session
// of its own, over the engine's assets, with Manifest's content mounted as the shell's; or, with
// --connect, a dedicated server's world played over a sealed connection, the shell holding none of the
// game's.

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

// The ports a host:port may name.
constexpr std::uint16_t kLeastPort = 1;

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
// pins it. Throws std::runtime_error for an endpoint that is not one or that the list does not name.
std::unique_ptr<Manifest::Remote::Client> Connect(const std::string &endpoint, const std::string &token)
{
	const std::size_t colon = endpoint.rfind(':');
	const char *pEnd = endpoint.data() + endpoint.size();
	std::uint16_t port = 0;
	const auto parsed = colon == std::string::npos
	                        ? std::from_chars_result{endpoint.data(), std::errc::invalid_argument}
	                        : std::from_chars(endpoint.data() + colon + 1, pEnd, port);
	if (colon == 0 || parsed.ec != std::errc{} || parsed.ptr != pEnd || port < kLeastPort)
		throw std::runtime_error{std::format("expected host:port, got '{}'", endpoint)};
	const std::filesystem::path servers = Manifest::Host::DefaultServers();
	std::optional<std::string> key =
	    Manifest::Remote::PinnedKey(Manifest::Remote::ReadServers(servers), endpoint);
	if (!key)
		throw std::runtime_error{
		    std::format("the server list {} pins no key for {}", servers.string(), endpoint)};
	return std::make_unique<Manifest::Remote::Client>(
	    endpoint.substr(0, colon), port, std::move(*key), token);
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
		std::unique_ptr<Jettison::Host::Game> world =
		    Manifest::Host::MakeGame(options[kScenario].value.value_or(std::string{kDefaultScenario}));
		Manifest::Client::ClientGame game{*world};
		return run(std::move(world), game);
	}

	std::unique_ptr<Manifest::Remote::Client> pRemote;
	try
	{
		pRemote = Connect(*connect, ReadToken(*options[kTokenFile].value));
	}
	catch (const std::exception &error)
	{
		std::cerr << std::format("{}: {}\n", argv[0], error.what());
		return EXIT_FAILURE;
	}

	// Connected, the shell holds no world of the game's: the server steps its own.
	Manifest::Client::ClientGame game{*pRemote};
	const int status = run(nullptr, game);
	if (!pRemote->Closed())
	{
		pRemote->Disconnect();
		try
		{
			(void)pRemote->Poll(kFarewell);
		}
		catch (const std::exception &error)
		{
			std::cerr << std::format("{}: the server's last message: {}\n", argv[0], error.what());
		}
	}
	return status;
}
