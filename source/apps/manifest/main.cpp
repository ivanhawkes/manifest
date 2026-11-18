// manifest --content <directory-or-pack> --check-startup: starts Manifest on the content, reports it
// ready, shuts down and exits 0. A failed start-up exits 1, a bad command line 2.
#include <manifest/host/Start.h>

#include <cstdio>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

#include <jettison/core/Log.h>

namespace
{
constexpr Jettison::Core::Category kManifest{"manifest"};


int Usage()
{
	// The usage line goes to stderr directly, as the engine shell's does; the exit status pairs it.
	std::fputs("usage: manifest --content <directory-or-pack> --check-startup\n", stderr);
	return 2;
}
} // namespace


int main(int argc, char **argv)
{
	const std::vector<std::string_view> arguments(argv + 1, argv + argc);
	std::optional<std::filesystem::path> content;
	bool checkStartup = false;
	for (auto at = arguments.begin(); at != arguments.end(); ++at)
		if (*at == "--content" && std::next(at) != arguments.end() && !content)
			content = *++at;
		else if (*at == "--check-startup" && !checkStartup)
			checkStartup = true;
		else
			return Usage();
	// Running the world unattended comes with hosted work to advance; until then only the check.
	if (!content || !checkStartup)
		return Usage();

	using Jettison::Core::Severity;
	try
	{
		const auto session = Manifest::Host::Start(*content);
		Jettison::Core::Log(Severity::Info, kManifest, "ready, from {}", content->string());
		session->Shutdown();
	}
	catch (const Jettison::Host::StartupError &error)
	{
		Jettison::Core::Log(Severity::Error, kManifest, "{}", error.what());
		return 1;
	}
	return 0;
}
