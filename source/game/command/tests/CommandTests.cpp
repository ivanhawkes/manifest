#include "TestHelpers.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>
#include <jettison/serialise/Binary.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/world/World.h>

using namespace Manifest::Command;
using Jettison::Serialise::BinaryArchive;
using Jettison::Serialise::FormatVersion;
using Jettison::Sim::SimWorld;
using Manifest::World::Account;
using Manifest::World::Kind;
using Status = Receipt::Status;

namespace
{
// The test's command: credits or debits a region's public account, never below zero.
struct Grant
{
	std::string regionId;
	std::int64_t amount{0};
};

// An administrative one, which changes nothing.
struct Audit
{
	std::string note;
};

constexpr std::uint32_t kGrantVersion = 1;


void RegisterTestTypes()
{
	RegisterTypes();
	static const bool registered = []
	{
		using namespace Jettison::Reflect;
		RegisterType<Grant>("Test.Grant")
		    .Field<&Grant::regionId>("regionId", Attribute::Saved)
		    .Field<&Grant::amount>("amount", Attribute::Saved);
		RegisterType<Audit>("Test.Audit").Field<&Audit::note>("note", Attribute::Saved);
		return true;
	}();
	(void)registered;
}


Result ApplyGrant(
    SimWorld &world, const Actor & /*actor*/, const Grant &grant,
    Manifest::World::Identities &identities)
{
	const entt::entity account = identities.Find(Kind::PublicAccount, grant.regionId);
	if (account == entt::null)
		return Result::UnknownTarget;
	std::int64_t &balance = world.Registry().get<Account>(account).balance;
	if (balance + grant.amount < 0)
		return Result::InsufficientFunds;
	balance += grant.amount;
	return Result::Succeeded;
}


// A world as the host registers one, with two principals and one region's account.
struct Hosted
{
	explicit Hosted(std::uint32_t grantVersion = kGrantVersion)
	    : identities{(RegisterTestTypes(), Manifest::World::RegisterWorld(world))},
	      requests{RegisterRequests(world, identities)}
	{
		requests.Register<Grant>(
		    "Test.Grant", grantVersion, ActorKind::Principal,
		    [this](SimWorld &target, const Actor &actor, const Grant &grant)
		    { return ApplyGrant(target, actor, grant, identities); });
		requests.Register<Audit>(
		    "Test.Audit", 1, ActorKind::Administrator,
		    [](SimWorld & /*world*/, const Actor & /*actor*/, const Audit & /*audit*/)
		    { return Result::Succeeded; });
	}

	void Populate()
	{
		entt::registry &registry = world.Registry();
		identities.Create(registry, Kind::Principal, "Ada");
		identities.Create(registry, Kind::Principal, "Bea");
		registry.emplace<Account>(identities.Create(registry, Kind::PublicAccount, "RegionA"), 100);
	}

	std::int64_t Balance() const
	{
		return world.Registry().get<Account>(identities.Find(Kind::PublicAccount, "RegionA")).balance;
	}

	Receipt Send(const Actor &actor, std::uint64_t sequence, std::int64_t amount)
	{
		const std::vector<std::byte> payload =
		    BinaryArchive::Write(Grant{"RegionA", amount}, FormatVersion{kGrantVersion});
		return requests.Admit(world, actor, sequence, "Test.Grant", kGrantVersion, payload);
	}

	SimWorld world{1.0, 0};
	Manifest::World::Identities &identities;
	Requests &requests;
};

const Actor kAda{ActorKind::Principal, "Ada"};
const Actor kBea{ActorKind::Principal, "Bea"};
const Actor kHost{ActorKind::Administrator, "Host"};


// A saved world whose Requests state is a ledger written as given, for a save no admission makes.
class Forged final : public Jettison::Sim::WorldService
{
  public:
	explicit Forged(RequestLedger ledger) : m_ledger{std::move(ledger)}
	{
	}

	void Save(Jettison::Serialise::BinaryWriter &writer) const override
	{
		writer.Write(m_ledger);
	}

	std::function<void()> Load(
	    Jettison::Serialise::BinaryReader & /*reader*/, const entt::registry & /*loaded*/,
	    std::span<const entt::meta_any> /*queued*/) override
	{
		return [] {};
	}

  private:
	RequestLedger m_ledger;
};


ActorLedger Ledger(
    const std::string &id, std::uint64_t next, std::uint64_t floor, std::vector<Request> pending = {})
{
	ActorLedger ledger;
	ledger.id = id;
	ledger.next = next;
	ledger.floor = floor;
	ledger.pending = std::move(pending);
	return ledger;
}


Request Queued(const std::string &actorId, std::uint64_t sequence, std::uint64_t order)
{
	Request request;
	request.actorId = actorId;
	request.sequence = sequence;
	request.order = order;
	request.command = "Test.Grant";
	return request;
}


std::vector<std::byte> ForgedSave(RequestLedger ledger, const std::vector<Request> &queue = {})
{
	RegisterTestTypes();
	SimWorld world{1.0, 0};
	Manifest::World::RegisterWorld(world);
	world.AddService("Manifest.Requests", std::make_unique<Forged>(std::move(ledger)));
	for (const Request &request : queue)
		world.Commands().Push(request);
	return world.Save();
}
} // namespace


TEST_CASE("A refused admission consumes no sequence")
{
	Hosted hosted;
	hosted.Populate();
	const std::vector<std::byte> grant = BinaryArchive::Write(Grant{"RegionA", 1}, FormatVersion{1});
	const std::vector<std::byte> audit = BinaryArchive::Write(Audit{"x"}, FormatVersion{1});
	auto refusal = [&](const Actor &actor, std::uint64_t sequence, const std::string &command,
	                   std::uint32_t version, std::span<const std::byte> payload)
	{
		const Receipt receipt =
		    hosted.requests.Admit(hosted.world, actor, sequence, command, version, payload);
		CHECK(receipt.status == Status::Refused);
		return receipt.result;
	};
	const Actor stranger{ActorKind::Principal, "Nobody"};
	CHECK(refusal(stranger, 1, "Test.Grant", 1, grant) == Result::NotAuthorised);
	CHECK(refusal(kAda, 1, "Test.Unknown", 1, grant) == Result::UnsupportedCommand);
	CHECK(refusal(kAda, 1, "Test.Grant", 2, grant) == Result::UnsupportedCommand);
	CHECK(refusal(kAda, 1, "Test.Audit", 1, audit) == Result::NotAuthorised);
	CHECK(refusal(kHost, 1, "Test.Grant", 1, grant) == Result::NotAuthorised);
	const std::vector<std::byte> huge(kMaxPayloadBytes + 1);
	CHECK(refusal(kAda, 1, "Test.Grant", 1, huge) == Result::PayloadTooLarge);
	const std::vector<std::byte> damaged(grant.begin(), grant.begin() + 6);
	CHECK(refusal(kAda, 1, "Test.Grant", 1, damaged) == Result::MalformedPayload);
	CHECK(refusal(kAda, 0, "Test.Grant", 1, grant) == Result::SequenceGap);
	CHECK(refusal(kAda, 2, "Test.Grant", 1, grant) == Result::SequenceGap);
	CHECK(hosted.requests.NextSequence(kAda) == 1);
	CHECK(hosted.world.Commands().Pending().empty());

	CHECK(
	    hosted.requests.Admit(hosted.world, kHost, 1, "Test.Audit", 1, audit).status == Status::Queued);
}


TEST_CASE("Nothing runs before its step, and the second of two conflicting requests sees the first")
{
	Hosted hosted;
	hosted.Populate();
	const Receipt first = hosted.Send(kBea, 1, -80);
	const Receipt second = hosted.Send(kAda, 1, -80);
	REQUIRE(first.status == Status::Queued);
	REQUIRE(second.status == Status::Queued);
	CHECK(first.order == 1);
	CHECK(second.order == 2);
	CHECK(hosted.Balance() == 100);

	hosted.world.Step();
	CHECK(hosted.Balance() == 20);
	const std::optional<Completed> bea = hosted.requests.Outcome(kBea, 1);
	const std::optional<Completed> ada = hosted.requests.Outcome(kAda, 1);
	REQUIRE(bea);
	REQUIRE(ada);
	CHECK(bea->result == Result::Succeeded);
	CHECK(ada->result == Result::InsufficientFunds);
	CHECK(bea->step == 0);
	// A rejection still spends its sequence.
	CHECK(hosted.requests.NextSequence(kAda) == 2);
}


TEST_CASE("A retry runs once, and a sequence reused for something else is a conflict")
{
	Hosted hosted;
	hosted.Populate();
	const Receipt queued = hosted.Send(kAda, 1, 5);
	const Receipt pendingRetry = hosted.Send(kAda, 1, 5);
	CHECK(pendingRetry.status == Status::Queued);
	CHECK(pendingRetry.order == queued.order);
	CHECK(hosted.world.Commands().Pending().size() == 1);
	CHECK(hosted.Send(kAda, 1, 6).result == Result::RequestConflict);

	hosted.world.Step();
	hosted.world.Step();
	const Receipt completedRetry = hosted.Send(kAda, 1, 5);
	CHECK(completedRetry.status == Status::Completed);
	CHECK(completedRetry.result == Result::Succeeded);
	CHECK(completedRetry.step == 0);
	CHECK(hosted.world.Commands().Pending().empty());
	CHECK(hosted.Send(kAda, 1, 6).result == Result::RequestConflict);
	CHECK(hosted.Balance() == 105);
}


TEST_CASE("Pending requests are bounded per actor, and a full queue consumes no sequence")
{
	Hosted hosted;
	hosted.Populate();
	for (std::uint64_t sequence = 1; sequence <= kPendingPerActor; ++sequence)
		REQUIRE(hosted.Send(kAda, sequence, 1).status == Status::Queued);
	CHECK(hosted.Send(kAda, kPendingPerActor + 1, 1).result == Result::QueueFull);
	CHECK(hosted.requests.NextSequence(kAda) == kPendingPerActor + 1);
	CHECK(hosted.Send(kBea, 1, 1).status == Status::Queued);
}


TEST_CASE("Pruned results are too old, never run again")
{
	Hosted hosted;
	hosted.Populate();
	std::uint64_t sequence = 1;
	while (sequence <= kRetainedResults + 1)
	{
		for (std::size_t batch = 0; batch < kPendingPerActor && sequence <= kRetainedResults + 1;
		     ++batch)
			REQUIRE(hosted.Send(kAda, sequence++, 1).status == Status::Queued);
		hosted.world.Step();
	}
	CHECK(hosted.Send(kAda, 1, 1).result == Result::RequestTooOld);
	CHECK(hosted.Send(kAda, 1, 2).result == Result::RequestTooOld);
	CHECK(hosted.Send(kAda, 2, 1).status == Status::Completed);
	CHECK_FALSE(hosted.requests.Outcome(kAda, 1));
	CHECK(hosted.Balance() == 100 + static_cast<std::int64_t>(kRetainedResults) + 1);
}


TEST_CASE("A save keeps pending requests, progress and results, and the loaded world runs each once")
{
	Hosted hosted;
	hosted.Populate();
	hosted.Send(kAda, 1, 10);
	hosted.world.Step();
	hosted.Send(kAda, 2, 20);
	hosted.Send(kBea, 1, -1000);
	const std::vector<std::byte> save = hosted.world.Save();

	Hosted loaded;
	loaded.world.Load(save);
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.requests.NextSequence(kAda) == 3);
	CHECK(loaded.Send(kAda, 2, 20).status == Status::Queued);
	CHECK(loaded.world.Commands().Pending().size() == 2);

	hosted.world.Step();
	loaded.world.Step();
	CHECK(loaded.world.StateHash() == hosted.world.StateHash());
	CHECK(loaded.Balance() == 130);
	REQUIRE(loaded.requests.Outcome(kBea, 1));
	CHECK(loaded.requests.Outcome(kBea, 1)->result == Result::InsufficientFunds);
	CHECK(loaded.Send(kAda, 1, 10).status == Status::Completed);
}


TEST_CASE("A replay of the recorded requests rebuilds the world and its ledger without admission")
{
	Hosted hosted;
	hosted.Populate();
	const std::vector<std::byte> start = hosted.world.Save();
	hosted.world.SetRecording(true);
	hosted.Send(kAda, 1, 10);
	hosted.Send(kBea, 1, -200);
	hosted.world.Step();
	hosted.world.Step();
	hosted.Send(kBea, 2, -30);
	hosted.world.Step();

	Hosted replayed;
	replayed.world.Load(start);
	for (std::uint64_t step = 0; step < 3; ++step)
	{
		for (const Jettison::Sim::AppliedCommand &applied : hosted.world.Recorded())
			if (applied.step == step)
				replayed.world.Commands().Push(applied.command);
		replayed.world.Step();
	}
	CHECK(replayed.world.StateHash() == hosted.world.StateHash());
	CHECK(replayed.requests.NextSequence(kBea) == 3);
	CHECK(replayed.Send(kBea, 1, -200).result == Result::InsufficientFunds);
	CHECK(replayed.Send(kBea, 3, 1).order == 4);
}


TEST_CASE("A request its ledger cannot have admitted stops the step")
{
	Hosted hosted;
	hosted.Populate();
	hosted.world.Commands().Push(Queued("Ada", 5, 1));
	CHECK_THROWS_AS(hosted.world.Step(), std::logic_error);
}


TEST_CASE("A saved ledger that breaks its rules does not load")
{
	Hosted hosted;
	hosted.Populate();
	const std::uint64_t before = hosted.world.StateHash();
	auto refused = [&](RequestLedger ledger)
	{
		CHECK_THROWS_AS(
		    hosted.world.Load(ForgedSave(std::move(ledger))), Jettison::Serialise::FormatError);
		CHECK(hosted.world.StateHash() == before);
	};
	refused({0, {Ledger("Ada", 1, 0)}});
	refused({0, {Ledger("Ada", 3, 1)}});
	refused({0, {Ledger("Ada", 2, 1, {Queued("Ada", 1, 1)})}});
	refused({1, {Ledger("Ada", 2, 1, {Queued("Bea", 1, 1)})}});
	refused({0, {Ledger("Ada", 1, 1), Ledger("Ada", 1, 1)}});
}


TEST_CASE("A save whose queue is not its ledger's pending requests, in order, does not load")
{
	Hosted hosted;
	hosted.Populate();
	hosted.Send(kAda, 1, 10);
	hosted.Send(kBea, 1, -20);
	std::vector<entt::meta_any> admitted = hosted.world.Commands().Drain();
	REQUIRE(admitted.size() == 2);
	auto saved = [&](std::initializer_list<std::size_t> queued)
	{
		(void)hosted.world.Commands().Drain();
		for (const std::size_t index : queued)
			hosted.world.Commands().Push(admitted[index]);
		return hosted.world.Save();
	};

	Hosted loaded;
	loaded.Populate();
	const std::uint64_t before = loaded.world.StateHash();
	for (const auto &[queued, problem] :
	     {std::pair{saved({0}), "the queue does not hold"}, std::pair{saved({1, 0}), "out of order"},
	      std::pair{saved({0, 0}), "out of order"}})
	{
		CHECK_THROWS_WITH_AS(
		    loaded.world.Load(queued), doctest::Contains(problem), Jettison::Serialise::FormatError);
		CHECK(loaded.world.StateHash() == before);
	}
	loaded.world.Load(saved({0, 1}));
	loaded.world.Step();
	CHECK(loaded.Balance() == 90);
	CHECK(loaded.requests.Outcome(kBea, 1)->request.order == 2);
}


TEST_CASE("A saved ledger whose pending requests the step could not run does not load")
{
	Hosted hosted;
	hosted.Populate();
	const std::uint64_t before = hosted.world.StateHash();
	auto granting = [](Request request)
	{
		for (const std::byte byte :
		     BinaryArchive::Write(Grant{"RegionA", 10}, FormatVersion{kGrantVersion}))
			request.payload.push_back(std::to_integer<std::uint8_t>(byte));
		return request;
	};
	// The queue in global order, but Ada's ledger holds her second request before her first.
	const Request first = granting(Queued("Ada", 1, 2));
	const Request second = granting(Queued("Ada", 2, 1));
	// A Grant whose payload is empty, in the queue and the ledger alike, and one over the limit.
	const Request empty = Queued("Ada", 1, 1);
	Request large = granting(Queued("Ada", 1, 1));
	large.payload.resize(kMaxPayloadBytes + 1);
	for (const auto &[save, problem] :
	     {std::pair{
	          ForgedSave({2, {Ledger("Ada", 3, 1, {first, second})}}, {second, first}),
	          "out of admission's order"},
	      std::pair{ForgedSave({1, {Ledger("Ada", 2, 1, {empty})}}, {empty}), "cannot read"},
	      std::pair{ForgedSave({1, {Ledger("Ada", 2, 1, {large})}}, {large}), "payload limit"},
	      std::pair{ForgedSave({0, {}}, std::vector<Request>(kQueueCapacity + 1, first)), "its limit"}})
	{
		CHECK_THROWS_WITH_AS(
		    hosted.world.Load(save), doctest::Contains(problem), Jettison::Serialise::FormatError);
		CHECK(hosted.world.StateHash() == before);
	}
}


TEST_CASE(
    "A retry is answered from the ledger after a restart registers another version of its command")
{
	Hosted hosted;
	hosted.Populate();
	REQUIRE(hosted.Send(kAda, 1, 10).status == Status::Queued);
	hosted.world.Step();
	REQUIRE(hosted.Send(kAda, 2, 20).status == Status::Queued);

	Hosted upgraded{kGrantVersion + 1};
	upgraded.world.Load(hosted.world.Save());
	const Receipt completed = upgraded.Send(kAda, 1, 10);
	CHECK(completed.status == Status::Completed);
	CHECK(completed.result == Result::Succeeded);
	CHECK(completed.order == 1);
	const Receipt pending = upgraded.Send(kAda, 2, 20);
	CHECK(pending.status == Status::Queued);
	CHECK(pending.order == 2);
	// A new request at the old version is refused, and the pending one runs as unsupported.
	CHECK(upgraded.Send(kAda, 3, 30).result == Result::UnsupportedCommand);
	upgraded.world.Step();
	CHECK(upgraded.Send(kAda, 2, 20).result == Result::UnsupportedCommand);
	CHECK(upgraded.Balance() == 110);
}

TEST_CASE("An act runs the handler at once, with no sequence, ledger or recording")
{
	Hosted hosted;
	hosted.Populate();
	hosted.world.SetRecording(true);
	CHECK(
	    hosted.requests.Act(hosted.world, kAda, "Test.Grant", kGrantVersion, Grant{"RegionA", 5}) ==
	    Result::Succeeded);
	CHECK(hosted.Balance() == 105);
	CHECK(
	    hosted.requests.Act(hosted.world, kAda, "Test.Grant", kGrantVersion, Grant{"RegionA", -500}) ==
	    Result::InsufficientFunds);
	CHECK(hosted.Balance() == 105);
	CHECK(hosted.requests.NextSequence(kAda) == 1);
	CHECK(hosted.world.Commands().Pending().empty());
	hosted.world.Step();
	CHECK(hosted.world.Recorded().empty());

	const Actor stranger{ActorKind::Principal, "Nobody"};
	CHECK(
	    hosted.requests.Act(hosted.world, stranger, "Test.Grant", kGrantVersion, Grant{"RegionA", 1}) ==
	    Result::NotAuthorised);
	CHECK(
	    hosted.requests.Act(hosted.world, kHost, "Test.Grant", kGrantVersion, Grant{"RegionA", 1}) ==
	    Result::NotAuthorised);
	CHECK(
	    hosted.requests.Act(hosted.world, kAda, "Test.Unknown", kGrantVersion, Grant{"RegionA", 1}) ==
	    Result::UnsupportedCommand);
	CHECK(
	    hosted.requests.Act(hosted.world, kAda, "Test.Grant", 2, Grant{"RegionA", 1}) ==
	    Result::UnsupportedCommand);
	const std::vector<std::byte> truncated{std::byte{1}};
	CHECK(
	    hosted.requests.Act(
	        hosted.world, kAda, std::string{"Test.Grant"}, kGrantVersion,
	        std::span<const std::byte>{truncated}) == Result::MalformedPayload);
	CHECK(hosted.Balance() == 105);
}
