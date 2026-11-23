#pragma once

#include <entt/entity/registry.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/command/Request.h>
#include <manifest/world/Identities.h>

namespace Manifest::Command
{
// What admission answers: queued, with its global order; completed, for a retry of one that has
// run; or refused, with why. Neither queued nor completed is a durable acknowledgement yet.
struct Receipt
{
	enum class Status
	{
		Queued,
		Completed,
		Refused
	};

	Status status{Status::Refused};
	Result result{Result::Succeeded};
	std::uint64_t order{0};
	// For a completed request, the step it ran at.
	std::uint64_t step{0};
};

// The one admission path into the world and the ledger of every actor's requests
// (docs/stage-3-authority-plan.md, "Command boundary and ordering" and "Retry and result contract").
// Admission checks the actor, the command and the sequence, assigns the next global order and
// queues the request on the world, so it runs at the start of the next step in admission order.
// Running it rechecks nothing admission promised: its command's handler checks the world as it then
// is. The ledger is saved; the handlers are registered again at start-up, as the world's are.
class Requests final : public Jettison::Sim::WorldService
{
  public:
	// The identities must outlive this: the world's own service.
	explicit Requests(const World::Identities &identities) : m_identities{identities}
	{
	}

	// How a command runs: its payload read as T at its version, then applied. Only an actor of the
	// sender's kind may send it. A handler returning anything but Succeeded has written nothing a
	// rejection must preserve. Throws std::logic_error for a name registered twice.
	template <typename T>
	void Register(
	    std::string name, std::uint32_t version, ActorKind sender,
	    std::function<Result(Jettison::Sim::SimWorld &, const Actor &, const T &)> apply)
	{
		const Jettison::Serialise::FormatVersion format{version};
		AddHandler(
		    std::move(name),
		    {version, sender,
		     [format](std::span<const std::byte> payload)
		     {
			     try
			     {
				     (void)Jettison::Serialise::BinaryArchive::Read<T>(payload, format);
				     return true;
			     }
			     catch (const Jettison::Serialise::FormatError &)
			     {
				     return false;
			     }
		     },
		     [format, apply = std::move(apply)](
		         Jettison::Sim::SimWorld &world, const Actor &actor, std::span<const std::byte> payload)
		     {
			     return apply(
			         world, actor, Jettison::Serialise::BinaryArchive::Read<T>(payload, format));
		     }});
	}

	// The one way a request reaches the world. A refusal consumes no sequence; a sequence already
	// admitted answers as the request it named did, or RequestConflict for a different command or
	// payload, and one pruned from retention is RequestTooOld, never a new request.
	Receipt Admit(
	    Jettison::Sim::SimWorld &world, const Actor &actor, std::uint64_t sequence,
	    const std::string &command, std::uint32_t version, std::span<const std::byte> payload);

	// Runs the command's handler for the actor at once, as a request admitted for them would run, for
	// an agent of the world's own, a standing policy's job (D-097): no admission, sequence, ledger or
	// queue, and nothing recorded, since the state that sent it sends it again in a replay, as
	// Jettison's SimWorld::Apply does. Running() is 0 while it runs. UnsupportedCommand for a command
	// not registered at the version, NotAuthorised for an actor not of its sender's kind or a principal
	// the world does not have, MalformedPayload for a payload it cannot read.
	Result Act(
	    Jettison::Sim::SimWorld &world, const Actor &actor, const std::string &command,
	    std::uint32_t version, std::span<const std::byte> payload);

	template <typename T>
	Result Act(
	    Jettison::Sim::SimWorld &world, const Actor &actor, std::string_view command,
	    std::uint32_t version, const T &payload)
	{
		const std::vector<std::byte> bytes = Jettison::Serialise::BinaryArchive::Write(
		    payload, Jettison::Serialise::FormatVersion{version});
		return Act(world, actor, std::string{command}, version, std::span<const std::byte>{bytes});
	}

	// The sequence of the request whose handler is running, for a handler to record; 0 outside one.
	std::uint64_t Running() const
	{
		return m_running;
	}

	// The global order of the request whose handler is running, for a handler to record; past every
	// request's outside one, as an agent's Act is, which runs after its step's requests.
	std::uint64_t RunningOrder() const
	{
		return m_runningOrder;
	}

	// The sequence an actor submits next, advertised on reconnect: 1 for one never seen.
	std::uint64_t NextSequence(const Actor &actor) const;

	// None for a sequence pending, pruned or never admitted.
	std::optional<Completed> Outcome(const Actor &actor, std::uint64_t sequence) const;

	// The world's handler for a Request, as admission queued it or a replay delivers it. A replayed
	// request admitted after the start the replay began from advances its actor's ledger here, so
	// replay rebuilds the ledger without admission; one already completed is not run twice. Throws
	// std::logic_error for a request the ledger cannot have admitted.
	void Execute(Jettison::Sim::SimWorld &world, const Request &request);

	void Save(Jettison::Serialise::BinaryWriter &writer) const override;

	// Fails the save for a ledger that breaks its own rules or the limits, or whose pending requests
	// are not exactly the queued ones, in the order admission gave them.
	std::function<void()> Load(
	    Jettison::Serialise::BinaryReader &reader, const entt::registry &loaded,
	    std::span<const entt::meta_any> queued) override;

  private:
	struct Handler
	{
		std::uint32_t version{1};
		ActorKind sender{ActorKind::Principal};
		std::function<bool(std::span<const std::byte>)> decodes;
		std::function<Result(Jettison::Sim::SimWorld &, const Actor &, std::span<const std::byte>)>
		    apply;
	};

	using Key = std::pair<ActorKind, std::string>;

	void AddHandler(std::string name, Handler handler);

	const World::Identities &m_identities;
	std::map<std::string, Handler> m_handlers;
	std::uint64_t m_order{0};
	std::map<Key, ActorLedger> m_actors;
	std::uint64_t m_running{0};
	std::uint64_t m_runningOrder{std::numeric_limits<std::uint64_t>::max()};
};

// Registers the Request command on the world and adds the Requests service, which the world owns
// and the returned reference reaches for the world's lifetime. RegisterTypes must have run.
Requests &RegisterRequests(Jettison::Sim::SimWorld &world, const World::Identities &identities);
} // namespace Manifest::Command
