// manifest-validate <options> [--manifest <file>] (Manifest::Host::kOptionsUsage): loads the content
// as the host would, with no world, and reports the mounts in order, the scenario and its game mode,
// and the content's fingerprint; --manifest writes the content manifest's bytes to the file, for the
// build to compare a pack's with its directory's. Content that would not start exits 1, as the host's
// start-up fails, as does a manifest that cannot be written; a bad command line exits 2.
#include <manifest/host/Start.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <jettison/core/Log.h>
#include <jettison/vfs/Mount.h>

namespace
{
constexpr Jettison::Core::Category kValidate{"validate"};


int Usage()
{
	// The usage line goes to stderr directly, as the host's does; the exit status pairs it.
	std::fputs(
	    std::format("usage: manifest-validate {} [--manifest <file>]\n", Manifest::Host::kOptionsUsage)
	        .c_str(),
	    stderr);
	return 2;
}
} // namespace


int main(int argc, char **argv)
{
	using Jettison::Core::Log;
	using Jettison::Core::Severity;
	try
	{
		const std::vector<std::string_view> arguments(argv + 1, argv + argc);
		std::vector<std::string_view> rest;
		const std::optional<Manifest::Host::Options> options =
		    Manifest::Host::ParseOptions(arguments, rest);
		const bool writeManifest = rest.size() == 2 && rest[0] == "--manifest";
		if (!options || (!rest.empty() && !writeManifest))
			return Usage();

		const Manifest::Content::ContentSet content = Manifest::Host::Validate(*options);
		const std::vector<Jettison::Vfs::Mount> mounts = Manifest::Host::Mounts(*options);
		for (std::size_t index = 0; index < mounts.size(); ++index)
			Log(Severity::Info, kValidate, "mount {}: {}, from {}", index + 1, mounts[index].name,
			    mounts[index].source.string());
		Log(Severity::Info, kValidate, "valid: scenario {}, game mode {}, content {:016x}",
		    content.scenario, content.scenarios.at(content.scenario).gameMode,
		    content.identity.fingerprint);
		if (writeManifest)
		{
			const std::filesystem::path path{rest[1]};
			std::ofstream out{path, std::ios::binary | std::ios::trunc};
			out.write(
			    reinterpret_cast<const char *>(content.identity.manifest.data()),
			    static_cast<std::streamsize>(content.identity.manifest.size()));
			if (!out.flush())
			{
				Log(Severity::Error, kValidate, "the manifest could not be written to {}",
				    path.string());
				return 1;
			}
		}
	}

	// Content that would not start (Jettison::Host::StartupError), or the default that cannot be found.
	catch (const std::runtime_error &error)
	{
		Log(Severity::Error, kValidate, "{}", error.what());
		return 1;
	}
	return 0;
}
