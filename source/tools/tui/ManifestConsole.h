#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>

#include <jettison/devtools/Console.h>

#include <manifest/content/Content.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/sim/Simulation.h>

namespace Manifest::Tui
{
// Host-side development commands, registered with the same console as Jettison's shell.
// Rendering, refresh and transcripts belong to the frontend; no terminal is needed here.
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

	Manifest::Content::ContentSet m_content;
	Manifest::Core::Simulation m_simulation;
	Manifest::Core::ProgressionManager m_progression;

	std::string m_contentSource;
	bool m_configLoaded{false};
	bool m_quitRequested{false};

	Jettison::DevTools::Console m_console;
};
} // namespace Manifest::Tui
