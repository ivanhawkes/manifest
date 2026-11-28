#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <jettison/app/Game.h>
#include <jettison/host/Session.h>
#include <jettison/ui/UiSystem.h>

#include <manifest/client/ClientModel.h>

namespace Manifest::Client
{
// The graphical client's presentation composed into Jettison's shell (docs/client.md): its screens,
// RmlUi documents under ui/manifest/ over the model's Screen, the data model "manifest", and the
// world fast-forwarded a budget a frame while a journey is under way; or, connected, a server's world
// shown and played, the shell holding no world of the game's.
class ClientGame final : public Jettison::App::Game
{
  public:
	// The screens, by the document each is in: ui/manifest/<name>.rml. The flight panel is shown
	// beside them while a journey is under way.
	static constexpr std::string_view kStart = "start";
	static constexpr std::string_view kCompany = "company";
	static constexpr std::string_view kMarket = "market";
	static constexpr std::string_view kTravel = "travel";
	static constexpr std::string_view kService = "service";
	static constexpr std::string_view kFlight = "flight";

	// The world is Manifest's (Host::MakeGame), which the shell's session holds and starts.
	explicit ClientGame(Jettison::Host::Game &world);

	// Over a server's connection, which outlives the game.
	explicit ClientGame(Remote::Client &remote);

	void OpenUi(Jettison::Ui::UiSystem &ui, Jettison::App::Shell &shell) override;

	// The model made once the world has started, the journey fast-forwarded, and the screens made
	// again from the snapshot.
	void UpdateUi(Jettison::App::Shell &shell) override;

	// The client's section in the shell's saves (kSaveSection) and its player as a detail; a save with
	// no section, not the client's, is refused, and what a load read is applied once it has loaded.
	// Connected, both are refused: the world is the server's, which saves it.
	void WriteSave(Jettison::Sim::SaveGame &save, Jettison::App::Shell &shell) override;

	void ReadSave(const Jettison::Sim::SaveGame &save, Jettison::App::Shell &shell) override;

	void Loaded(Jettison::App::Shell &shell) override;

	// The render test's game UI stage: the start screen provisions the player, then the market
	// screen buys a tote at the ask, and the screen shows it held.
	void DriveSmoke(Jettison::App::SmokeActions &actions, Jettison::App::Shell &shell) override;

	// Escape: a screen other than the company's goes back to it.
	bool Back(Jettison::App::Shell &shell) override;

	// screen <name> shows a screen and press <element> clicks one of the screen shown or the flight
	// panel, as the mouse would, and read <element> logs what one holds: for --run lines and captures,
	// which have no mouse. found <company name> names the player's company and starts them, as the
	// start screen does.
	void AddCommands(Jettison::DevTools::Console &console, Jettison::App::Shell &shell) override;

	// A click on the element of the screen shown, or of the flight panel; false for neither.
	bool Press(std::string_view element);

	// The data model and the documents loaded into the UI, as OpenUi does: the screens check's way
	// in, with no shell.
	void OpenScreens(Jettison::Ui::UiSystem &ui);

	// The model, over the world once a session has started it, or over the connection; and the
	// screens made from it.
	void Attach(Jettison::Sim::SimWorld &world);

	void Present();

	// The screen shown, the start screen until the player is provisioned.
	void Show(std::string_view screen);

	const std::string &Shown() const
	{
		return m_shown;
	}

	ClientModel *Model()
	{
		return m_model ? &*m_model : nullptr;
	}

	Jettison::Ui::DocumentHandle Document(std::string_view screen) const;

  private:
	void BindEvents(Jettison::Ui::DataModel &data);

	// One of them.
	Jettison::Host::Game *m_pWorld{nullptr};
	Remote::Client *m_pRemote{nullptr};
	std::optional<ClientModel> m_model;
	Jettison::Ui::UiSystem *m_pUi{nullptr};
	Jettison::Ui::DataModel *m_pData{nullptr};
	std::map<std::string, Jettison::Ui::DocumentHandle, std::less<>> m_documents;
	std::string m_shown;

	// What the screens read, the model's with the client's words for the last answer; and what the
	// start screen's fields hold until the player is provisioned.
	Screen m_screen;
	std::string m_name;
	std::string m_companyName;

	// What a save being loaded holds of the client, until the world has loaded.
	std::optional<ClientState> m_loading;

	// The render test's presses still to be answered, and the frames since the last was.
	int m_smokePresses{0};
	int m_smokeFrames{0};
};
} // namespace Manifest::Client
