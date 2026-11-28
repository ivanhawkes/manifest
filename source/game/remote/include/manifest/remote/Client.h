#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/host/WorldContent.h>
#include <jettison/net/Client.h>

#include <manifest/query/Query.h>
#include <manifest/remote/Protocol.h>

namespace Manifest::Remote
{
// The client end of Server, for a frontend: connects with a token as it is made, to the server holding
// the key it pins (PinnedKey), and is welcomed as the token's actor, or closed with the reason:
// KeyMismatch for a server that does not hold it. A welcome whose world content differs from the
// content the client holds (Hold) is not taken: the client disconnects, refused with the difference
// (Jettison::Host::ContentDifference). It submits intentions and keeps the newest snapshot; it never
// steps or holds a world. To reconnect, make another.
class Client
{
  public:
	// Throws std::runtime_error when the address does not resolve, and std::invalid_argument for a
	// server key that is not one (Jettison::Net::ClientConfig::serverKey).
	Client(std::string address, std::uint16_t port, std::string serverKey, std::string_view token);

	// What arrived, waiting up to wait for the first of it: the replies in order. A snapshot only
	// replaces Latest, and one older than it is dropped by the transport.
	std::vector<Reply> Poll(std::chrono::milliseconds wait = std::chrono::milliseconds{0});

	// The world content this client runs on, given before the welcome arrives: a welcome before it is
	// refused, as one of content the client cannot check.
	void Hold(Jettison::Host::WorldContent content);

	// Set once the server's welcome arrives and its content is the client's: who this is and what it
	// sends next.
	const std::optional<Welcome> &Welcomed() const
	{
		return m_welcome;
	}

	// Why the connection ended, once it has: a refusal at the handshake or a closing after it.
	const std::optional<Jettison::Net::Reason> &Closed() const
	{
		return m_closed;
	}

	// Why the welcome was not taken, its world content not the client's: the connection is then
	// closing, and Closed follows.
	const std::optional<std::string> &Refused() const
	{
		return m_refused;
	}

	const std::optional<Query::Snapshot> &Latest() const
	{
		return m_latest;
	}

	// Sends a request under the next sequence and returns it; none before the welcome or after
	// closing. Its receipt comes back as a Reply; after a refusal the next sequence is the server's.
	std::optional<std::uint64_t> Submit(
	    std::string_view command, std::uint32_t version, std::span<const std::byte> payload);

	// Sends a request again under the sequence it was first sent with, as after a reconnect when its
	// completion never arrived: the ledger answers it as the request it named, never running it twice.
	// False before the welcome, after closing, or when it cannot be sent.
	bool Retry(
	    std::uint64_t sequence, std::string_view command, std::uint32_t version,
	    std::span<const std::byte> payload);

	// Closes the connection once what is queued is sent, at the next Poll: poll after it, or the
	// server hears the client go only when it times out.
	void Disconnect();

  private:
	Jettison::Net::Client m_net;
	std::optional<Jettison::Host::WorldContent> m_held;
	std::optional<Welcome> m_welcome;
	std::optional<Jettison::Net::Reason> m_closed;
	std::optional<std::string> m_refused;
	std::optional<Query::Snapshot> m_latest;
};

// Whether a connection that closed so is dialled again with the same token, after a welcome: the
// server gone quiet or restarting. A credential, key, protocol, content or message it refused stays
// refused, and a full server is left for the player to try again, so no refusal is dialled in a loop
// (D-125).
bool Redials(Jettison::Net::Reason reason);
} // namespace Manifest::Remote
