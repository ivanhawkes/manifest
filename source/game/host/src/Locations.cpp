#include <manifest/host/Locations.h>

#include <vector>

#include <jettison/core/File.h>
#include <jettison/platform/Paths.h>

namespace Manifest::Host
{
std::filesystem::path DefaultContent()
{
	return Jettison::Platform::ExecutablePath().parent_path().parent_path() / "share" / "manifest" /
	       "manifest.pack";
}


std::filesystem::path DefaultWorld()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::UserData, "manifest") /
	       "world";
}


std::filesystem::path DefaultBackups()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::UserData, "manifest") /
	       "backups";
}


std::filesystem::path DefaultTokens()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::Config, "manifest") /
	       "tokens";
}


std::filesystem::path DefaultTranscript()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::State, "manifest") /
	       "tui-session.log";
}


void MakePrivateDirectories(const std::filesystem::path &directory)
{
	std::vector<std::filesystem::path> missing;
	for (std::filesystem::path at = directory; !at.empty() && !std::filesystem::exists(at);
	     at = at.parent_path())
		missing.push_back(at);
	Jettison::Core::CreateDirectoriesDurably(directory);
	for (const std::filesystem::path &made : missing)
		std::filesystem::permissions(made, std::filesystem::perms::owner_all);
}
} // namespace Manifest::Host
