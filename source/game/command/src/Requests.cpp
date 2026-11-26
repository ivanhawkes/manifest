#include <manifest/command/Requests.h>

#include <algorithm>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

#include <jettison/core/Assert.h>

namespace Manifest::Command
{
namespace
{
constexpr std::uint64_t kLast = std::numeric_limits<std::uint64_t>::max();


bool SameRequest(
    const Request &request, const std::string &command, std::uint32_t version,
    std::span<const std::byte> payload)
{
	return request.command == command && request.version == version &&
	       std::ranges::equal(std::as_bytes(std::span{request.payload}), payload);
}


bool SameRequest(const Request &left, const Request &right)
{
	return SameRequest(left, right.command, right.version, std::as_bytes(std::span{right.payload}));
}


ActorLedger Fresh(ActorKind kind, const std::string &id)
{
	ActorLedger ledger;
	ledger.kind = kind;
	ledger.id = id;
	return ledger;
}


Receipt Refused(Result result)
{
	return {Receipt::Status::Refused, result};
}


const Completed *FindCompleted(const ActorLedger &ledger, std::uint64_t sequence)
{
	const auto found = std::ranges::find(
	    ledger.completed, sequence,
	    [](const Completed &completed) { return completed.request.sequence; });
	return found == ledger.completed.end() ? nullptr : &*found;
}


const Request *FindPending(const ActorLedger &ledger, std::uint64_t sequence)
{
	const auto found = std::ranges::find(ledger.pending, sequence, &Request::sequence);
	return found == ledger.pending.end() ? nullptr : &*found;
}


// The ledger's rules, as Load holds a save to them: completed then pending, contiguous from floor to
// next and in increasing global order, each request its actor's, within the limits, none ordered
// after the last order assigned.
std::optional<std::string> Broken(const ActorLedger &ledger, std::uint64_t lastOrder)
{
	if (ledger.id.empty())
		return "an actor has no ID";
	if (ledger.id.size() > kMaxActorIdBytes)
		return "an actor's ID is longer than the limit";
	if (ledger.floor == 0 || ledger.floor > ledger.next)
		return std::format("'{}' has its floor above its next sequence", ledger.id);
	if (ledger.completed.size() > kRetainedResults || ledger.pending.size() > kPendingPerActor)
		return std::format("'{}' holds more requests than the limits allow", ledger.id);
	if (ledger.completed.size() + ledger.pending.size() != ledger.next - ledger.floor)
		return std::format("'{}' is missing requests between its floor and next", ledger.id);
	std::uint64_t sequence = ledger.floor;
	std::uint64_t order = 0;
	auto check = [&](const Request &request) -> std::optional<std::string>
	{
		if (request.actorKind != ledger.kind || request.actorId != ledger.id)
			return std::format("'{}' holds another actor's request", ledger.id);
		if (request.sequence != sequence++)
			return std::format("'{}' has its requests out of sequence", ledger.id);
		if (request.order == 0 || request.order > lastOrder)
			return std::format("'{}' has a request ordered after the last order", ledger.id);
		if (request.order <= order)
			return std::format("'{}' has its requests out of admission's order", ledger.id);
		if (request.payload.size() > kMaxPayloadBytes)
			return std::format("'{}' has a request larger than the payload limit", ledger.id);
		order = request.order;
		return std::nullopt;
	};
	for (const Completed &completed : ledger.completed)
		if (auto problem = check(completed.request))
			return problem;
	for (const Request &request : ledger.pending)
		if (auto problem = check(request))
			return problem;
	return std::nullopt;
}
} // namespace


void Requests::AddHandler(std::string name, Handler handler)
{
	if (!m_handlers.emplace(name, std::move(handler)).second)
		throw std::logic_error{std::format("the command '{}' is registered twice", name)};
}


Receipt Requests::Admit(
    Jettison::Sim::SimWorld &world, const Actor &actor, std::uint64_t sequence,
    const std::string &command, std::uint32_t version, std::span<const std::byte> payload)
{
	if (actor.id.empty() || actor.id.size() > kMaxActorIdBytes ||
	    (actor.kind == ActorKind::Principal &&
	     m_identities.Find(World::Kind::Principal, actor.id) == entt::null))
		return Refused(Result::NotAuthorised);

	// A sequence already admitted answers from the ledger before the handlers are asked, so a retry
	// still has its receipt after a restart that registers another version of its command.
	const auto known = m_actors.find({actor.kind, actor.id});
	const ActorLedger fresh = Fresh(actor.kind, actor.id);
	const ActorLedger &ledger = known == m_actors.end() ? fresh : known->second;
	if (sequence != 0 && sequence < ledger.floor)
		return Refused(Result::RequestTooOld);
	if (sequence != 0 && sequence < ledger.next)
	{
		if (const Completed *completed = FindCompleted(ledger, sequence))
			return SameRequest(completed->request, command, version, payload)
			           ? Receipt{Receipt::Status::Completed, completed->result,
			                     completed->request.order, completed->step}
			           : Refused(Result::RequestConflict);
		const Request *pending = FindPending(ledger, sequence);
		JETTISON_VERIFY(pending != nullptr);
		return SameRequest(*pending, command, version, payload)
		           ? Receipt{Receipt::Status::Queued, Result::Succeeded, pending->order}
		           : Refused(Result::RequestConflict);
	}

	const auto handler = m_handlers.find(command);
	if (handler == m_handlers.end() || handler->second.version != version)
		return Refused(Result::UnsupportedCommand);
	if (handler->second.sender != actor.kind)
		return Refused(Result::NotAuthorised);
	if (payload.size() > kMaxPayloadBytes)
		return Refused(Result::PayloadTooLarge);
	if (!handler->second.decodes(payload))
		return Refused(Result::MalformedPayload);
	if (sequence != ledger.next)
		return Refused(Result::SequenceGap);
	if (ledger.next == kLast || m_order == kLast)
		return Refused(Result::SequenceExhausted);
	if (ledger.pending.size() >= kPendingPerActor ||
	    world.Commands().Pending().size() >= kQueueCapacity)
		return Refused(Result::QueueFull);

	Request request{actor.kind, actor.id, sequence, ++m_order, command, version, {}};
	request.payload.reserve(payload.size());
	for (const std::byte byte : payload)
		request.payload.push_back(std::to_integer<std::uint8_t>(byte));
	world.Commands().Push(request);
	ActorLedger &admitted = m_actors.try_emplace({actor.kind, actor.id}, fresh).first->second;
	admitted.pending.push_back(std::move(request));
	++admitted.next;
	return {Receipt::Status::Queued, Result::Succeeded, m_order};
}


Result Requests::Act(
    Jettison::Sim::SimWorld &world, const Actor &actor, const std::string &command,
    std::uint32_t version, std::span<const std::byte> payload)
{
	const auto handler = m_handlers.find(command);
	if (handler == m_handlers.end() || handler->second.version != version)
		return Result::UnsupportedCommand;
	if (handler->second.sender != actor.kind || actor.id.empty() ||
	    (actor.kind == ActorKind::Principal &&
	     m_identities.Find(World::Kind::Principal, actor.id) == entt::null))
		return Result::NotAuthorised;
	if (!handler->second.decodes(payload))
		return Result::MalformedPayload;
	const std::uint64_t running = std::exchange(m_running, 0);
	const std::uint64_t order =
	    std::exchange(m_runningOrder, std::numeric_limits<std::uint64_t>::max());
	const Result result = handler->second.apply(world, actor, payload);
	m_running = running;
	m_runningOrder = order;
	return result;
}


std::vector<Registered> Requests::Commands() const
{
	std::vector<Registered> commands;
	commands.reserve(m_handlers.size());
	for (const auto &[name, handler] : m_handlers)
		commands.push_back({name, handler.version});
	return commands;
}


std::uint64_t Requests::NextSequence(const Actor &actor) const
{
	const auto known = m_actors.find({actor.kind, actor.id});
	return known == m_actors.end() ? 1 : known->second.next;
}


std::optional<Completed> Requests::Outcome(const Actor &actor, std::uint64_t sequence) const
{
	const auto known = m_actors.find({actor.kind, actor.id});
	if (known == m_actors.end())
		return std::nullopt;
	const Completed *completed = FindCompleted(known->second, sequence);
	return completed ? std::optional{*completed} : std::nullopt;
}


void Requests::Execute(Jettison::Sim::SimWorld &world, const Request &request)
{
	ActorLedger &ledger =
	    m_actors
	        .try_emplace(
	            {request.actorKind, request.actorId}, Fresh(request.actorKind, request.actorId))
	        .first->second;
	if (!ledger.pending.empty() && ledger.pending.front().sequence == request.sequence)
	{
		if (!SameRequest(ledger.pending.front(), request) ||
		    ledger.pending.front().order != request.order)
			throw std::logic_error{std::format(
			    "request {} of '{}' is not the one admitted", request.sequence, request.actorId)};
		ledger.pending.erase(ledger.pending.begin());
	}
	else if (
	    ledger.pending.empty() && request.sequence == ledger.next && request.order > m_order &&
	    ledger.next != kLast)
	{
		m_order = request.order;
		++ledger.next;
	}
	else if (const Completed *completed = FindCompleted(ledger, request.sequence);
	         completed && SameRequest(completed->request, request))
	{
		return;
	}
	else
	{
		throw std::logic_error{std::format(
		    "request {} of '{}' is not one its ledger admitted", request.sequence, request.actorId)};
	}

	const auto handler = m_handlers.find(request.command);
	const Actor actor{request.actorKind, request.actorId};
	m_running = request.sequence;
	m_runningOrder = request.order;
	const Result result =
	    handler == m_handlers.end() || handler->second.version != request.version
	        ? Result::UnsupportedCommand
	        : handler->second.apply(world, actor, std::as_bytes(std::span{request.payload}));
	m_running = 0;
	m_runningOrder = std::numeric_limits<std::uint64_t>::max();
	ledger.completed.push_back({request, world.Clock().Step(), result});
	if (ledger.completed.size() > kRetainedResults)
	{
		ledger.completed.erase(ledger.completed.begin());
		ledger.floor = ledger.completed.front().request.sequence;
	}
}


void Requests::Save(Jettison::Serialise::BinaryWriter &writer) const
{
	RequestLedger saved{m_order, {}};
	for (const ActorLedger &ledger : m_actors | std::views::values)
		saved.actors.push_back(ledger);
	writer.Write(saved);
}


std::function<void()> Requests::Load(
    Jettison::Serialise::BinaryReader &reader, const entt::registry & /*loaded*/,
    std::span<const entt::meta_any> queued)
{
	RequestLedger saved;
	reader.Read(saved);
	std::map<Key, ActorLedger> actors;
	for (ActorLedger &ledger : saved.actors)
	{
		if (const std::optional<std::string> problem = Broken(ledger, saved.order))
			reader.Fail(*problem);

		// Execute takes a request off its ledger before decoding it, so one it cannot read is refused
		// here rather than lost there.
		for (const Request &request : ledger.pending)
			if (const auto handler = m_handlers.find(request.command);
			    handler != m_handlers.end() && handler->second.version == request.version &&
			    !handler->second.decodes(std::as_bytes(std::span{request.payload})))
				reader.Fail(
				    std::format(
				        "request {} of '{}' has a payload its command cannot read", request.sequence,
				        ledger.id));
		Key key{ledger.kind, ledger.id};
		if (!actors.emplace(std::move(key), std::move(ledger)).second)
			reader.Fail("an actor's ledger is saved twice");
	}

	// A pending request the queue lacks would answer its retry as queued for ever, and two queued
	// out of admission's order would run out of it.
	if (queued.size() > kQueueCapacity)
		reader.Fail("the queue holds more commands than its limit");
	std::size_t matched = 0;
	std::uint64_t lastOrder = 0;
	for (const entt::meta_any &command : queued)
	{
		const auto *request = command.try_cast<const Request>();
		if (request == nullptr)
			continue;
		const auto ledger = actors.find({request->actorKind, request->actorId});
		const Request *admitted =
		    ledger == actors.end() ? nullptr : FindPending(ledger->second, request->sequence);
		if (admitted == nullptr || !SameRequest(*admitted, *request) ||
		    admitted->order != request->order || request->order <= lastOrder)
			reader.Fail(
			    std::format(
			        "request {} of '{}' is queued out of order or not pending in its ledger",
			        request->sequence, request->actorId));
		lastOrder = request->order;
		++matched;
	}
	std::size_t pending = 0;
	for (const ActorLedger &ledger : actors | std::views::values)
		pending += ledger.pending.size();
	if (matched != pending)
		reader.Fail("the ledger has a request pending that the queue does not hold");
	return [this, order = saved.order, actors = std::move(actors)]() mutable
	{
		m_order = order;
		m_actors = std::move(actors);
	};
}


Requests &RegisterRequests(Jettison::Sim::SimWorld &world, const World::Identities &identities)
{
	auto requests = std::make_unique<Requests>(identities);
	Requests &added = *requests;
	world.RegisterCommand<Request>([&added](Jettison::Sim::SimWorld &target, const Request &request)
	                               { added.Execute(target, request); });
	world.AddService("Manifest.Requests", std::move(requests));
	return added;
}
} // namespace Manifest::Command
