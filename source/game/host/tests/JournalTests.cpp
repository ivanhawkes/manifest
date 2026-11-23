#include "TestHelpers.h"

#include <manifest/host/Journal.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <jettison/core/File.h>
#include <jettison/host/Session.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/company/Company.h>
#include <manifest/host/Start.h>
#include <manifest/host/WorldDirectory.h>

namespace
{
namespace Host = Manifest::Host;
namespace Company = Manifest::Company;
using Manifest::Command::Actor;
using Manifest::Command::ActorKind;
using Manifest::Command::Completed;
using Manifest::Command::Receipt;
using Manifest::Command::Result;

const Actor kOperator{ActorKind::Administrator, "Operator"};


// A place for a world's directory, removed with the case.
class Scratch
{
  public:
	Scratch()
	{
		static std::atomic<int> count{0};
		m_root = std::filesystem::temp_directory_path() /
		         std::format("manifest-journal-tests-{}-{}", getpid(), count++);
		std::filesystem::remove_all(m_root);
	}


	~Scratch()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_root, ignored);
	}


	Scratch(const Scratch &) = delete;


	Scratch &operator=(const Scratch &) = delete;


	std::filesystem::path Root() const
	{
		return m_root / "world";
	}


	std::filesystem::path Batch(std::uint64_t sequence) const
	{
		return Root() / "journal" / std::format("{:020}", sequence);
	}

  private:
	std::filesystem::path m_root;
};


// The trading scenario's world as the host starts it.
struct Hosted
{
	Host::Identity Identity() const
	{
		return Host::RunningIdentity(*pSession);
	}


	// The administrator's next request: a company for the principal.
	Receipt Provision(const std::string &principal)
	{
		const std::vector<std::byte> payload = Jettison::Serialise::BinaryArchive::Write(
		    Company::Provision{principal, principal, principal + " Freight"},
		    Jettison::Serialise::FormatVersion{Company::kVersion});
		return requests.Admit(
		    world, kOperator, requests.NextSequence(kOperator), std::string{Company::kProvision},
		    Company::kVersion, payload);
	}


	void Take(std::uint64_t steps)
	{
		const std::uint64_t until = world.Clock().Step() + steps;
		while (world.Clock().Step() < until)
			world.FastForward(until);
	}


	// The steps committed, then taken, as the server takes them.
	void Advance(Host::Journal &journal, std::uint64_t steps)
	{
		journal.Commit(world, steps);
		Take(steps);
	}

	std::unique_ptr<Jettison::Host::Session> pSession{
	    Host::Start({MANIFEST_MOD_DIRECTORY, {}, "trading"})};
	Jettison::Sim::SimWorld &world{pSession->World()};
	Manifest::Command::Requests &requests{Host::HostedServices(*pSession).requests};
};


// The durable writer until it is armed: the write that many calls on then fails, as a full disk
// fails it, with nothing in place but half of a partial file, or as a flush that fails does, with
// the file in place.
struct Disk
{
	Host::Journal::Write Writer()
	{
		return [this](const std::filesystem::path &path, std::span<const std::byte> bytes)
		{
			const bool armed = failIn > 0;
			if (armed)
				--failIn;
			const bool fails = armed && failIn == 0;
			if (!fails || afterWriting)
				Jettison::Core::WriteFileDurably(path, bytes);
			if (!fails)
				return;
			if (!afterWriting)
			{
				std::filesystem::create_directories(path.parent_path());
				std::ofstream partial{
				    path.string() + std::string{Jettison::Core::kPartialSuffix}, std::ios::binary};
				partial.write(
				    reinterpret_cast<const char *>(bytes.data()),
				    static_cast<std::streamsize>(bytes.size() / 2));
			}
			throw std::runtime_error{"no space left on device"};
		};
	}

	int failIn{0};
	bool afterWriting{false};
};


// A new world whose first commit, of one step and the request that provisions Ada, fails at the
// write that many on: nothing is acknowledged, and the journal takes no other commit.
void FailFirstCommit(const Scratch &scratch, int failIn, bool afterWriting)
{
	Hosted hosted;
	Disk disk;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	Host::Journal journal{directory, hosted.Identity(), disk.Writer()};
	REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
	disk.failIn = failIn;
	disk.afterWriting = afterWriting;
	CHECK_THROWS_AS(journal.Commit(hosted.world, 1), std::runtime_error);
	CHECK(journal.Head() == 0);
	CHECK_THROWS_AS(journal.Commit(hosted.world, 1), std::logic_error);
}


// What opening the world and replaying its journal into the fresh world throws, empty for nothing.
std::string Refusal(const Scratch &scratch, Hosted &hosted)
{
	try
	{
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		const Host::Journal journal{directory, hosted.Identity()};
		journal.Replay(hosted.world);
		return {};
	}
	catch (const std::runtime_error &error)
	{
		return error.what();
	}
}
} // namespace


TEST_CASE(
    "A new world is given its identity, and opens again only for the build and content that made it")
{
	const Scratch scratch;
	Hosted hosted;
	const Host::Identity running = hosted.Identity();
	CHECK(running.world.empty());
	CHECK(running.game.starts_with("0.0.1+"));
	CHECK(running.game.size() == std::string_view{"0.0.1+"}.size() + 40);
	CHECK(running.sdk.size() == 40);
	CHECK(running.fixedStep == 1.0 / 60.0);
	REQUIRE_FALSE(running.commands.empty());
	CHECK(std::ranges::is_sorted(running.commands, {}, &Manifest::Command::Registered::name));
	REQUIRE_FALSE(running.content.empty());

	std::string world;
	{
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		const Host::Journal journal{directory, running};
		world = journal.World();
		CHECK(world.size() == 32);
		CHECK(journal.Head() == 0);
	}
	{
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		CHECK(Host::Journal{directory, running}.World() == world);
	}

	const auto refused = [&](const Host::Identity &other, const std::string &naming)
	{
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		try
		{
			const Host::Journal journal{directory, other};
			FAIL("a world opened for an identity that did not make it");
		}
		catch (const std::runtime_error &error)
		{
			MANIFEST_CHECK_CONTAINS(error.what(), naming);
		}
	};
	Host::Identity other = running;
	other.game += "x";
	refused(other, "its game is " + running.game + ", and this one's is " + other.game);
	other = running;
	other.sdk = "another";
	refused(other, "its SDK is " + running.sdk);
	other = running;
	other.fixedStep = 1.0;
	refused(other, "its fixed step is");
	other = running;
	++other.commands.front().version;
	refused(other, "its command is " + running.commands.front().name + " at version");
	other = running;
	other.commands.pop_back();
	refused(other, "its command is " + running.commands.back().name);
	other = running;
	other.content.back() ^= 1u;
	refused(other, "its content is");
}


TEST_CASE("A journal replayed takes a fresh world to the state and the outcomes of the one that ran")
{
	const Scratch scratch;
	std::uint64_t step = 0;
	std::uint64_t hash = 0;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity()};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		hosted.Advance(journal, 1);
		// Steps with no request are committed as any others are, and several to a batch.
		hosted.Advance(journal, 600);
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		hosted.Advance(journal, 3);
		CHECK(journal.Head() == 3);
		step = hosted.world.Clock().Step();
		hash = hosted.world.StateHash();
		// Queued and never committed, so promised nothing: the journal's world never has it.
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		CHECK(hosted.requests.NextSequence(kOperator) == 4);
	}

	Hosted again;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	Host::Journal journal{directory, again.Identity()};
	CHECK(journal.Head() == 3);
	journal.Replay(again.world);
	CHECK(again.world.Clock().Step() == step);
	CHECK(again.world.StateHash() == hash);
	const std::optional<Completed> first = again.requests.Outcome(kOperator, 1);
	REQUIRE(first.has_value());
	CHECK(first->result == Result::Succeeded);
	CHECK(first->step == 0);
	const std::optional<Completed> second = again.requests.Outcome(kOperator, 2);
	REQUIRE(second.has_value());
	CHECK(second->result == Result::AlreadyProvisioned);
	CHECK(second->step == 601);
	CHECK(again.requests.NextSequence(kOperator) == 3);

	// And goes on from where the journal ends, and nowhere else.
	CHECK_THROWS_AS(journal.Commit(again.world, 0), std::logic_error);
	again.Advance(journal, 1);
	CHECK(journal.Head() == 4);
	again.Take(1);
	CHECK_THROWS_AS(journal.Commit(again.world, 1), std::logic_error);
}


TEST_CASE("A batch that could not be written is not committed, and its request was never run")
{
	const Scratch scratch;
	FailFirstCommit(scratch, 1, false);
	CHECK_FALSE(std::filesystem::exists(scratch.Batch(1)));

	Hosted again;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	Host::Journal journal{directory, again.Identity()};
	CHECK(journal.Head() == 0);
	journal.Replay(again.world);
	CHECK(again.world.Clock().Step() == 0);
	CHECK(again.requests.NextSequence(kOperator) == 1);
	// The half-written file beside it is in no one's way.
	again.Advance(journal, 1);
	CHECK(journal.Head() == 1);
}


TEST_CASE("A batch the catalogue was never made to name is an orphan: not run, and written over")
{
	const Scratch scratch;
	FailFirstCommit(scratch, 2, false);
	CHECK(std::filesystem::exists(scratch.Batch(1)));
	{
		Hosted again;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, again.Identity()};
		CHECK(journal.Head() == 0);
		journal.Replay(again.world);
		CHECK(again.world.Clock().Step() == 0);
		CHECK(again.requests.NextSequence(kOperator) == 1);
		again.Advance(journal, 2);
	}
	Hosted last;
	CHECK(Refusal(scratch, last).empty());
	CHECK(last.world.Clock().Step() == 2);
	CHECK(last.requests.NextSequence(kOperator) == 1);
}


TEST_CASE("A commit that failed with its catalogue in place is committed: its step is taken on opening")
{
	const Scratch scratch;
	FailFirstCommit(scratch, 2, true);

	Hosted again;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	const Host::Journal journal{directory, again.Identity()};
	CHECK(journal.Head() == 1);
	journal.Replay(again.world);
	CHECK(again.world.Clock().Step() == 1);
	const std::optional<Completed> outcome = again.requests.Outcome(kOperator, 1);
	REQUIRE(outcome.has_value());
	CHECK(outcome->result == Result::Succeeded);
	CHECK(outcome->step == 0);
}


TEST_CASE(
    "A committed batch missing, damaged or out of its chain, or a catalogue so, stops the opening")
{
	const Scratch scratch;
	const Scratch elsewhere;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity()};
		hosted.Advance(journal, 1);
		hosted.Advance(journal, 1);
		const Host::WorldDirectory other = Host::OpenWorldDirectory(elsewhere.Root());
		const Host::Journal another{other, hosted.Identity()};
	}
	const auto bytes = [](const std::filesystem::path &path)
	{
		std::ifstream in{path, std::ios::binary};
		return std::string{std::istreambuf_iterator<char>{in}, {}};
	};
	const auto put = [](const std::filesystem::path &path, const std::string &contents)
	{ std::ofstream{path, std::ios::binary | std::ios::trunc} << contents; };

	// Each fault is in the first batch or before it, so the world it is replayed into is still new.
	Hosted fresh;
	const std::string first = bytes(scratch.Batch(1));
	std::string damaged = first;
	damaged[damaged.size() / 2] ^= 1;
	put(scratch.Batch(1), damaged);
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), scratch.Batch(1).string() + " is damaged");
	put(scratch.Batch(1), first.substr(0, first.size() - 1));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), scratch.Batch(1).string() + " is damaged");
	put(scratch.Batch(1), "");
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), "no such file is 0 bytes long");
	put(scratch.Batch(1), bytes(scratch.Batch(2)));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), "the chain is broken");
	std::filesystem::remove(scratch.Batch(1));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), scratch.Batch(1).string() + " is missing");
	put(scratch.Batch(1), first);

	const std::filesystem::path catalogue = scratch.Root() / "catalogue";
	const std::string committed = bytes(catalogue);
	put(catalogue, committed.substr(0, committed.size() / 2));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), catalogue.string() + " is damaged");
	put(catalogue, bytes(elsewhere.Root() / "catalogue"));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), "is the catalogue of another world");
	// A world with no catalogue is not made again over its journal.
	std::filesystem::remove(catalogue);
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), catalogue.string() + " is missing");
	CHECK(fresh.world.Clock().Step() == 0);

	put(catalogue, committed);
	CHECK(Refusal(scratch, fresh).empty());
	CHECK(fresh.world.Clock().Step() == 2);
}
