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
	AlreadyThere,
	// Provisioning at a starter venue whose life support has no connection free (D-047).
	NoServiceFree,
	// A system's tier bought for a ship that has it already.
	AlreadyFitted,
	// A system the ship cannot take: no slot for it, a prerequisite it lacks, or a tier beyond its
	// next or the offer's.
	NotFittable,
	// A venue's fees whose arrears rate is above the tuning's usury limit (D-100).
	RateTooHigh,
	// Travel for a leg the ship's fuel cannot fly at the burn chosen (D-107).
	InsufficientFuel,
	// A tender closed, or past the close its command must come before (W12).
	Closed,
	// A bound of procurement's reached: a region's tenders not yet closed, or a tender's bids (W12).
	LimitReached,
	// A bid replaced at other than its current revision (W12).
	StaleRevision,
	// Founding in a region that has a governor, or by a principal who directs one's already (D-110).
	Governed,
	AlreadyFounded,
	// A project queued in a region that has it queued, under way or done (D-110).
	AlreadyQueued,
	// Travel whose drive would still thrust within its exclusion radius once its course is settled, or
	// that has no course round the bodies in the way (W22b).
	InsideRadius,
	NoCourse,
	// Travel to a venue no pad of which the ship's hull fits (W22b).
	NoPadFits,
	// A licence applied for that the company holds already, or a grant it has claimed (W22b).
	AlreadyLicensed,
	AlreadyClaimed,
	// A company's or a ship's name empty once trimmed, longer than its bound, or not printable: bytes
	// that are not UTF-8, or a control character (D-123).
	NameEmpty,
	NameTooLong,
	NameNotPrintable
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

// A command as it is registered: what a world's identity holds a build to (D-111).
struct Registered
{
	std::string name;
	std::uint32_t version{1};

	bool operator==(const Registered &) const = default;
};

// The limits are part of the command-state contract: a save made under one set is checked against it.
inline constexpr std::size_t kRetainedResults = 256;
inline constexpr std::size_t kPendingPerActor = 32;
inline constexpr std::size_t kQueueCapacity = 1024;
inline constexpr std::size_t kMaxPayloadBytes = 4096;

// An actor's ID, so a journal batch of the most requests stays under what a start reads.
inline constexpr std::size_t kMaxActorIdBytes = 256;

// The types above, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::Command
