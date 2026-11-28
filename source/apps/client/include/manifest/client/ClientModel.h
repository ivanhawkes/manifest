#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/host/Start.h>
#include <manifest/query/Query.h>
#include <manifest/remote/Client.h>

namespace Manifest::Client
{
// One good of the market where the ship is docked, as the market screen shows it: its best bid and
// ask in credits, which the screen writes in the language's form, and as the raw prices an order is
// placed at, 0 for a side with none; the totes resting at each, and the totes of it the company holds
// there and the ship has aboard.
struct GoodRow
{
	std::string itemId;
	double bid{0.0};
	std::int64_t bidRaw{0};
	std::uint64_t bidTotes{0};
	double ask{0.0};
	std::int64_t askRaw{0};
	std::uint64_t askTotes{0};
	std::uint64_t held{0};
	std::uint64_t aboard{0};

	bool operator==(const GoodRow &) const = default;
};

struct LotRow
{
	std::string itemId;
	std::uint64_t totes{0};

	bool operator==(const LotRow &) const = default;
};

// A ship as the company and flight screens show it: where it is docked, empty under way; its journey's
// destination and leg (Manifest.World.Leg's name), empty before its first; its hold; its fuel and its
// tanks' in kilograms, whole; and its life support's oxygen in kilograms and scrubbers in
// occupant-days, the venue it is connected to, and whether it refuels as it docks.
struct ShipRow
{
	std::string shipId;
	std::string hullId;
	std::string dockedAt;
	std::string destination;
	std::string leg;
	bool underWay{false};
	std::vector<LotRow> cargo;
	std::uint64_t aboard{0};
	std::uint64_t hold{0};
	std::int64_t reactionMass{0};
	std::int64_t reactionMassMost{0};
	std::int64_t pellets{0};
	std::int64_t pelletsMost{0};
	std::int64_t oxygen{0};
	std::int64_t scrubbers{0};
	std::string connectedTo;

	bool operator==(const ShipRow &) const = default;
};

struct HoldingRow
{
	std::string venueId;
	std::string itemId;
	std::uint64_t available{0};
	std::uint64_t reserved{0};

	bool operator==(const HoldingRow &) const = default;
};

// A resting order: its side, Manifest.World.Side's name, and its limit in credits.
struct OrderRow
{
	std::string orderId;
	std::string venueId;
	std::string itemId;
	std::string side;
	double price{0.0};
	std::uint64_t remaining{0};
	std::uint64_t totes{0};

	bool operator==(const OrderRow &) const = default;
};

// What the client's screens show (docs/client.md, "Screens"), made from the player's snapshot alone.
// The screens act on the company's first ship, the only one the first loop has. The destinations are
// the venues the player has a market report of, but where the ship is docked; the travel settings are
// the next journey's, its thrust in percent of the cruise and its acceleration in tenths of g (D-107).
// The day is the world's, from 1, with its time of day in minutes since midnight; status is the
// client's words for the last request's answer, which the client fills in. Connected, the world is a
// server's, which steps it itself.
struct Screen
{
	std::string principal;
	std::string companyName;
	bool provisioned{false};
	std::string companyId;
	std::int64_t credits{0};
	std::vector<ShipRow> ships;
	std::vector<HoldingRow> holdings;
	std::vector<OrderRow> orders;
	std::string venueId;
	bool live{false};
	std::vector<GoodRow> goods;
	std::vector<std::string> destinations;
	std::int64_t thrustPercent{0};
	std::int64_t accelerationTenths{0};
	bool fastForward{true};
	std::uint64_t day{1};
	std::int64_t minutes{0};
	std::string status;
	bool connected{false};

	bool operator==(const Screen &) const = default;
};

// The client's own part of a save, which the world does not hold: the player, and the next journey's
// settings. The shell's saves carry it as the section kSaveSection, a binary archive at kSaveVersion.
struct ClientState
{
	std::string principal;
	std::int64_t thrustPercent{0};
	std::int64_t accelerationTenths{0};
	bool fastForward{true};

	bool operator==(const ClientState &) const = default;
};

inline constexpr std::string_view kSaveSection = "Manifest.Client";
inline constexpr std::uint32_t kSaveVersion = 1;

std::vector<std::byte> WriteState(const ClientState &state);

// Throws Jettison::Serialise::FormatError for bytes that are not a ClientState of this version.
ClientState ReadState(std::span<const std::byte> bytes);

// Registers the screen's types and ClientState with Reflect, for the client's data model and its
// saves; safe to call again.
void RegisterTypes();

// The player's side of a world (docs/client.md, "The model"), with nothing of the UI in it: what the
// screens show, made from the player's snapshot, and their actions sent as the console sends them.
// Over a local world the snapshot is Query::Observe as the player's actor and each action goes through
// Command::Requests::Admit with the player's next sequence, called between steps on the thread that
// steps the world. Over a server's connection (Remote::Client) the snapshot is the newest the server
// sent and each action is submitted to it, as the welcome's actor: a receipt Queued then means sent,
// its order still to come, and an action before the welcome or after the closing is refused
// NotAuthorised before it is sent.
class ClientModel
{
  public:
	// The world's services are those of the world given, and both outlive the model.
	ClientModel(Jettison::Sim::SimWorld &world, const Host::Services &services);

	// Over a connection, which outlives the model and which it polls.
	explicit ClientModel(Remote::Client &remote);

	// Why the connection ended, once it has, a server's message the client cannot read included
	// (BadMessage); none over a local world.
	std::optional<Jettison::Net::Reason> Closed() const;

	// Why the server's welcome was refused, its world content not the client's; none over a local
	// world.
	std::optional<std::string> Refused() const;

	// The player, once Provision has named them or a save has.
	const std::string &Principal() const
	{
		return m_screen.principal;
	}

	void SetPrincipal(std::string principal);

	// What a save keeps of the model, and the model as a save left it.
	ClientState State() const;

	void Restore(const ClientState &state);

	// The snapshot and the screen made from it again, and the last request's answer taken if it has
	// come; true when the screen changed.
	bool Refresh();

	const Query::Snapshot &Snapshot() const
	{
		return m_snapshot;
	}

	const Screen &View() const
	{
		return m_screen;
	}

	// The last request sent: none before the first; Succeeded or why not once it has run or been
	// refused, and none while it waits for its step.
	bool Waiting() const
	{
		return m_waiting.has_value();
	}

	std::optional<Command::Result> LastResult() const
	{
		return m_last;
	}

	// The player's company provisioned, as the host does for a new player: the principal named and
	// given the company. Refused for a principal the model has already. Over a connection the
	// principal is the welcome's, whatever is given (D-122).
	Command::Receipt Provision(std::string principal, std::string companyName);

	// An order for a tote of the good at the venue where the ship is docked, at its best ask or bid;
	// and an order withdrawn.
	Command::Receipt Buy(std::string_view itemId);

	Command::Receipt Sell(std::string_view itemId);

	Command::Receipt Cancel(std::string orderId);

	// All the company holds of the good there moved aboard, or all the ship has of it ashore.
	Command::Receipt Load(std::string_view itemId);

	Command::Receipt Unload(std::string_view itemId);

	// The next journey's settings, kept within what Travel takes, and the journey begun to a venue.
	void SetThrust(std::int64_t percent);

	void SetAcceleration(std::int64_t tenths);

	Command::Receipt Travel(std::string venueId);

	// The ship's life support resupplied where it is docked: its tanks refilled, its scrubbers
	// replaced; and whether it refuels as it docks.
	Command::Receipt Refill();

	Command::Receipt Replace();

	Command::Receipt SetRefuel(bool enabled);

	// Whether the client passes over a journey's steps, and whether a ship of the player's is under
	// way: of a server's world, as its newest snapshot shows it.
	void SetFastForward(bool enabled);

	bool UnderWay() const;

	// The world stepped on while a journey is under way, its quiet steps passed over
	// (Jettison::Sim::SimWorld::FastForward), for at most the budget and to no further than the step
	// the journey ends at: the steps taken. A frame calls it once; nothing is taken with fast-forward
	// off, no journey under way or over a connection, whose server steps its own world.
	std::uint64_t FastForward(std::chrono::nanoseconds budget);

  private:
	template <typename T>
	Command::Receipt Send(std::string_view command, std::uint32_t version, const T &payload);

	// A refusal made here, before anything is sent, answered as the world's are.
	Command::Receipt Refuse(Command::Result result);

	// The replies the connection has, the answer waited on taken from them.
	void Receive();

	Command::Receipt Admit(
	    const Command::Actor &actor, std::string_view command, std::uint32_t version,
	    std::span<const std::byte> payload);

	const Query::ShipView *Ship() const;

	void MakeScreen();

	// One of the world and its services, or the connection.
	Jettison::Sim::SimWorld *m_pWorld{nullptr};
	std::optional<Host::Services> m_services;
	Remote::Client *m_pRemote{nullptr};

	// The server sent what the client cannot read, so nothing more of it is.
	bool m_unreadable{false};
	Query::Snapshot m_snapshot;
	Screen m_screen;

	// The request whose answer is still to come, and the last answer.
	std::optional<std::pair<Command::Actor, std::uint64_t>> m_waiting;
	std::optional<Command::Result> m_last;
};
} // namespace Manifest::Client
