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

// A world's journal and catalogue (docs/stage-5-recovery-plan.md, "Step journals and the catalogue"):
// the steps a server is about to take and the requests that run at the start of them, written as
// one immutable batch file, then the catalogue replaced to name that batch as the committed head.
// Only then are the steps the world's to take and the requests acknowledged. A committed batch is an
// obligation: Replay takes its steps in a world that had not. Every file is the binary archive's
// record with its 64-bit FNV-1a after it, which is also a batch's identity in the chain.
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
	Journal(const WorldDirectory &directory, Identity running, Write write = {});

	const std::string &World() const
	{
		return m_world;
	}

	// The sequence of the last committed batch, from 1; 0 for none.
	std::uint64_t Head() const
	{
		return m_head;
	}

	// Takes every committed batch's steps in a world as Host::Start left it, its requests run where
	// they first ran, so the world and its request ledger are as the journal's last step left them.
	// A batch written but never named by the catalogue is not run. Throws std::runtime_error naming
	// the file for a batch that is missing, damaged or not the next in the chain; the world is then
	// part-way through the journal and is discarded.
	void Replay(Jettison::Sim::SimWorld &world) const;

	// Commits the next steps of the world, at least one, from the step its clock is at, with the
	// requests queued on it: nothing else may be queued. Returns once both files are on the disk;
	// until then no request of the batch is acknowledged and no step of it taken. A failure throws
	// what the writer threw and leaves the commit uncertain, since the catalogue may already name
	// the batch: the journal then refuses every later commit with std::logic_error, and the next
	// opening finds which it was. Throws std::logic_error too for a world not at the step the
	// journal ends at.
	void Commit(Jettison::Sim::SimWorld &world, std::uint64_t steps);

  private:
	std::filesystem::path m_root;
	Write m_write;
	std::string m_world;
	std::uint64_t m_head{0};
	// The head batch's identity, which the next names as the one before it.
	std::uint64_t m_headId{0};
	// The step after the head batch's last.
	std::uint64_t m_step{0};
	bool m_uncertain{false};
};
} // namespace Manifest::Host
