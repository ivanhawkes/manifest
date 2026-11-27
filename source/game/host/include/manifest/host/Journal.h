#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/script/ScriptHost.h>
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

// The largest file of a snapshot a start reads: a world of 256 players was guessed at 110 MiB
// (2026-11-24).
inline constexpr std::uintmax_t kMaxSnapshotBytes = std::uintmax_t{4} * 1024 * 1024 * 1024;

// Where a journal keeps its backups, and what a world may store (docs/persistence.md, "Backups" and
// "What a world stores").
struct Storage
{
	// None when empty. Otherwise a backup is written here with every snapshot, as
	// `<world>.<batch>`, and the newest `kept` of the world's are kept: outside the world's
	// directory, for a disk that is not the world's.
	std::filesystem::path backups;
	std::size_t kept{24};

	// The largest save a snapshot may be: a larger one is refused as it is written, since a start
	// would refuse it as it is read.
	std::uintmax_t snapshotBytes{kMaxSnapshotBytes};

	// The mods' scripts, which the session owns and which outlive the journal: what they keep by key
	// is written into each snapshot (Jettison::Script::kStoreSection) and given back to them as a
	// world is recovered. None for a world whose scripts keep nothing.
	Jettison::Script::ScriptHost *pScripts{nullptr};
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

// A world's journal, snapshots and catalogue (docs/persistence.md, "The journal and its batches"). The
// world steps and answers its requests at once; every Cadence::flushSteps the steps taken since the
// last flush, and the requests that ran in them, are written as one immutable batch file and the
// catalogue replaced to name it as the head. So a crash loses what ran since the last flush and nothing
// before it, and a clean stop, which flushes, loses nothing. Every Cadence::snapshotSteps the world's
// save is written beside them (a Jettison::Sim::SaveGame, with the scripts' keys when the storage
// names the scripts), the last two kept with every batch since the older, and what is older
// than that removed. Every file is followed by its 64-bit FNV-1a, which is also a batch's identity in
// the chain. Each snapshot is also a backup, when the storage names a directory for them: the world's
// identity, the snapshot and a catalogue naming it alone, the catalogue written last, so a backup cut
// off before it is none and is removed by the next.
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
	// other content made, or backups within the world's directory.
	Journal(
	    const WorldDirectory &directory, Identity running, Cadence cadence = {}, Write write = {},
	    Storage storage = {});

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
	// is the snapshot's and the replay's, so a retry is answered as before the stop; the scripts'
	// keys, for a storage that names the scripts, are the snapshot's, none from the world's start. Throws
	// std::runtime_error naming the file when no snapshot loads, or for a batch that is missing,
	// damaged or not the next in the chain; the world is then part-way and is discarded.
	void Recover(Jettison::Sim::SimWorld &world) const;

	// Takes the world's next steps, the requests queued on it running at the start of the first, and
	// writes each batch and snapshot due on the way at the step it is due, as shorter advances would.
	// Nothing but requests may be queued. A write that fails throws what the writer threw and leaves
	// the files uncertain, since the catalogue may already name what was written: the journal then
	// refuses every later call with std::logic_error, and the next opening finds which it was. Throws
	// std::logic_error too for a world stepped other than here since it was recovered.
	void Advance(Jettison::Sim::SimWorld &world, std::uint64_t steps);

	// Writes the steps taken since the last flush; nothing when there are none. Throws as Advance
	// does.
	void Flush();

	// For a clean stop, once nothing more is admitted: runs the requests queued since the last step in
	// one more, so none is left unanswered, then writes a snapshot, and its backup, unless the newest
	// is of this step, so a start loads it and replays nothing. Throws as Advance does, and
	// std::runtime_error for a save larger than the storage allows, which is not written.
	void Stop(Jettison::Sim::SimWorld &world);

  private:
	void Snapshot(Jettison::Sim::SimWorld &world);

	void Expect(const Jettison::Sim::SimWorld &world) const;

	void Publish(Catalogue next, const std::filesystem::path &path, std::span<const std::byte> file);

	void Prune() const;

	void Backup(std::span<const std::byte> snapshot) const;

	std::filesystem::path m_root;
	Write m_write;
	Cadence m_cadence;
	Storage m_storage;
	Catalogue m_catalogue;

	// The requests that have run since the last flush, in the order they ran.
	std::vector<Ran> m_ran;

	// The step the world is at, as its advances here have left it.
	std::uint64_t m_at{0};
	bool m_uncertain{false};
};

// Restores the backup (Storage::backups), or a stopped world's directory, into the directory, which
// must hold nothing but its lock, and recovers it into the world as Host::Start left it, to the head
// its catalogue names: the journal it returns serves it on from there. The backup is only read. Its
// catalogue is written last, so a restore cut off leaves a world a server refuses, not a new one.
// Throws std::runtime_error naming the file or the field as Journal and Journal::Recover do, or the
// directory when it holds anything; anything written is then removed again, the catalogue first.
Journal Restore(
    const std::filesystem::path &backup, const WorldDirectory &directory, Identity running,
    Jettison::Sim::SimWorld &world);
} // namespace Manifest::Host
