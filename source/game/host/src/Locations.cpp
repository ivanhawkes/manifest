#include <manifest/host/Locations.h>

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


std::filesystem::path DefaultServerKey()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::Config, "manifest") /
	       "server-key";
}


std::filesystem::path DefaultServers()
{
	std::filesystem::path own = PlayerServers();
	if (std::filesystem::exists(own))
		return own;
	return ShippedServers();
}


std::filesystem::path PlayerServers()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::Config, "manifest") /
	       "servers.yaml";
}


std::filesystem::path ShippedServers()
{
	return DefaultContent().parent_path() / "servers.yaml";
}


std::filesystem::path DefaultTranscript()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::State, "manifest") /
	       "tui-session.log";
}


void MakePrivateDirectories(const std::filesystem::path &directory)
{
	// Made so, not changed after: a change cut off would leave one open that a later run keeps.
	Jettison::Core::CreateDirectoriesDurably(directory, std::filesystem::perms::owner_all);
}
} // namespace Manifest::Host
