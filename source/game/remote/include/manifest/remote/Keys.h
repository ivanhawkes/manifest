#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Manifest::Remote
{
// The server's secret key (Jettison::Net::ServerKey), as manifest-token key writes it: the key's hex on
// one line. Throws std::runtime_error naming the file when it cannot be read or holds no secret key.
std::string ReadServerKey(const std::filesystem::path &path);

// Writes the secret key durably, the file its owner's alone (0600) from the moment it is made
// (Jettison::Core::WriteFileDurably). Throws std::runtime_error naming the file when it cannot.
void WriteServerKey(const std::filesystem::path &path, std::string_view secretKey);

// A server a client connects to, and the public half of its key, which the client pins.
struct PinnedServer
{
	// host:port, as the console's connect takes it.
	std::string endpoint;
	std::string key;
};

// The servers a client knows (Manifest::Host::DefaultServers): a YAML file a build ships, or its
// player keeps.
struct ServerList
{
	std::vector<PinnedServer> servers;
};

// Throws std::runtime_error naming the file when it cannot be read or is not a server list.
ServerList ReadServers(const std::filesystem::path &path);

// The key pinned for the endpoint, matched as written; none for one the list does not name.
std::optional<std::string> PinnedKey(const ServerList &list, std::string_view endpoint);
} // namespace Manifest::Remote
