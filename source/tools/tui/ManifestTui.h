#pragma once

#include "ManifestConsole.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace Manifest::Tui
{
// FTXUI presentation of the shared console, its sandbox and the hosted world it drives.
class ManifestTui
{
  public:
	// The console on the content, its transcript appended to the file.
	ManifestTui(const std::filesystem::path &content, const std::filesystem::path &transcript);

	ManifestTui(const ManifestTui &) = delete;

	ManifestTui &operator=(const ManifestTui &) = delete;

	void Run();

  private:
	void AppendSessionLog(const std::string &text);

	Tools::ManifestConsole m_session;
	std::ofstream m_logFile;
};
} // namespace Manifest::Tui
