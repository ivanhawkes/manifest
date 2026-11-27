#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/app/RenderApplication.h>
#include <jettison/host/Session.h>

#include <manifest/host/Start.h>

#include "ClientGame.h"

// The graphical client (docs/client.md): Manifest's game started by Jettison's shell through a session
// of its own, over the engine's assets, with Manifest's content mounted as the shell's.

namespace
{
// The scenario the first loop is played in, unless --scenario names another.
constexpr std::string_view kDefaultScenario = "trading";
constexpr std::string_view kScenarioOption = "--scenario";
} // namespace


int main(int argc, char **argv)
{
	// --scenario is Manifest's, which the shell does not know and would refuse, so it is taken out
	// before the shell reads the rest.
	std::string scenario{kDefaultScenario};
	std::vector<char *> shell{argv, argv + argc};
	for (auto at = shell.begin() + 1; at != shell.end();)
	{
		if (*at != kScenarioOption)
		{
			++at;
			continue;
		}
		if (at + 1 == shell.end())
		{
			std::cerr << std::format("{}: {} needs a scenario's name\n", argv[0], kScenarioOption);
			return EXIT_FAILURE;
		}
		scenario = *(at + 1);
		at = shell.erase(at, at + 2);
	}
	std::unique_ptr<Jettison::Host::Game> world = Manifest::Host::MakeGame(scenario);
	Manifest::Client::ClientGame game{*world};
	return Jettison::App::RunRenderApplication(
	    static_cast<int>(shell.size()), shell.data(), "Manifest", JETTISON_ASSET_SUBDIRECTORY,
	    std::move(world), &game, MANIFEST_CONTENT_SUBDIRECTORY);
}
