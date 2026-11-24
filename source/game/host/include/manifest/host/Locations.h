#pragma once

#include <filesystem>

namespace Manifest::Host
{
// The content a build carries beside its executables, <bin>/../share/manifest/manifest.pack, from
// the running executable (Jettison::Platform::ExecutablePath): a distribution's, and a build tree's,
// which is laid out the same way. Throws std::filesystem::filesystem_error when /proc is not mounted.
std::filesystem::path DefaultContent();

// Where Manifest writes when no option names a place (docs/stage-5-recovery-plan.md, "The runtime
// distribution contract"): under the XDG base directories, Jettison::Platform::ResolvePaths's for
// "manifest". Resolving touches nothing on disk. Throws std::runtime_error naming the variable when
// HOME is needed and is not an absolute path.
struct Locations
{
	std::filesystem::path world;
	std::filesystem::path backups;
	std::filesystem::path tokens;
	std::filesystem::path transcript;
};
Locations DefaultLocations();

// Makes the directory and each one above it that is missing, durably
// (Jettison::Core::CreateDirectoriesDurably), each it makes private to its owner (0700), as the XDG
// specification has a writer make a base directory. One that was already there is left as it is.
void MakePrivateDirectories(const std::filesystem::path &directory);
} // namespace Manifest::Host
