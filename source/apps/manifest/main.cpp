// manifest <options> --check-startup (Manifest::Host::kOptionsUsage): starts Manifest on the content,
// reports it ready, shuts down and exits 0. A failed start-up exits 1, a bad command line 2.
#include <manifest/host/Start.h>

#include <cstdio>
#include <format>
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
	std::fputs(
	    std::format("usage: manifest {} --check-startup\n", Manifest::Host::kOptionsUsage).c_str(),
	    stderr);
	return 2;
}
} // namespace


int main(int argc, char **argv)
{
	const std::vector<std::string_view> arguments(argv + 1, argv + argc);
	std::vector<std::string_view> rest;
	const std::optional<Manifest::Host::Options> options =
	    Manifest::Host::ParseOptions(arguments, rest);
	// Running the world unattended comes with hosted work to advance; until then only the check.
	if (!options || rest != std::vector<std::string_view>{"--check-startup"})
		return Usage();

	using Jettison::Core::Severity;
	try
	{
		const auto session = Manifest::Host::Start(*options);
		Jettison::Core::Log(
		    Severity::Info, kManifest, "ready, from {}, scenario {}", options->content.string(),
		    options->scenario);
		session->Shutdown();
	}
	catch (const Jettison::Host::StartupError &error)
	{
		Jettison::Core::Log(Severity::Error, kManifest, "{}", error.what());
		return 1;
	}
	return 0;
}
