#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Manifest::Command
{
// Every outcome a request can have: Succeeded, or why it was refused at admission or rejected when
// it ran. A rejection is an expected answer, not an error; the frontend owns the words for it.
enum class Result
{
	Succeeded,
	// At admission, consuming no sequence.
	NotAuthorised,
	UnsupportedCommand,
	MalformedPayload,
	PayloadTooLarge,
	QueueFull,
	SequenceGap,
	SequenceExhausted,
	// At admission, for a sequence already used.
	RequestConflict,
	RequestTooOld,
	// When it runs, from its command's rules.
	UnknownTarget,
	InsufficientFunds,
	// Provisioning a principal that already has a company.
	AlreadyProvisioned,
	NotDocked,
	WrongVenue,
	// Too little available at the venue, or aboard, for the quantity.
	InsufficientStock,
	CapacityExceeded,
	// Not finite, not above 0, or one whose arithmetic would not be.
	InvalidQuantity,
	// An order's limit not above 0, or one whose cost the ledger cannot hold.
	InvalidPrice,
	// The side of the venue's book an order would rest on holds kMaxRestingOrders.
	BookFull,
	// Provisioning at a starter venue with no free pad the starter hull fits.
	NoPadFree,
	// Travel for a ship whose journey is still under way, or to the venue it is docked at.
	Travelling,
	AlreadyThere
};

enum class ActorKind
{
	// A player, by their Principal's ID, which must exist in the world.
	Principal,
	// The host itself, with the administrative capability. Only a trusted caller makes one.
	Administrator
};

// Who sends a request, as the trusted local harness resolved it. Never read from a payload.
struct Actor
{
	ActorKind kind{ActorKind::Principal};
	std::string id;
};

// A request as the world queues, saves, records and replays it: the command's name and version and
// its payload in the binary archive's canonical bytes, under the identity (actor, sequence) and the
// global order admission assigned. The bytes are what an identical retry is compared on.
struct Request
{
	ActorKind actorKind{ActorKind::Principal};
	std::string actorId;
	std::uint64_t sequence{0};
	std::uint64_t order{0};
	std::string command;
	std::uint32_t version{1};
	std::vector<std::uint8_t> payload;
};

// A request that has run, kept while it is within its actor's retention.
struct Completed
{
	Request request;
	// The clock's step it ran at the start of.
	std::uint64_t step{0};
	Result result{Result::Succeeded};
};

// One actor's progress: every sequence below `next` has been admitted, those from `floor` are
// completed (oldest first) then pending, and those below `floor` are pruned, refused as too old.
struct ActorLedger
{
	ActorKind kind{ActorKind::Principal};
	std::string id;
	std::uint64_t next{1};
	std::uint64_t floor{1};
	std::vector<Completed> completed;
	std::vector<Request> pending;
};

// The saved part of Requests.
struct RequestLedger
{
	// The last global order assigned.
	std::uint64_t order{0};
	// In (kind, id) order.
	std::vector<ActorLedger> actors;
};

// The limits are part of the command-state contract: a save made under one set is checked against it.
inline constexpr std::size_t kRetainedResults = 256;
inline constexpr std::size_t kPendingPerActor = 32;
inline constexpr std::size_t kQueueCapacity = 1024;
inline constexpr std::size_t kMaxPayloadBytes = 4096;

// The types above, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::Command
