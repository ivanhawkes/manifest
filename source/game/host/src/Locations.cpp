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


std::filesystem::path DefaultTranscript()
{
	return Jettison::Platform::ResolvePath(Jettison::Platform::Directory::State, "manifest") /
	       "tui-session.log";
}


void MakePrivateDirectories(const std::filesystem::path &directory)
{
	// A component at a time, each made private through the path it was made by the moment it is
	// made: a path listed before would name, through ".." or a symlink, one that was already there.
	std::filesystem::path at;
	for (const std::filesystem::path &component : directory)
	{
		at /= component;
		if (Jettison::Core::CreateDirectoriesDurably(at))
			std::filesystem::permissions(at, std::filesystem::perms::owner_all);
	}
}
} // namespace Manifest::Host
