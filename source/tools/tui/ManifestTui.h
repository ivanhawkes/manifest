#pragma once

#include "ManifestConsole.h"

#include <fstream>
#include <string>

namespace Manifest::Tui
{
// FTXUI presentation of the shared console, its sandbox and the hosted world it drives.
class ManifestTui
{
  public:
	ManifestTui();
	ManifestTui(const ManifestTui &) = delete;
	ManifestTui &operator=(const ManifestTui &) = delete;

	void Run();

  private:
	static std::ofstream OpenSessionLog();
	void AppendSessionLog(const std::string &text);

	Tools::ManifestConsole m_session;
	std::ofstream m_logFile;
};
} // namespace Manifest::Tui
