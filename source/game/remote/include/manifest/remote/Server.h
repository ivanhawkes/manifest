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
// the token file, as the actor it names, never an NPC's principal (D-097), its requests admitted
// through the world's one admission path and its snapshot filtered to what that actor may see.
// Sealed to keys made with the server's secret key, which each client pins the public half of (D-081),
// so it may be hosted on an open network. Single-threaded, as Jettison::Net is: the caller polls
// between steps and steps the world itself, which steps the same with no session. An actor may hold
// three sessions at once, each sent its own replies and the actor's snapshot; a fourth is closed as
// ServerFull. Each welcome carries the session's world content (Jettison::Host::DescribeWorldContent),
// read once as the server is made, for the client to check against its own. Destroying it tells every
// client Shutdown.
class Server
{
  public:
	// The address is dotted IPv4 or empty for every interface, the port 0 for an ephemeral one.
	// Throws std::runtime_error when the token file or the key file (ReadServerKey) cannot be read, or
	// the address bound.
	Server(
	    Jettison::Host::Session &session, std::filesystem::path tokens,
	    const std::filesystem::path &key, std::string address, std::uint16_t port);

	std::uint16_t Port() const;

	// The public half of the server's key, for its operator to give the clients to pin.
	const std::string &PublicKey() const
	{
		return m_publicKey;
	}

	// Admits connections and requests, waiting up to wait for the first: every request gets its
	// receipt. Reads the token file again when it has changed and closes, as Revoked, every session
	// whose token is gone from it.
	void Poll(std::chrono::milliseconds wait);

	// After a step: the completed requests' replies and each session's snapshot.
	void Publish();

	// For a clean stop, after the last Publish: closes every session as Shutdown once what it was sent
	// has arrived, and any admitted after it at once. Poll on while Stopping holds, to a deadline, for
	// the clients to have their last replies: destroying the server closes at once, dropping them.
	void Stop();

	bool Stopping() const;

  private:
	struct Session
	{
		Credential credential;

		// Queued here and not yet reported completed, oldest first.
		std::vector<std::uint64_t> pending;

		// Its actor held every session it may when it was verified: closed as ServerFull on admission.
		bool overCap{false};
	};

	bool Verify(Jettison::Net::SessionId id, std::span<const std::byte> credential);

	void Admit(Jettison::Net::SessionId id, Session &session);

	void ReloadTokens();

	Jettison::Host::Session &m_hosted;

	// Its world content's bytes, as each welcome sends them.
	std::vector<std::uint8_t> m_content;
	std::filesystem::path m_tokensPath;
	std::filesystem::file_time_type m_tokensWritten;
	TokenFile m_tokens;
	std::map<Jettison::Net::SessionId, Session> m_sessions;
	bool m_stopped{false};

	// Before m_net, whose configuration sets it from the secret.
	std::string m_publicKey;
	Jettison::Net::Server m_net;
};
} // namespace Manifest::Remote
