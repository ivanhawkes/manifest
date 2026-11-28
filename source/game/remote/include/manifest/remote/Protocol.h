#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/net/Protocol.h>

#include <manifest/command/Requests.h>

namespace Manifest::Remote
{
// Manifest's protocol on Jettison::Net (docs/server.md, "The messages"). Each body is the binary
// archive's, at the protocol's version, so a message's shape changing is a new version, unless it
// changes in place as formats do until player testing (D-109): the welcome's world content came so, at
// 5 (L53). A snapshot's body is a Query::Snapshot, the session's own view.
inline constexpr std::string_view kProtocol = "manifest";

// 2: a snapshot holds its ships' journeys and its company's orders (W15).
// 3: and its ships' life support and opened totes (W06).
// 4: and its player's market reports (W07).
// 5: and its ships' holds and systems (W08).
inline constexpr std::uint32_t kVersion = 5;

// The longest message either end sends or takes, a snapshot being the longest: three times the trading
// scenario's 63,567 bytes at step 0 (2026-11-23), which the engine's 64 KiB default dropped ten orders
// later, and under the 256 KiB at which an unreliable message still crosses whole (Jettison's
// architecture, "Networking"). A longer view is logged and not sent, and wants deltas.
inline constexpr std::size_t kMaxMessageBytes = std::size_t{192} * 1024;

enum class Kind : Jettison::Net::MessageKind
{
	// Server to client, once admitted: who the token named, the sequence to send next and the world
	// content the server runs on, which the client checks against its own.
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

	// Jettison::Host::EncodeWorldContent's bytes, of the server's session.
	std::vector<std::uint8_t> content;
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
