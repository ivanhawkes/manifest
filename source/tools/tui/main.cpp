// manifest_tui [--content <directory-or-pack>]: the development console in the terminal, on the
// package beside the executable (Manifest::Host::DefaultContent) unless one is given, keeping its
// transcript where Manifest::Host::DefaultTranscript puts it. A content or transcript that cannot be
// found exits 1, a bad command line 2.
#include "ManifestTui.h"

#include <cstdio>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <manifest/host/Locations.h>

int main(int argc, char **argv)
{
	const std::vector<std::string_view> arguments(argv + 1, argv + argc);
	if (!arguments.empty() && (arguments.size() != 2 || arguments[0] != "--content"))
	{
		std::fputs("usage: manifest_tui [--content <directory-or-pack>]\n", stderr);
		return 2;
	}
	try
	{
		const std::filesystem::path content =
		    arguments.empty() ? Manifest::Host::DefaultContent() : std::filesystem::path{arguments[1]};
		const std::filesystem::path transcript = Manifest::Host::DefaultTranscript();
		Manifest::Host::MakePrivateDirectories(transcript.parent_path());
		Manifest::Tui::ManifestTui repl{content, transcript};
		repl.Run();
	}
	catch (const std::runtime_error &error)
	{
		std::fputs(std::format("{}\n", error.what()).c_str(), stderr);
		return 1;
	}
	return 0;
}
