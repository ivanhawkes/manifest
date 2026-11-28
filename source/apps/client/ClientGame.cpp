#include "ClientGame.h"

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include <jettison/app/Game.h>
#include <jettison/app/SmokeSequence.h>
#include <jettison/core/Log.h>
#include <jettison/devtools/Console.h>
#include <jettison/host/WorldContent.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/SaveGame.h>
#include <jettison/ui/DataModel.h>

#include <manifest/company/Company.h>
#include <manifest/host/Start.h>

namespace Manifest::Client
{
namespace
{
using Jettison::Ui::EventArgument;
using Arguments = std::span<const EventArgument>;

// The screens' documents, each loaded once.
constexpr std::array<std::string_view, 6> kScreens{ClientGame::kStart,   ClientGame::kCompany,
                                                   ClientGame::kMarket,  ClientGame::kTravel,
                                                   ClientGame::kService, ClientGame::kFlight};

// What a frame gives the journey's fast-forward, of the 16.7 ms a frame has at 60 Hz: the rest
// draws the frame.
constexpr std::chrono::milliseconds kFrameBudget{4};

// The start screen's fields to begin with, which the player changes.
constexpr std::string_view kDefaultName = "Ada";
constexpr std::string_view kDefaultCompany = "Ada Co";
constexpr std::string_view kDefaultShip = "Kestrel";

// The tenths of g each press of the acceleration's buttons moves it by.
constexpr std::int64_t kTenthStep = 1;

// The detail a save's list shows of a client's save: its player.
constexpr std::string_view kPlayerDetail = "manifest.player";

constexpr Jettison::Core::Category kClient{"client"};

// Why a save is refused while connected.
constexpr const char *kServersWorld =
    "the world is the server's, which saves it: nothing is saved or loaded while connected";

// The frames the render test lets pass after an answer, so the screen has drawn it.
constexpr int kSmokeSettleFrames = 2;


// The status line's words for why the connection ended.
std::string_view ClosedKey(Jettison::Net::Reason reason)
{
	using Jettison::Net::Reason;
	switch (reason)
	{
		case Reason::Closed: return "manifest.closed.closed";
		case Reason::UnknownProtocol: return "manifest.closed.unknownProtocol";
		case Reason::VersionMismatch: return "manifest.closed.versionMismatch";
		case Reason::BadCredential: return "manifest.closed.badCredential";
		case Reason::ServerFull: return "manifest.closed.serverFull";
		case Reason::HandshakeTimeout: return "manifest.closed.handshakeTimeout";
		case Reason::BadMessage: return "manifest.closed.badMessage";
		case Reason::InboundOverflow: return "manifest.closed.inboundOverflow";
		case Reason::Revoked: return "manifest.closed.revoked";
		case Reason::Shutdown: return "manifest.closed.shutdown";
		case Reason::KeyMismatch: return "manifest.closed.keyMismatch";
	}
	return "manifest.closed.closed";
}


std::string Text(Arguments arguments)
{
	return arguments.empty() ? std::string{} : Jettison::Ui::AsText(arguments.front());
}


bool Flag(Arguments arguments)
{
	const bool *flag = arguments.empty() ? nullptr : std::get_if<bool>(&arguments.front());
	return flag != nullptr && *flag;
}


// A number a form's control gives as text ("50"), or as a number; none for neither.
std::optional<std::int64_t> Number(Arguments arguments)
{
	if (arguments.empty())
		return std::nullopt;
	if (const auto *text = std::get_if<std::string>(&arguments.front()))
	{
		std::int64_t value = 0;
		const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), value);
		return error == std::errc{} ? std::optional{value} : std::nullopt;
	}
	return Jettison::Ui::AsInteger(arguments.front());
}
} // namespace


ClientGame::ClientGame(Jettison::Host::Game &world)
    : m_pWorld{&world}, m_name{kDefaultName}, m_companyName{kDefaultCompany}, m_shipName{kDefaultShip}
{
}


ClientGame::ClientGame(Remote::Client &remote, ClientModel::Dial dial)
    : m_pRemote{&remote}, m_dial{std::move(dial)}, m_name{kDefaultName}, m_companyName{kDefaultCompany},
      m_shipName{kDefaultShip}
{
}


void ClientGame::OpenUi(Jettison::Ui::UiSystem &ui, Jettison::App::Shell &shell)
{
	// Before the connection is first polled, which the model does once the UI is open.
	if (m_pRemote != nullptr)
	{
		m_content = Jettison::Host::DescribeWorldContent(shell.Assets());
		m_pRemote->Hold(*m_content);
	}
	OpenScreens(ui);
}


void ClientGame::OpenScreens(Jettison::Ui::UiSystem &ui)
{
	RegisterTypes();
	m_pUi = &ui;
	m_pData = &ui.CreateDataModel("manifest");
	m_pData->Bind("screen", &m_screen);
	BindEvents(*m_pData);
	for (const std::string_view screen : kScreens)
		m_documents.emplace(screen, ui.LoadDocument(std::format("ui/manifest/{}.rml", screen)));
	Show(kStart);
}


void ClientGame::BindEvents(Jettison::Ui::DataModel &data)
{
	// Each action is the model's, so a screen sends what the console sends.
	const auto act = [this, &data](std::string_view name, auto action)
	{
		data.BindEvent(
		    name,
		    [this, action](Arguments arguments)
		    {
			    if (m_model)
				    action(*m_model, arguments);
		    });
	};
	data.BindEvent("show", [this](Arguments arguments) { Show(Text(arguments)); });
	data.BindEvent("name", [this](Arguments arguments) { m_name = Text(arguments); });
	data.BindEvent("company", [this](Arguments arguments) { m_companyName = Text(arguments); });
	data.BindEvent("ship", [this](Arguments arguments) { m_shipName = Text(arguments); });
	act("provision",
	    [this](ClientModel &model, Arguments) { model.Provision(m_name, m_companyName, m_shipName); });
	act("buy", [](ClientModel &model, Arguments arguments) { model.Buy(Text(arguments)); });
	act("sell", [](ClientModel &model, Arguments arguments) { model.Sell(Text(arguments)); });
	act("load", [](ClientModel &model, Arguments arguments) { model.Load(Text(arguments)); });
	act("unload", [](ClientModel &model, Arguments arguments) { model.Unload(Text(arguments)); });
	act("cancel", [](ClientModel &model, Arguments arguments) { model.Cancel(Text(arguments)); });
	act("travel", [](ClientModel &model, Arguments arguments) { model.Travel(Text(arguments)); });
	act("thrust",
	    [](ClientModel &model, Arguments arguments)
	    {
		    if (const std::optional<std::int64_t> percent = Number(arguments))
			    model.SetThrust(*percent);
	    });
	act("faster", [](ClientModel &model, Arguments)
	    { model.SetAcceleration(model.View().accelerationTenths + kTenthStep); });
	act("slower", [](ClientModel &model, Arguments)
	    { model.SetAcceleration(model.View().accelerationTenths - kTenthStep); });
	act("fastforward",
	    [](ClientModel &model, Arguments) { model.SetFastForward(!model.View().fastForward); });
	act("refill", [](ClientModel &model, Arguments) { model.Refill(); });
	act("replace", [](ClientModel &model, Arguments) { model.Replace(); });
	act("refuel", [](ClientModel &model, Arguments arguments) { model.SetRefuel(Flag(arguments)); });
}


void ClientGame::Attach(Jettison::Sim::SimWorld &world)
{
	if (m_pRemote != nullptr)
		m_model.emplace(
		    *m_pRemote,
		    [this]
		    {
			    // Held before the model first polls it, as the first connection is.
			    std::unique_ptr<Remote::Client> pRemote = m_dial();
			    if (m_content)
				    pRemote->Hold(*m_content);
			    return pRemote;
		    });
	else
		m_model.emplace(world, Host::HostedServices(*m_pWorld));
	Present();
}


void ClientGame::UpdateUi(Jettison::App::Shell &shell)
{
	if (!m_model)
		Attach(shell.World());

	// The steps passed over are the world's own, which the shell's snapshots take again.
	if (m_model->FastForward(kFrameBudget) > 0)
		shell.Resync();
	m_model->Refresh();
	Present();
}


void ClientGame::Present()
{
	if (!m_model || m_pUi == nullptr)
		return;
	Screen screen = m_model->View();
	if (!screen.provisioned)
	{
		screen.principal = m_name;
		screen.companyName = m_companyName;
	}
	const Jettison::Ui::StringTable &strings = m_pUi->Strings();
	if (const std::optional<std::string> refused = m_model->Refused())
		screen.status = strings.Format("manifest.closed.content", *refused);
	else if (m_model->Reconnecting())
		screen.status = strings.Text("manifest.reconnecting");
	else if (const std::optional<Jettison::Net::Reason> closed = m_model->Closed())
		screen.status = strings.Text(ClosedKey(*closed));
	else if (m_model->Waiting())
		screen.status = strings.Text("manifest.result.waiting");
	else if (const std::optional<Command::Result> result = m_model->LastResult())
	{
		const std::string key =
		    std::format("manifest.result.{}", Jettison::Reflect::EnumName(entt::meta_any{*result}));
		// The bound a name is refused at is the rule's, not the table's.
		screen.status = *result == Command::Result::NameTooLong
		                    ? strings.Format(key, Company::kMaxNameCodePoints)
		                    : strings.Text(key);
	}

	if (screen != m_screen)
	{
		m_screen = std::move(screen);
		m_pData->Changed("screen");
	}

	// The start screen gives way to the company's once the player has a company.
	if (m_screen.provisioned && m_shown == kStart)
		Show(kCompany);
	const bool underWay = !m_screen.ships.empty() && m_screen.ships.front().underWay;
	if (underWay != m_pUi->IsShown(Document(kFlight)))
	{
		if (underWay)
			m_pUi->Show(Document(kFlight));
		else
			m_pUi->Hide(Document(kFlight));
	}
}


void ClientGame::Show(std::string_view screen)
{
	if (m_pUi == nullptr || !m_documents.contains(screen) || screen == kFlight)
		return;
	if (screen != kStart && !m_screen.provisioned)
		screen = kStart;
	for (const auto &[name, document] : m_documents)
		if (name != kFlight && name != screen)
			m_pUi->Hide(document);
	m_pUi->Show(Document(screen));
	m_shown = screen;
}


void ClientGame::AddCommands(Jettison::DevTools::Console &console, Jettison::App::Shell & /*shell*/)
{
	console.RegisterCommand(
	    "screen", "screen <start|company|market|travel|service>: show a screen of the client's",
	    [this](Jettison::DevTools::Console &, std::span<const std::string> arguments)
	    {
		    if (arguments.size() != 1 || !m_documents.contains(arguments.front()))
			    throw std::runtime_error{"usage: screen <start|company|market|travel|service>"};
		    Show(arguments.front());
	    });
	console.RegisterCommand(
	    "press", "press <element>: click an element of the screen shown or the flight panel",
	    [this](Jettison::DevTools::Console &, std::span<const std::string> arguments)
	    {
		    if (arguments.size() != 1 || !Press(arguments.front()))
			    throw std::runtime_error{"usage: press <element of the screen shown>"};
	    });
	console.RegisterCommand(
	    "read", "read <element>: log what an element of the screen shown holds",
	    [this](Jettison::DevTools::Console &, std::span<const std::string> arguments)
	    {
		    if (arguments.size() != 1 || m_pUi == nullptr)
			    throw std::runtime_error{"usage: read <element of the screen shown>"};
		    Jettison::Core::Log(
		        Jettison::Core::Severity::Info, kClient, "{} {}: {}", m_shown, arguments.front(),
		        m_pUi->ElementText(Document(m_shown), arguments.front()));
	    });
	console.RegisterCommand(
	    "found",
	    "found <company name> <ship name>: name your company and its ship and start trading, as the "
	    "start screen does",
	    [this](Jettison::DevTools::Console &, std::span<const std::string> arguments)
	    {
		    if (arguments.size() != 2)
			    throw std::runtime_error{
			        "usage: found <company name> <ship name>, each in double quotes for spaces"};
		    if (!m_model)
			    throw std::runtime_error{"found: the game has not started yet"};
		    m_companyName = arguments[0];
		    m_shipName = arguments[1];
		    const Command::Receipt receipt = m_model->Provision(m_name, m_companyName, m_shipName);
		    if (receipt.status == Command::Receipt::Status::Refused)
			    throw std::runtime_error{std::format(
			        "found: refused, {}", Jettison::Reflect::EnumName(entt::meta_any{receipt.result}))};
		    Present();
	    });
}


bool ClientGame::Press(std::string_view element)
{
	if (m_pUi == nullptr)
		return false;
	return m_pUi->Click(Document(m_shown), element) ||
	       (m_pUi->IsShown(Document(kFlight)) && m_pUi->Click(Document(kFlight), element));
}


void ClientGame::WriteSave(Jettison::Sim::SaveGame &save, Jettison::App::Shell &shell)
{
	if (m_pRemote != nullptr)
		throw std::runtime_error{kServersWorld};
	if (!m_model)
		Attach(shell.World());
	save.AddSection(std::string{kSaveSection}, WriteState(m_model->State()));
	save.info.details.push_back({std::string{kPlayerDetail}, m_model->Principal()});
}


void ClientGame::ReadSave(const Jettison::Sim::SaveGame &save, Jettison::App::Shell & /*shell*/)
{
	if (m_pRemote != nullptr)
		throw std::runtime_error{kServersWorld};
	const Jettison::Sim::SaveSection *section = save.FindSection(kSaveSection);
	if (section == nullptr)
		throw std::runtime_error{"the save is not the Manifest client's: it has no Manifest.Client"};
	m_loading = ReadState(section->data);
}


void ClientGame::Loaded(Jettison::App::Shell &shell)
{
	if (!m_model)
		Attach(shell.World());
	if (m_loading)
		m_model->Restore(*std::exchange(m_loading, std::nullopt));
	Present();
	Show(m_screen.provisioned ? kCompany : kStart);
}


void ClientGame::DriveSmoke(Jettison::App::SmokeActions &actions, Jettison::App::Shell &shell)
{
	// Provisioning, then a tote bought at the market's first good's ask: each pressed as the player
	// would, and waited for until its step has answered and a frame has shown it.
	actions.gameUi = [this, &shell]() -> std::optional<bool>
	{
		if (m_pUi == nullptr)
			return std::nullopt;
		if (!m_model)
			Attach(shell.World());
		Show(kStart);
		if (m_screen.provisioned || m_pUi->ElementText(Document(kStart), "provision").empty())
			return false;
		m_smokePresses = 2;
		m_smokeFrames = 0;
		return Press("provision");
	};
	actions.gameUiWaiting = [this]
	{
		if (!m_model || m_model->Waiting() || ++m_smokeFrames < kSmokeSettleFrames)
			return true;
		if (--m_smokePresses == 0)
			return false;
		Show(kMarket);
		m_smokeFrames = 0;
		return Press("buy-0");
	};
	actions.gameUiAnswered = [this]
	{
		const std::string held = m_pUi->ElementText(Document(kMarket), "held-0");
		Jettison::Core::Log(
		    Jettison::Core::Severity::Info, kClient, "The render test's purchase: {}, {} held",
		    m_screen.status, held);
		return m_model && m_model->LastResult() == Command::Result::Succeeded && held == "1";
	};
}


bool ClientGame::Back(Jettison::App::Shell & /*shell*/)
{
	if (m_shown == kStart || m_shown == kCompany)
		return false;
	Show(kCompany);
	return true;
}


Jettison::Ui::DocumentHandle ClientGame::Document(std::string_view screen) const
{
	const auto found = m_documents.find(screen);
	return found != m_documents.end() ? found->second : Jettison::Ui::DocumentHandle{};
}
} // namespace Manifest::Client
