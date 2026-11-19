#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/net/Server.h>

#include <manifest/remote/Tokens.h>

namespace Manifest::Remote
{
// Serves a started session's world (Host::Start) to remote clients: each admitted by a token from
// the token file, as the actor it names, its requests admitted through the world's one admission
// path and its snapshot filtered to what that actor may see. Plaintext, for loopback or a LAN
// (D-081). Single-threaded, as Jettison::Net is: the caller polls between steps and steps the
// world itself, which steps the same with no session. Destroying it tells every client Shutdown.
class Server
{
  public:
	// The address is dotted IPv4 or empty for every interface, the port 0 for an ephemeral one.
	// Throws std::runtime_error when the token file cannot be read or the address bound.
	Server(
	    Jettison::Host::Session &session, std::filesystem::path tokens, std::string address,
	    std::uint16_t port);

	std::uint16_t Port() const;

	// Admits connections and requests, waiting up to wait for the first: every request gets its
	// receipt. Reads the token file again when it has changed and closes, as Revoked, every session
	// whose token is gone from it.
	void Poll(std::chrono::milliseconds wait);

	// After a step: the completed requests' replies and each session's snapshot.
	void Publish();

  private:
	struct Session
	{
		Credential credential;
		// Queued here and not yet reported completed, oldest first.
		std::vector<std::uint64_t> pending;
	};

	bool Verify(Jettison::Net::SessionId id, std::span<const std::byte> credential);
	void Admit(Jettison::Net::SessionId id, Session &session);
	void ReloadTokens();

	Jettison::Host::Session &m_hosted;
	std::filesystem::path m_tokensPath;
	std::filesystem::file_time_type m_tokensWritten;
	TokenFile m_tokens;
	std::map<Jettison::Net::SessionId, Session> m_sessions;
	Jettison::Net::Server m_net;
};
} // namespace Manifest::Remote
