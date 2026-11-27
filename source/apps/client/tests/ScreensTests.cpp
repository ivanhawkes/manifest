#include "TestHelpers.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include <jettison/host/Session.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/ui/UiSystem.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/host/Start.h>

#include "ClientGame.h"

using Manifest::Client::ClientGame;

namespace
{
// The frame the screens are laid out in, the shell's default window's.
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
} // namespace


// The screens check: every document loads and is laid out with nothing RmlUi or the string table
// counts as a problem, bound to the model of a started world, and a press reaches the world.
TEST_CASE("Every screen of the client loads and shows its world with no problem")
{
	const std::unique_ptr<Jettison::Host::Session> session =
	    Manifest::Host::Start({MANIFEST_MOD_DIRECTORY, {}, "trading"});
	Jettison::Vfs::FileSystem files;
	files.AddMount({"engine", MANIFEST_ENGINE_ASSETS});
	files.AddMount({"base", MANIFEST_MOD_DIRECTORY});
	Jettison::Ui::UiSystem ui{files, kWidth, kHeight};
	for (const std::string_view font : {"fonts/Roboto-Medium.ttf", "fonts/Hack-Regular.ttf"})
		ui.LoadFont(font);
	ui.LoadFont("fonts/DroidSans.ttf", true);
	ui.SetLanguage("en");

	ClientGame game{session->GetGame()};
	game.OpenScreens(ui);
	game.Attach(session->World());
	ui.Update(0.0);
	CHECK(game.Shown() == ClientGame::kStart);
	CHECK(ui.IsShown(game.Document(ClientGame::kStart)));
	CHECK(ui.ElementText(game.Document(ClientGame::kStart), "provision") == "Start trading");

	// Before a company, every screen but the start's gives way to it.
	game.Show(ClientGame::kMarket);
	CHECK(game.Shown() == ClientGame::kStart);

	REQUIRE(game.Press("provision"));
	session->World().Step();
	game.Model()->Refresh();
	game.Present();
	ui.Update(0.0);
	CHECK(game.Shown() == ClientGame::kCompany);
	CHECK(ui.ElementText(game.Document(ClientGame::kCompany), "company-name") == "Ada Co");
	CHECK(ui.ElementText(game.Document(ClientGame::kCompany), "status") == "Done.");

	for (const std::string_view screen :
	     {ClientGame::kMarket, ClientGame::kTravel, ClientGame::kService, ClientGame::kCompany})
	{
		CAPTURE(screen);
		REQUIRE(game.Press(std::string{"nav-"} + std::string{screen}));
		ui.Update(0.0);
		CHECK(game.Shown() == screen);
		CHECK(ui.IsShown(game.Document(screen)));
	}
	game.Show(ClientGame::kMarket);
	ui.Update(0.0);
	CHECK(ui.ElementText(game.Document(ClientGame::kMarket), "venue") == "VenueOreHighport");
	CHECK_FALSE(ui.ElementText(game.Document(ClientGame::kMarket), "ask-0").empty());

	// A journey begun shows the flight panel beside the screen.
	game.Show(ClientGame::kTravel);
	ui.Update(0.0);
	REQUIRE(game.Press("travel-0"));
	session->World().Step();
	game.Model()->Refresh();
	game.Present();
	ui.Update(0.0);
	CHECK(ui.IsShown(game.Document(ClientGame::kFlight)));
	CHECK(ui.Problems() == 0);
}
