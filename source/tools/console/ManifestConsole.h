#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/devtools/Console.h>
#include <jettison/host/Session.h>

#include <manifest/command/Request.h>
#include <manifest/content/Content.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/query/Query.h>
#include <manifest/remote/Client.h>
#include <manifest/sim/Simulation.h>

namespace Manifest::Tools
{
// Host-side development commands, registered with the same console as Jettison's shell.
// Rendering, refresh and transcripts belong to the frontend; no terminal is needed here.
// Two kinds of command, kept apart: the sandbox's (generate, tick, upgrade and the rest) play the
// local development simulation, which no player shares; the hosted ones (host, observe, provision,
// cargo, trade, cancel, travel, step) go through the hosted world's admission and queries, as a
// player's would, and book shows a venue's orders as the host sees them, a development view, where a
// player's observe shows only their own market reports (W07). There is at most one hosted world, the
// one host started, and nothing in the sandbox reaches it. Connected to a server (connect), observe and
// the players' requests go to it instead, as the token's actor.
class ManifestConsole
{
  public:
	// The content is the Manifest package, a directory or a pack, which load mounts on its own.
	explicit ManifestConsole(std::string contentSource);
	ManifestConsole(const ManifestConsole &) = delete;
	ManifestConsole &operator=(const ManifestConsole &) = delete;

	Jettison::DevTools::Console &GetConsole()
	{
		return m_console;
	}

	const Manifest::Core::Simulation &GetSimulation() const
	{
		return m_simulation;
	}

	const std::string &ContentSource() const
	{
		return m_contentSource;
	}

	bool IsConfigurationLoaded() const
	{
		return m_configLoaded;
	}

	bool IsQuitRequested() const
	{
		return m_quitRequested;
	}

	// The hosted world's completed steps; none before host.
	std::optional<std::uint64_t> HostedStep() const;

	// The server connect reached, while connected or connecting.
	const Remote::Client *Connected() const
	{
		return m_pRemote.get();
	}

	// What the server sent since the last call, as lines for the frontend to print: the welcome,
	// each request's receipt and outcome, and why the connection closed. Never blocks.
	std::vector<std::string> Poll();

  private:
	using Arguments = std::span<const std::string>;
	using Handler = std::function<std::string(Arguments)>;

	void RegisterCommands();
	void RegisterCommand(
	    std::string name, std::string help, std::size_t minimum, std::size_t maximum, Handler run);

	std::string CommandVersion();
	std::string CommandStatus();
	std::string CommandLoad(const std::string &source);
	std::string CommandGenerate(Arguments arguments);
	std::string CommandList();
	std::string CommandShow(const std::string &argument);
	std::string CommandPrices(Arguments arguments);
	std::string CommandPlan(Arguments arguments);
	std::string CommandTick(Arguments arguments);
	std::string CommandSkills();
	std::string CommandUpgrade(const std::string &argument);

	std::string CommandHost(const std::string &scenario);
	std::string CommandObserve(Arguments arguments);
	std::string CommandProvision(const std::string &principalId, const std::string &companyName);
	std::string CommandCargo(Arguments arguments);
	std::string CommandTrade(Arguments arguments);
	std::string CommandCancel(Arguments arguments);
	std::string CommandTravel(Arguments arguments);
	std::string CommandService(Arguments arguments);
	std::string CommandBook(Arguments arguments);
	std::string CommandStep(Arguments arguments);
	std::string CommandConnect(const std::string &endpoint, const std::string &token);
	std::string CommandDisconnect();
	std::string Admit(
	    const Command::Actor &actor, std::string_view command, std::uint32_t version,
	    std::span<const std::byte> payload);
	std::string Send(
	    std::string_view command, std::uint32_t version, std::span<const std::byte> payload);
	// A player's request: the operands follow the principal at arguments[first], which is not given
	// when connected, as the actor is the token's. Throws the usage for any other count.
	Arguments Operands(
	    Arguments arguments, std::size_t first, std::size_t count, std::string_view usage) const;
	std::string Queue(
	    Arguments arguments, std::size_t first, std::string_view command, std::uint32_t version,
	    std::span<const std::byte> payload);
	Jettison::Host::Session &Hosted();

	Manifest::Content::ContentSet m_content;
	Manifest::Core::Simulation m_simulation;
	Manifest::Core::ProgressionManager m_progression;

	std::string m_contentSource;
	bool m_configLoaded{false};
	bool m_quitRequested{false};

	std::unique_ptr<Jettison::Host::Session> m_pHosted;
	// Requests admitted here whose results step has not yet printed, oldest first.
	std::vector<std::pair<Command::Actor, std::uint64_t>> m_unreported;

	std::unique_ptr<Remote::Client> m_pRemote;
	std::string m_remoteEndpoint;

	Jettison::DevTools::Console m_console;
};
} // namespace Manifest::Tools
