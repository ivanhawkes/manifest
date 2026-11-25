#pragma once

#include <filesystem>
#include <string_view>

#include <jettison/core/FileLock.h>

namespace Manifest::Host
{
// The file under a world's directory its lock is held on.
inline constexpr std::string_view kLockName = "lock";

// A world's writable directory, held by one opener until this is destroyed: the one writer of its
// snapshots, journal and catalogue (docs/persistence.md, "The world's directory and its lock"). What is
// written under it goes through Jettison::Core::WriteFileDurably, which makes any directory on the way
// durably.
struct WorldDirectory
{
	std::filesystem::path root;
	Jettison::Core::FileLock lock;
};

// Makes the directory, and those above it, so that they survive a power cut, private to its owner
// when it is made here (the XDG rule, as the engine's shell follows it), and takes its lock. Throws
// std::runtime_error naming the directory when it cannot be made, or when a server holds it already,
// this process included. For a local file system (Jettison::Core::FileLock).
WorldDirectory OpenWorldDirectory(const std::filesystem::path &root);
} // namespace Manifest::Host
