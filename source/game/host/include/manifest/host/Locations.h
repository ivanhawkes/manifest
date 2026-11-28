#pragma once

#include <filesystem>

namespace Manifest::Host
{
// The content a build carries beside its executables, <bin>/../share/manifest/manifest.pack, from
// the running executable (Jettison::Platform::ExecutablePath): a distribution's, and a build tree's,
// which is laid out the same way. Throws std::filesystem::filesystem_error when /proc is not mounted.
std::filesystem::path DefaultContent();

// Where Manifest writes when no option names a place (docs/host.md, "The default places"): under the
// XDG base directories, Jettison::Platform::ResolvePath's for "manifest", each resolved alone, so a
// program needs only the variables of the places it writes. Resolving touches nothing on disk. Throws
// std::runtime_error naming the variable when HOME is needed and is not an absolute path.
std::filesystem::path DefaultWorld();

std::filesystem::path DefaultBackups();

std::filesystem::path DefaultTokens();

std::filesystem::path DefaultServerKey();

std::filesystem::path DefaultTranscript();

// The servers a client pins the keys of (Remote::ServerList): the player's,
// $XDG_CONFIG_HOME/manifest/servers.yaml (PlayerServers), when it is there, else the one the build
// ships beside its content (ShippedServers). Throws as the others do.
std::filesystem::path DefaultServers();

std::filesystem::path PlayerServers();

std::filesystem::path ShippedServers();

// Makes the directory and each one above it that is missing, durably
// (Jettison::Core::CreateDirectoriesDurably), each it makes private to its owner (0700) from the moment
// it is made, as the XDG specification has a writer make a base directory. One that was already there
// is left as it is.
void MakePrivateDirectories(const std::filesystem::path &directory);
} // namespace Manifest::Host
