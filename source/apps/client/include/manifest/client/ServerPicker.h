#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <manifest/remote/Client.h>
#include <manifest/remote/Keys.h>

namespace Manifest::Client
{
// Why the server picker would not save a server or connect to one, before anything was sent.
enum class PickRefusal : std::uint8_t
{
	NoName,
	BadEndpoint,
	NoToken,
	Unpinned,
	BadKey,
	UnknownHost,
	Unreadable,
	Unwritable
};

// A refusal of the picker's, its message in the log's words, which never hold a token.
class Refused : public std::runtime_error
{
  public:
	Refused(PickRefusal reason, const std::string &message)
	    : std::runtime_error{message}, m_reason{reason}
	{
	}

	PickRefusal Reason() const
	{
		return m_reason;
	}

  private:
	PickRefusal m_reason;
};

// A server the player saved, as the picker lists it: the name they gave it and its host:port, never
// their token for it.
struct SavedServer
{
	std::string name;
	std::string endpoint;

	bool operator==(const SavedServer &) const = default;
};

// What the server picker shows: the saved servers, and its words for what it last did or refused,
// which the client fills in.
struct ServerScreen
{
	std::vector<SavedServer> servers;
	std::string status;

	bool operator==(const ServerScreen &) const = default;
};

// The server at host:port, its key pinned from the server list in the file (Remote::PinnedKey) as the
// console pins it, connecting with the token. Throws Refused for an endpoint that is not host:port
// (BadEndpoint), a list that cannot be read (Unreadable), one that pins no key for it (Unpinned) or one
// that is not a key (BadKey), and a host that does not resolve (UnknownHost).
std::unique_ptr<Remote::Client> Dial(
    const std::filesystem::path &servers, const std::string &endpoint, std::string_view token);

// The servers the player has saved (docs/client.md, "Choosing a server"): those of their own server
// list that hold a name and a token beside their pinned key (D-127), and a connection to one. The list
// is written whole each time, its owner's alone (Remote::WriteServers), in a directory made private if
// it makes one.
class ServerPicker
{
  public:
	// The player's list (Host::PlayerServers) and the build's (Host::ShippedServers), which is read
	// while the player has none and copied into theirs as they first save a server, so its pins stay.
	// A list that cannot be read is logged and lists nothing.
	ServerPicker(std::filesystem::path own, std::filesystem::path shipped);

	// In the list's order.
	const std::vector<SavedServer> &Servers() const
	{
		return m_servers;
	}

	// The server saved under the name with the player's token for it, replacing what the list held of
	// the endpoint: its key pinned when one is given, else the list must pin one already. White space
	// around each is taken off. Throws Refused: NoName, BadEndpoint, NoToken, Unpinned, Unreadable or
	// Unwritable, and nothing is written.
	void Add(std::string name, std::string endpoint, std::string key, std::string token);

	// The server's name and token taken out of the list, its pinned key kept, as a build's list would
	// have it. Throws Refused as Add does when the list cannot be read or written.
	void Remove(std::string_view endpoint);

	// A connection to a saved server with the player's token for it (Dial). Throws Refused as Dial
	// does, and NoToken for an endpoint not saved.
	std::unique_ptr<Remote::Client> Connect(const std::string &endpoint) const;

  private:
	// The player's list when they have one, else the build's.
	const std::filesystem::path &Listed() const;

	Remote::ServerList Read() const;

	void Write(const Remote::ServerList &list);

	void List(const Remote::ServerList &list);

	std::filesystem::path m_own;
	std::filesystem::path m_shipped;
	std::vector<SavedServer> m_servers;
};
} // namespace Manifest::Client
