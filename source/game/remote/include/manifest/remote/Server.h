#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/net/Server.h>

#include <manifest/remote/Protocol.h>
#include <manifest/remote/Tokens.h>

namespace Manifest::Remote
{
// Serves a started session's world (Host::Start) to remote clients: each admitted by a token from
// the token file, as the actor it names, never an NPC's principal (D-097), its requests admitted
// through the world's one admission path and its snapshot filtered to what that actor may see.
// Plaintext, for loopback or a LAN (D-081). Single-threaded, as Jettison::Net is: the caller polls
// between steps and steps the world itself, which steps the same with no session. An actor may hold
// three sessions at once, each sent its own replies and the actor's snapshot; a fourth is closed as
// ServerFull. Destroying it tells every client Shutdown.
class Server
{
  public:
	// The address is dotted IPv4 or empty for every interface, the port 0 for an ephemeral one.
	// Throws std::runtime_error when the token file cannot be read or the address bound.
	Server(
	    Jettison::Host::Session &session, std::filesystem::path tokens, std::string address,
	    std::uint16_t port);

	std::uint16_t Port() const;

	// Admits connections and requests, waiting up to wait for the first. A refusal, or a retry of a
	// request that has run, is answered at once; a queued request's receipt is held for Acknowledge.
	// Reads the token file again when it has changed and closes, as Revoked, every session whose
	// token is gone from it.
	void Poll(std::chrono::milliseconds wait);

	// Sends the receipts of the requests queued since the last: the host's word that their order
	// will survive it, so called once the batch that holds them is committed (Host::Journal::Commit)
	// and before the step that runs them. A host that stops instead has acknowledged nothing, and a
	// client that was not answered sends its request again.
	void Acknowledge();

	// After a step: the completed requests' replies and each session's snapshot.
	void Publish();

  private:
	struct Session
	{
		Credential credential;
		// Queued here and not yet reported completed, oldest first.
		std::vector<std::uint64_t> pending;
		// Its actor held every session it may when it was verified: closed as ServerFull on admission.
		bool overCap{false};
		// The receipts of its queued requests not yet acknowledged, oldest first.
		std::vector<Reply> unacknowledged;
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
