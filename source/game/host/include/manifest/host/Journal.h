#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/command/Request.h>
#include <manifest/host/WorldDirectory.h>

namespace Manifest::Host
{
// What a world is held to for as long as it lasts (D-111): a build opens only a world its own
// identity made. Kept whole in the world's directory, once, as the world is made; a batch names it
// by the world.
struct Identity
{
	// 128 bits from the operating system as the world is made, in hex: no part of the state or its
	// hash. Empty in a running build's own.
	std::string world;
	// The project's version and the tree the build was made from, then the SDK's tree: git's hash of
	// each checkout's build inputs as they were on disk (cmake/Revision.cmake), so uncommitted
	// changes are another build.
	std::string game;
	std::string sdk;
	double fixedStep{0.0};
	// In name order.
	std::vector<Command::Registered> commands;
	// The canonical content manifest (Content::ContentIdentity::manifest).
	std::vector<std::uint8_t> content;
};

// The identity of the session's build and content, with no world.
Identity RunningIdentity(Jettison::Host::Session &session);

// How often a journal writes, in steps: the first is the most a crash loses, the second the most a
// start replays. The defaults are 30 s and an hour of the host's 1/60 s steps (D-113).
struct Cadence
{
	std::uint64_t flushSteps{1'800};
	std::uint64_t snapshotSteps{216'000};
};

// A request and the step it ran at the start of.
struct Ran
{
	std::uint64_t step{0};
	Command::Request request;
};

// A snapshot the catalogue keeps: the batch it follows, with that batch's identity, the checksum of
// the snapshot's file and the step the world was at.
struct Kept
{
	std::uint64_t batch{0};
	std::uint64_t batchId{0};
	std::uint64_t snapshot{0};
	std::uint64_t step{0};
};

// What the catalogue holds: the head of the world's journal, the step after that batch's last, and
// the snapshots kept, oldest first.
struct Catalogue
{
	std::string world;
	std::uint64_t head{0};
	std::uint64_t headId{0};
	std::uint64_t step{0};
	std::vector<Kept> snapshots;
};

// A world's journal, snapshots and catalogue (docs/stage-5-recovery-plan.md, "The journal and its
// snapshots"). The world steps and answers its requests at once; every Cadence::flushSteps the steps
// taken since the last flush, and the requests that ran in them, are written as one immutable batch
// file and the catalogue replaced to name it as the head. So a crash loses what ran since the last
// flush and nothing before it, and a clean stop, which flushes, loses nothing. Every
// Cadence::snapshotSteps the world's save is written beside them, the last two kept with every batch
// since the older, and what is older than that removed. Every file is followed by its 64-bit
// FNV-1a, which is also a batch's identity in the chain.
class Journal
{
  public:
	// How a file under the root is written: Jettison::Core::WriteFileDurably unless a test gives one
	// that fails where a disk would.
	using Write = std::function<void(const std::filesystem::path &, std::span<const std::byte>)>;

	// Opens the journal of the world in the directory, which must outlive this. One holding neither
	// an identity, a catalogue nor a journal is a new world: its identity is made and written, and a
	// catalogue with no head. Throws std::runtime_error naming the file for one that is missing,
	// damaged or another world's, and naming the field and both values for a world another build or
	// other content made.
	Journal(const WorldDirectory &directory, Identity running, Cadence cadence = {}, Write write = {});

	const std::string &World() const
	{
		return m_catalogue.world;
	}

	// The sequence of the last batch flushed, from 1; 0 for none.
	std::uint64_t Head() const
	{
		return m_catalogue.head;
	}

	// Brings a world as Host::Start left it to the journal's last flushed step: the newest snapshot
	// that loads, or the one before it, or the world's own start while every batch is still kept,
	// then each batch after it, its requests run at the steps they first ran at. The request ledger
	// is the snapshot's and the replay's, so a retry is answered as before the stop. Throws
	// std::runtime_error naming the file when no snapshot loads, or for a batch that is missing,
	// damaged or not the next in the chain; the world is then part-way and is discarded.
	void Recover(Jettison::Sim::SimWorld &world) const;

	// Takes the world's next steps, the requests queued on it running at the start of the first, and
	// writes a batch or a snapshot when one is due. Nothing but requests may be queued. A write that
	// fails throws what the writer threw and leaves the files uncertain, since the catalogue may
	// already name what was written: the journal then refuses every later call with
	// std::logic_error, and the next opening finds which it was. Throws std::logic_error too for a
	// world stepped other than here since it was recovered.
	void Advance(Jettison::Sim::SimWorld &world, std::uint64_t steps);

	// Writes the steps taken since the last flush, for a clean stop; nothing when there are none.
	// Throws as Advance does.
	void Flush();

  private:
	void Snapshot(Jettison::Sim::SimWorld &world);
	void Publish(Catalogue next, const std::filesystem::path &path, std::vector<std::byte> file);
	void Prune() const;

	std::filesystem::path m_root;
	Write m_write;
	Cadence m_cadence;
	Catalogue m_catalogue;
	// The requests that have run since the last flush, in the order they ran.
	std::vector<Ran> m_ran;
	// The step the world is at, as its advances here have left it.
	std::uint64_t m_at{0};
	bool m_uncertain{false};
};
} // namespace Manifest::Host
