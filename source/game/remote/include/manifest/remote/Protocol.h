#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/net/Protocol.h>

#include <manifest/command/Requests.h>

namespace Manifest::Remote
{
// Manifest's protocol on Jettison::Net (docs/gameplay-and-server-plan.md, "Network contract"). Each
// body is the binary archive's, at the protocol's version, so a message's shape changing is a new
// version. A snapshot's body is a Query::Snapshot, the session's own view.
inline constexpr std::string_view kProtocol = "manifest";
// 2: a snapshot holds its ships' journeys and its company's orders (W15).
inline constexpr std::uint32_t kVersion = 2;

enum class Kind : Jettison::Net::MessageKind
{
	// Server to client, once admitted: who the token named and the sequence to send next.
	Welcome = 1,
	// Client to server: a request, its actor the session's.
	Submit,
	// Server to client: a request's receipt at admission, then Completed once it has run.
	Reply
};

struct Welcome
{
	Command::ActorKind actorKind{Command::ActorKind::Principal};
	std::string actorId;
	std::uint64_t nextSequence{1};
};

struct Submit
{
	std::uint64_t sequence{0};
	std::string command;
	std::uint32_t version{1};
	std::vector<std::uint8_t> payload;
};

struct Reply
{
	std::uint64_t sequence{0};
	Command::Receipt receipt;
	// The actor's next sequence after this: a refusal consumes none, so the client sends it again.
	std::uint64_t nextSequence{1};
};

// The messages, the receipt and the snapshot, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::Remote
