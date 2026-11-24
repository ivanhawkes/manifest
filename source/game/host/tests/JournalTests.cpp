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


	// Beside the world, not in it.
	std::filesystem::path Elsewhere(std::string_view name) const
	{
		return m_root / name;
	}


	std::filesystem::path Batch(std::uint64_t sequence) const
	{
		return Root() / "journal" / std::format("{:020}", sequence);
	}


	// The snapshot taken after the batch.
	std::filesystem::path Snapshot(std::uint64_t batch) const
	{
		return Root() / "snapshots" / std::format("{:020}", batch);
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


	// The administrator's request, its next unless a sequence is given: a company for the principal.
	Receipt Provision(const std::string &principal, std::uint64_t sequence = 0)
	{
		const std::vector<std::byte> payload = Jettison::Serialise::BinaryArchive::Write(
		    Company::Provision{principal, principal, principal + " Freight"},
		    Jettison::Serialise::FormatVersion{Company::kVersion});
		return requests.Admit(
		    world, kOperator, sequence == 0 ? requests.NextSequence(kOperator) : sequence,
		    std::string{Company::kProvision}, Company::kVersion, payload);
	}


	void Take(std::uint64_t steps)
	{
		const std::uint64_t until = world.Clock().Step() + steps;
		while (world.Clock().Step() < until)
			world.FastForward(until);
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


// A new world whose first flush, of one step and the request that provisions Ada, fails at the write
// that many on: the journal then takes nothing more.
void FailFirstFlush(const Scratch &scratch, int failIn, bool afterWriting)
{
	Hosted hosted;
	Disk disk;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	Host::Journal journal{directory, hosted.Identity(), {}, disk.Writer()};
	REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
	journal.Advance(hosted.world, 1);
	disk.failIn = failIn;
	disk.afterWriting = afterWriting;
	CHECK_THROWS_AS(journal.Flush(), std::runtime_error);
	CHECK(journal.Head() == 0);
	CHECK_THROWS_AS(journal.Flush(), std::logic_error);
	CHECK_THROWS_AS(journal.Advance(hosted.world, 1), std::logic_error);
}


// What opening the world and recovering it into the fresh world throws, empty for nothing.
std::string Refusal(const Scratch &scratch, Hosted &hosted)
{
	try
	{
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		const Host::Journal journal{directory, hosted.Identity()};
		journal.Recover(hosted.world);
		return {};
	}
	catch (const std::runtime_error &error)
	{
		return error.what();
	}
}


std::string Bytes(const std::filesystem::path &path)
{
	std::ifstream in{path, std::ios::binary};
	return std::string{std::istreambuf_iterator<char>{in}, {}};
}


void Put(const std::filesystem::path &path, const std::string &contents)
{
	std::ofstream{path, std::ios::binary | std::ios::trunc} << contents;
}


void Damage(const std::filesystem::path &path)
{
	std::string contents = Bytes(path);
	contents[contents.size() / 2] ^= 1;
	Put(path, contents);
}


std::filesystem::path Backup(
    const std::filesystem::path &backups, const std::string &world, std::uint64_t batch)
{
	return backups / std::format("{}.{:020}", world, batch);
}


// What restoring the backup into the directory throws, empty for nothing.
std::string RestoreRefusal(
    const std::filesystem::path &backup, const std::filesystem::path &into, Hosted &hosted,
    const Host::Identity &running)
{
	try
	{
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(into);
		(void)Host::Restore(backup, directory, running, hosted.world);
		return {};
	}
	catch (const std::runtime_error &error)
	{
		return error.what();
	}
}


// Whether the directory holds nothing but its lock.
bool Empty(const std::filesystem::path &directory)
{
	return std::ranges::all_of(
	    std::filesystem::directory_iterator{directory},
	    [](const std::filesystem::directory_entry &entry)
	    { return entry.path().filename() == Host::kLockName; });
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

	// Content patched by an overlay is named by its first definition that differs.
	const std::unique_ptr<Jettison::Host::Session> pPatched{Host::Start(
	    {MANIFEST_MOD_DIRECTORY,
	     {std::filesystem::path{MANIFEST_HOST_OVERLAYS} / "copper"},
	     "trading"})};
	refused(
	    Host::RunningIdentity(*pPatched),
	    "; the first definition that differs is Manifest.Item item-copper");
}


TEST_CASE("A world recovered is the one that ran as its last flush left it, and what ran after is lost")
{
	const Scratch scratch;
	// A flush every hundred steps, and no snapshot.
	const Host::Cadence cadence{100, 1'000'000};
	std::uint64_t step = 0;
	std::uint64_t hash = 0;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), cadence};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Advance(hosted.world, 1);
		CHECK(journal.Head() == 0);
		// Steps with no request are the journal's as any others are, flushed every hundred.
		journal.Advance(hosted.world, 600);
		CHECK(journal.Head() == 6);
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Advance(hosted.world, 3);
		journal.Flush();
		CHECK(journal.Head() == 7);
		journal.Flush();
		CHECK(journal.Head() == 7);
		step = hosted.world.Clock().Step();
		hash = hosted.world.StateHash();
		// Run, answered, and never flushed: a crash now loses it.
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Advance(hosted.world, 5);
		CHECK(hosted.requests.Outcome(kOperator, 3).has_value());
		CHECK(journal.Head() == 7);
	}

	Hosted again;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	Host::Journal journal{directory, again.Identity(), cadence};
	CHECK(journal.Head() == 7);
	journal.Recover(again.world);
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
	// The lost request's sequence is free again, and a retry of one that was kept is answered as
	// it was, not run again.
	CHECK(again.requests.NextSequence(kOperator) == 3);
	const Receipt retried = again.Provision("Ada", 1);
	CHECK(retried.status == Receipt::Status::Completed);
	CHECK(retried.result == Result::Succeeded);

	// And goes on from there, in a world stepped nowhere else.
	journal.Advance(again.world, 1);
	again.Take(1);
	CHECK_THROWS_AS(journal.Advance(again.world, 1), std::logic_error);
}


TEST_CASE(
    "Snapshots are kept two deep with the batches since the older, and a start loads the newest that "
    "reads")
{
	const Scratch scratch;
	// A flush every ten steps and a snapshot every forty.
	const Host::Cadence cadence{10, 40};
	std::uint64_t hash = 0;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), cadence};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		for (int advance = 0; advance < 26; ++advance)
			journal.Advance(hosted.world, 5);
		CHECK(journal.Head() == 13);
		hash = hosted.world.StateHash();
	}
	// Snapshots after batches 4, 8 and 12: the last two are kept, with every batch after the older.
	CHECK_FALSE(std::filesystem::exists(scratch.Snapshot(4)));
	CHECK(std::filesystem::exists(scratch.Snapshot(8)));
	CHECK(std::filesystem::exists(scratch.Snapshot(12)));
	CHECK_FALSE(std::filesystem::exists(scratch.Batch(8)));
	CHECK(std::filesystem::exists(scratch.Batch(9)));
	CHECK(std::filesystem::exists(scratch.Batch(13)));

	const auto recovers = [&]
	{
		Hosted again;
		CHECK(Refusal(scratch, again).empty());
		CHECK(again.world.Clock().Step() == 130);
		CHECK(again.world.StateHash() == hash);
		const std::optional<Completed> outcome = again.requests.Outcome(kOperator, 1);
		REQUIRE(outcome.has_value());
		CHECK(outcome->result == Result::Succeeded);
	};
	recovers();
	// The newest does not read: the one before it and the batches since reach the same world.
	Damage(scratch.Snapshot(12));
	recovers();
	// Neither reads, and the batches before them are gone: there is nowhere to start from.
	std::filesystem::remove(scratch.Snapshot(8));
	Hosted last;
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, last), scratch.Snapshot(8).string() + " is missing");
}


TEST_CASE(
    "One long advance flushes and snapshots at each step a short one would, and reaches its world")
{
	std::uint64_t stepped = 0;
	{
		const Scratch scratch;
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), {10, 40}};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		for (int advance = 0; advance < 13; ++advance)
			journal.Advance(hosted.world, 10);
		stepped = hosted.world.StateHash();
	}
	const Scratch scratch;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), {10, 40}};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Advance(hosted.world, 130);
		CHECK(journal.Head() == 13);
		CHECK(hosted.world.StateHash() == stepped);
	}
	CHECK(std::filesystem::exists(scratch.Snapshot(8)));
	CHECK(std::filesystem::exists(scratch.Snapshot(12)));
	CHECK(std::filesystem::exists(scratch.Batch(9)));
	Hosted again;
	CHECK(Refusal(scratch, again).empty());
	CHECK(again.world.StateHash() == stepped);
}


TEST_CASE("A world with one snapshot, which does not read, is recovered from its start")
{
	const Scratch scratch;
	std::uint64_t hash = 0;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), {10, 40}};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Advance(hosted.world, 50);
		CHECK(journal.Head() == 5);
		hash = hosted.world.StateHash();
	}
	REQUIRE(std::filesystem::exists(scratch.Snapshot(4)));
	Damage(scratch.Snapshot(4));
	Hosted again;
	CHECK(Refusal(scratch, again).empty());
	CHECK(again.world.Clock().Step() == 50);
	CHECK(again.world.StateHash() == hash);
}


TEST_CASE("A batch that could not be written is not in the journal, and the world starts without it")
{
	const Scratch scratch;
	FailFirstFlush(scratch, 1, false);
	CHECK_FALSE(std::filesystem::exists(scratch.Batch(1)));

	Hosted again;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	Host::Journal journal{directory, again.Identity()};
	CHECK(journal.Head() == 0);
	journal.Recover(again.world);
	CHECK(again.world.Clock().Step() == 0);
	CHECK(again.requests.NextSequence(kOperator) == 1);
	// The half-written file beside it is in no one's way.
	journal.Advance(again.world, 1);
	journal.Flush();
	CHECK(journal.Head() == 1);
}


TEST_CASE("A batch the catalogue was never made to name is an orphan: not run, and written over")
{
	const Scratch scratch;
	FailFirstFlush(scratch, 2, false);
	CHECK(std::filesystem::exists(scratch.Batch(1)));
	{
		Hosted again;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, again.Identity()};
		CHECK(journal.Head() == 0);
		journal.Recover(again.world);
		CHECK(again.world.Clock().Step() == 0);
		CHECK(again.requests.NextSequence(kOperator) == 1);
		journal.Advance(again.world, 2);
		journal.Flush();
	}
	Hosted last;
	CHECK(Refusal(scratch, last).empty());
	CHECK(last.world.Clock().Step() == 2);
	CHECK(last.requests.NextSequence(kOperator) == 1);
}


TEST_CASE(
    "A flush that failed with its catalogue in place was made: a start has its step and its request")
{
	const Scratch scratch;
	FailFirstFlush(scratch, 2, true);

	Hosted again;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	const Host::Journal journal{directory, again.Identity()};
	CHECK(journal.Head() == 1);
	journal.Recover(again.world);
	CHECK(again.world.Clock().Step() == 1);
	const std::optional<Completed> outcome = again.requests.Outcome(kOperator, 1);
	REQUIRE(outcome.has_value());
	CHECK(outcome->result == Result::Succeeded);
	CHECK(outcome->step == 0);
}


TEST_CASE("A batch missing, damaged or out of its chain, or a catalogue so, stops the start")
{
	const Scratch scratch;
	const Scratch elsewhere;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity()};
		journal.Advance(hosted.world, 1);
		journal.Flush();
		journal.Advance(hosted.world, 1);
		journal.Flush();
		const Host::WorldDirectory other = Host::OpenWorldDirectory(elsewhere.Root());
		const Host::Journal another{other, hosted.Identity()};
	}

	// Each fault is in the first batch or before it, so the world it is recovered into is still new.
	Hosted fresh;
	const std::string first = Bytes(scratch.Batch(1));
	Damage(scratch.Batch(1));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), scratch.Batch(1).string() + " is damaged");
	Put(scratch.Batch(1), first.substr(0, first.size() - 1));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), scratch.Batch(1).string() + " is damaged");
	Put(scratch.Batch(1), "");
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), "no such file is 0 bytes long");
	Put(scratch.Batch(1), Bytes(scratch.Batch(2)));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), "the chain is broken");
	std::filesystem::remove(scratch.Batch(1));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), scratch.Batch(1).string() + " is missing");
	Put(scratch.Batch(1), first);

	const std::filesystem::path catalogue = scratch.Root() / "catalogue";
	const std::string named = Bytes(catalogue);
	Put(catalogue, named.substr(0, named.size() / 2));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), catalogue.string() + " is damaged");
	Put(catalogue, Bytes(elsewhere.Root() / "catalogue"));
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), "is the catalogue of another world");
	// A world with no catalogue is not made again over its journal.
	std::filesystem::remove(catalogue);
	MANIFEST_CHECK_CONTAINS(Refusal(scratch, fresh), catalogue.string() + " is missing");
	CHECK(fresh.world.Clock().Step() == 0);

	Put(catalogue, named);
	CHECK(Refusal(scratch, fresh).empty());
	CHECK(fresh.world.Clock().Step() == 2);
}


TEST_CASE(
    "A clean stop runs what was queued in one last step and leaves a snapshot a start replays none "
    "after")
{
	const Scratch scratch;
	std::uint64_t hash = 0;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity()};
		journal.Advance(hosted.world, 10);
		// Admitted, and the server stopped before its step.
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Stop(hosted.world);
		CHECK(hosted.world.Clock().Step() == 11);
		const std::optional<Completed> outcome = hosted.requests.Outcome(kOperator, 1);
		REQUIRE(outcome.has_value());
		CHECK(outcome->result == Result::Succeeded);
		CHECK(outcome->step == 10);
		CHECK(journal.Head() == 1);
		CHECK(std::filesystem::exists(scratch.Snapshot(1)));
		hash = hosted.world.StateHash();
		// Stopped again with nothing since: nothing is written.
		const std::string catalogue = Bytes(scratch.Root() / "catalogue");
		journal.Stop(hosted.world);
		CHECK(Bytes(scratch.Root() / "catalogue") == catalogue);
	}
	// The start needs no batch: it loads the snapshot, which is of the last step.
	std::filesystem::remove(scratch.Batch(1));
	Hosted again;
	CHECK(Refusal(scratch, again).empty());
	CHECK(again.world.Clock().Step() == 11);
	CHECK(again.world.StateHash() == hash);
	CHECK(again.Provision("Ada", 1).status == Receipt::Status::Completed);
}


TEST_CASE("A backup is written with each snapshot, the newest kept, and one cut off is none and goes")
{
	const Scratch scratch;
	const std::filesystem::path backups = scratch.Elsewhere("backups");
	Hosted hosted;
	Disk disk;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	// A flush every ten steps, a snapshot every forty and two backups kept.
	Host::Journal journal{directory, hosted.Identity(), {10, 40}, disk.Writer(), {backups, 2}};
	const std::string world = journal.World();
	REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
	for (int advance = 0; advance < 26; ++advance)
		journal.Advance(hosted.world, 5);
	// Snapshots after batches 4, 8 and 12, and the last two backed up.
	CHECK_FALSE(std::filesystem::exists(Backup(backups, world, 4)));
	CHECK(std::filesystem::exists(Backup(backups, world, 8) / "catalogue"));
	CHECK(std::filesystem::exists(Backup(backups, world, 12) / "catalogue"));

	// The next, after batch 16: the batch, the catalogue, the snapshot and the catalogue again, then
	// the backup's identity, its snapshot and, failing, its catalogue. The world goes on.
	for (int advance = 0; advance < 5; ++advance)
		journal.Advance(hosted.world, 5);
	disk.failIn = 7;
	journal.Advance(hosted.world, 5);
	CHECK(journal.Head() == 16);
	CHECK(std::filesystem::exists(scratch.Snapshot(16)));
	CHECK(std::filesystem::exists(Backup(backups, world, 16) / "identity"));
	CHECK_FALSE(std::filesystem::exists(Backup(backups, world, 16) / "catalogue"));
	CHECK(std::filesystem::exists(Backup(backups, world, 8)));

	// The one after: the one cut off goes, and the oldest.
	for (int advance = 0; advance < 8; ++advance)
		journal.Advance(hosted.world, 5);
	CHECK(journal.Head() == 20);
	CHECK_FALSE(std::filesystem::exists(Backup(backups, world, 8)));
	CHECK(std::filesystem::exists(Backup(backups, world, 12) / "catalogue"));
	CHECK_FALSE(std::filesystem::exists(Backup(backups, world, 16)));
	CHECK(std::filesystem::exists(Backup(backups, world, 20) / "catalogue"));
}


TEST_CASE(
    "A snapshot cut off, or written and never named, is passed over for the one before it and its "
    "batches")
{
	// The second snapshot's writes: its world's last batch, the catalogue naming it, the snapshot,
	// then the catalogue naming that. The third and fourth fail, the fourth after it landed too.
	struct Interrupted
	{
		int failIn;
		bool afterWriting;
	};
	for (const Interrupted interrupted :
	     {Interrupted{3, false}, Interrupted{4, false}, Interrupted{4, true}})
	{
		CAPTURE(interrupted.failIn);
		CAPTURE(interrupted.afterWriting);
		const Scratch scratch;
		std::uint64_t hash = 0;
		{
			Hosted hosted;
			Disk disk;
			const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
			Host::Journal journal{directory, hosted.Identity(), {40, 40}, disk.Writer()};
			REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
			journal.Advance(hosted.world, 40);
			REQUIRE(std::filesystem::exists(scratch.Snapshot(1)));
			disk.failIn = interrupted.failIn;
			disk.afterWriting = interrupted.afterWriting;
			CHECK_THROWS_AS(journal.Advance(hosted.world, 40), std::runtime_error);
			CHECK(journal.Head() == 2);
			hash = hosted.world.StateHash();
		}
		// Nothing a start needs was removed: the older snapshot and the batch after it.
		CHECK(std::filesystem::exists(scratch.Snapshot(1)));
		CHECK(std::filesystem::exists(scratch.Batch(2)));
		CHECK(std::filesystem::exists(scratch.Snapshot(2)) == (interrupted.failIn == 4));
		{
			Hosted again;
			const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
			Host::Journal journal{directory, again.Identity()};
			journal.Recover(again.world);
			CHECK(again.world.Clock().Step() == 80);
			CHECK(again.world.StateHash() == hash);
			// A stop writes the snapshot again, over one never named.
			journal.Stop(again.world);
		}
		Hosted last;
		CHECK(Refusal(scratch, last).empty());
		CHECK(last.world.StateHash() == hash);
	}
}


TEST_CASE("A backup restores into an empty directory, to its head, and is only read")
{
	const Scratch scratch;
	const std::filesystem::path backups = scratch.Elsewhere("backups");
	std::filesystem::path backup;
	std::uint64_t step = 0;
	std::uint64_t hash = 0;
	std::uint64_t head = 0;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), {10, 1'000'000}, {}, {backups}};
		REQUIRE(hosted.Provision("Ada").status == Receipt::Status::Queued);
		journal.Advance(hosted.world, 25);
		REQUIRE(hosted.Provision("Bea").status == Receipt::Status::Queued);
		journal.Stop(hosted.world);
		step = hosted.world.Clock().Step();
		hash = hosted.world.StateHash();
		head = journal.Head();
		backup = Backup(backups, journal.World(), head);
	}
	const std::string catalogue = Bytes(backup / "catalogue");
	const std::string snapshot = Bytes(backup / "snapshots" / std::format("{:020}", head));
	{
		Hosted again;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Elsewhere("restored"));
		Host::Journal journal = Host::Restore(backup, directory, again.Identity(), again.world);
		CHECK(journal.Head() == head);
		CHECK(again.world.Clock().Step() == step);
		CHECK(again.world.StateHash() == hash);
		// The retry history with it: a retry answered as before, not run again.
		CHECK(again.Provision("Ada", 1).status == Receipt::Status::Completed);
		CHECK(again.requests.NextSequence(kOperator) == 3);
		// And served on from there, and stopped.
		journal.Advance(again.world, 1);
		journal.Stop(again.world);
	}
	CHECK(Bytes(backup / "catalogue") == catalogue);
	CHECK(Bytes(backup / "snapshots" / std::format("{:020}", head)) == snapshot);
	// The restored world is one a server opens.
	{
		Hosted served;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Elsewhere("restored"));
		const Host::Journal journal{directory, served.Identity()};
		journal.Recover(served.world);
		CHECK(served.world.Clock().Step() == step + 1);
	}

	Hosted fresh;
	MANIFEST_CHECK_CONTAINS(
	    RestoreRefusal(backup, scratch.Root(), fresh, fresh.Identity()),
	    "is not empty: a backup is restored only into an empty directory");
	const std::filesystem::path into = scratch.Elsewhere("refused");
	Host::Identity other = fresh.Identity();
	other.game += "x";
	MANIFEST_CHECK_CONTAINS(
	    RestoreRefusal(backup, into, fresh, other), "was made by another build or other content");
	CHECK(Empty(into));
	// Copied, then found damaged as it is recovered: what was written goes again.
	Damage(backup / "snapshots" / std::format("{:020}", head));
	MANIFEST_CHECK_CONTAINS(RestoreRefusal(backup, into, fresh, fresh.Identity()), "is damaged");
	CHECK(Empty(into));
	CHECK(fresh.world.Clock().Step() == 0);
	// Found damaged before anything is written: into a directory that takes no file, the refusal is
	// the damage and not the write.
	std::filesystem::permissions(
	    into, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
	MANIFEST_CHECK_CONTAINS(RestoreRefusal(backup, into, fresh, fresh.Identity()), "is damaged");
	std::filesystem::permissions(into, std::filesystem::perms::owner_all);
}


TEST_CASE("A save larger than a snapshot may be is not written, and the journal keeps the world")
{
	const Scratch scratch;
	{
		Hosted hosted;
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
		Host::Journal journal{directory, hosted.Identity(), {10, 40}, {}, {{}, 1, 1024}};
		journal.Advance(hosted.world, 30);
		try
		{
			journal.Advance(hosted.world, 10);
			FAIL("a snapshot larger than the storage allows was written");
		}
		catch (const std::runtime_error &error)
		{
			MANIFEST_CHECK_CONTAINS(error.what(), "more than a snapshot may be (1024 bytes");
		}
		CHECK(journal.Head() == 4);
		CHECK_FALSE(std::filesystem::exists(scratch.Snapshot(4)));
	}
	Hosted again;
	CHECK(Refusal(scratch, again).empty());
	CHECK(again.world.Clock().Step() == 40);
}


TEST_CASE("Backups within the world's directory are refused before the world is made")
{
	const Scratch scratch;
	Hosted hosted;
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	for (const std::filesystem::path &backups :
	     {scratch.Root() / "backups", std::filesystem::path{scratch.Root().string() + "/"}})
	{
		try
		{
			const Host::Journal journal{directory, hosted.Identity(), {}, {}, {backups}};
			FAIL("backups within the world were taken");
		}
		catch (const std::runtime_error &error)
		{
			MANIFEST_CHECK_CONTAINS(error.what(), "are within the world at");
		}
	}
	CHECK(Empty(scratch.Root()));
}


TEST_CASE(
    "A world within its backups is refused, and backups led into the world by a symlink after "
    "a directory made on the way")
{
	const Scratch scratch;
	Hosted hosted;
	{
		// A backup named as this world's directory is would be removed and written over it.
		const std::filesystem::path backups = scratch.Elsewhere("backups");
		const Host::WorldDirectory directory = Host::OpenWorldDirectory(backups / "world.1");
		CHECK_THROWS_WITH_AS(
		    (Host::Journal{directory, hosted.Identity(), {}, {}, {backups}}),
		    doctest::Contains("is within the backups at"), std::runtime_error);
	}
	// "missing" is not there to be resolved until it is made, and the link after it is the world.
	const Host::WorldDirectory directory = Host::OpenWorldDirectory(scratch.Root());
	std::filesystem::create_directory_symlink(scratch.Root(), scratch.Elsewhere("link"));
	const std::filesystem::path backups = scratch.Elsewhere("missing") / ".." / "link" / "backups";
	CHECK_THROWS_WITH_AS(
	    (Host::Journal{directory, hosted.Identity(), {}, {}, {backups}}),
	    doctest::Contains("are within the world at"), std::runtime_error);
	CHECK_FALSE(std::filesystem::exists(scratch.Root() / "identity"));
}
