#include <manifest/host/WorldDirectory.h>

#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <jettison/core/File.h>

namespace Manifest::Host
{
namespace
{
constexpr std::string_view kLockName = "lock";
} // namespace


WorldDirectory OpenWorldDirectory(const std::filesystem::path &root)
{
	if (Jettison::Core::CreateDirectoriesDurably(root))
		std::filesystem::permissions(root, std::filesystem::perms::owner_all);
	std::optional<Jettison::Core::FileLock> lock =
	    Jettison::Core::FileLock::TryAcquire(root / kLockName);
	if (!lock)
		throw std::runtime_error{
		    std::format("the world at {} is held by a server already running", root.string())};
	return {root, std::move(*lock)};
}
} // namespace Manifest::Host
