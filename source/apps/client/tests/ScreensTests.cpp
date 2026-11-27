#include "TestHelpers.h"

#include <array>
#include <cstddef>
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

// The mount the game's content is, above the engine's assets (0): where the shell looks for the game's
// fonts, its Japanese face one.
constexpr std::size_t kFirstGameMount = 1;
constexpr std::size_t kGameFonts = 1;
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
	CHECK(ui.LoadFallbackFonts("fonts", kFirstGameMount) == kGameFonts);
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

	// Prices, numbers and the time of day in the language's form.
	CHECK(ui.ElementText(game.Document(ClientGame::kTravel), "acceleration") == "1.0");
	CHECK(ui.ElementText(game.Document(ClientGame::kMarket), "clock") == "Day 1, 12:00 AM");
	ui.SetLanguage("de");
	ui.Update(0.0);
	CHECK(ui.ElementText(game.Document(ClientGame::kTravel), "acceleration") == "1,0");
	CHECK(ui.ElementText(game.Document(ClientGame::kMarket), "clock") == "Tag 1, 00:00");
	CHECK(ui.ElementText(game.Document(ClientGame::kMarket), "ask-0").find(',') != std::string::npos);
	ui.SetLanguage("en");
	ui.Update(0.0);

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

	// Every screen again in each language the client ships, its translations all taken.
	for (const std::string_view language : {"fr", "de", "ja"})
	{
		CAPTURE(language);
		ui.SetLanguage(language);
		for (const std::string_view screen :
		     {ClientGame::kMarket, ClientGame::kTravel, ClientGame::kService, ClientGame::kCompany})
		{
			CAPTURE(screen);
			game.Show(screen);
			ui.Update(0.0);
			CHECK(ui.IsShown(game.Document(screen)));
		}
		CHECK(ui.Problems() == 0);
	}
	ui.SetLanguage("ja");
	game.Present();
	ui.Update(0.0);
	CHECK(ui.ElementText(game.Document(ClientGame::kCompany), "status") == "完了。");
}
