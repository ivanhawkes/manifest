#include <manifest/host/Journal.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <exception>
#include <format>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#include <jettison/core/Assert.h>
#include <jettison/core/File.h>
#include <jettison/core/Hash.h>
#include <jettison/core/Log.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>

#include <manifest/content/ContentIdentity.h>
#include <manifest/host/Start.h>

#include "Revision.h"

namespace Manifest::Host
{
namespace
{
using Jettison::Serialise::BinaryArchive;

// Formats change in place until player testing (D-109): a changed layout is another build's, and a
// world does not outlive its build (D-111).
constexpr Jettison::Serialise::FormatVersion kFormat{1};

constexpr std::string_view kIdentityName = "identity";
constexpr std::string_view kCatalogueName = "catalogue";
constexpr std::string_view kJournalName = "journal";
constexpr std::string_view kSnapshotsName = "snapshots";

constexpr Jettison::Core::Category kJournal{"journal"};

// The newest, and the one before it for a start to fall back to when the newest does not load.
constexpr std::size_t kKeptSnapshots = 2;
// A batch is flushed early once it holds this many requests, so a flood cannot grow one without
// bound; a step adds at most a full queue to it.
constexpr std::size_t kMaxBatchRequests = 4'096;

constexpr std::size_t kChecksumBytes = sizeof(std::uint64_t);
// The longest file of each kind read: nothing larger is one, so nothing larger is taken into memory.
// A batch is at most kMaxBatchRequests and a step's full queue of the largest payloads, 21 MiB, and
// each request's names; a content manifest is a few hundred KiB. A snapshot's is kMaxSnapshotBytes.
constexpr std::uintmax_t kMaxCatalogueBytes = std::uintmax_t{4} * 1024;
constexpr std::uintmax_t kMaxBatchBytes = std::uintmax_t{32} * 1024 * 1024;
constexpr std::uintmax_t kMaxIdentityBytes = std::uintmax_t{64} * 1024 * 1024;

// The steps the world took, from `step`, and the requests that ran in them, in the order they ran.
struct Batch
{
	std::string world;
	std::uint64_t sequence{0};
	// The identity of the batch before it, 0 for the first.
	std::uint64_t previous{0};
	std::uint64_t step{0};
	std::uint64_t steps{0};
	std::vector<Ran> ran;
};


void RegisterTypes()
{
	static const bool registered = []
	{
		Command::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<Identity>("Manifest.Host.Identity")
		    .Field<&Identity::world>("world", kSaved)
		    .Field<&Identity::game>("game", kSaved)
		    .Field<&Identity::sdk>("sdk", kSaved)
		    .Field<&Identity::fixedStep>("fixedStep", kSaved)
		    .Field<&Identity::commands>("commands", kSaved)
		    .Field<&Identity::content>("content", kSaved);
		RegisterType<Ran>("Manifest.Host.Ran")
		    .Field<&Ran::step>("step", kSaved)
		    .Field<&Ran::request>("request", kSaved);
		RegisterType<Kept>("Manifest.Host.Kept")
		    .Field<&Kept::batch>("batch", kSaved)
		    .Field<&Kept::batchId>("batchId", kSaved)
		    .Field<&Kept::snapshot>("snapshot", kSaved)
		    .Field<&Kept::step>("step", kSaved);
		RegisterType<Catalogue>("Manifest.Host.Catalogue")
		    .Field<&Catalogue::world>("world", kSaved)
		    .Field<&Catalogue::head>("head", kSaved)
		    .Field<&Catalogue::headId>("headId", kSaved)
		    .Field<&Catalogue::step>("step", kSaved)
		    .Field<&Catalogue::snapshots>("snapshots", kSaved);
		RegisterType<Batch>("Manifest.Host.Batch")
		    .Field<&Batch::world>("world", kSaved)
		    .Field<&Batch::sequence>("sequence", kSaved)
		    .Field<&Batch::previous>("previous", kSaved)
		    .Field<&Batch::step>("step", kSaved)
		    .Field<&Batch::steps>("steps", kSaved)
		    .Field<&Batch::ran>("ran", kSaved);
		return true;
	}();
	(void)registered;
}


template <typename Byte> std::uint64_t Checksum(std::span<const Byte> bytes)
{
	return Jettison::Core::Fnv1a64(
	    std::string_view{reinterpret_cast<const char *>(bytes.data()), bytes.size()});
}


// A record as its file holds it, its checksum after it, low byte first.
std::vector<std::byte> Framed(std::vector<std::byte> record)
{
	const std::uint64_t checksum = Checksum(std::span<const std::byte>{record});
	for (std::size_t index = 0; index < kChecksumBytes; ++index)
		record.push_back(static_cast<std::byte>((checksum >> (8 * index)) & 0xffu));
	return record;
}


// A file's contents with the checksum taken off the end of them.
struct Contents
{
	std::vector<std::byte> bytes;
	std::uint64_t checksum{0};
};


// Throws std::runtime_error naming the file for one that is missing, longer than any of its kind,
// cut short or changed since it was written.
Contents ReadFile(const std::filesystem::path &path, std::uintmax_t limit)
{
	std::error_code error;
	const std::uintmax_t size = std::filesystem::file_size(path, error);
	std::ifstream in{path, std::ios::binary};
	if (error || !in)
		throw std::runtime_error{std::format("{} is missing or cannot be read", path.string())};
	if (size < kChecksumBytes || size > limit)
		throw std::runtime_error{
		    std::format("{} is damaged: no such file is {} bytes long", path.string(), size)};
	std::vector<std::byte> bytes(size);
	in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
	if (!in)
		throw std::runtime_error{std::format("{} cannot be read", path.string())};
	std::uint64_t checksum = 0;
	for (std::size_t index = 0; index < kChecksumBytes; ++index)
		checksum |= std::to_integer<std::uint64_t>(bytes[size - kChecksumBytes + index]) << (8 * index);
	bytes.resize(size - kChecksumBytes);
	if (checksum != Checksum(std::span<const std::byte>{bytes}))
		throw std::runtime_error{
		    std::format("{} is damaged: its checksum is not its contents'", path.string())};
	return {std::move(bytes), checksum};
}


template <typename T> struct Record
{
	T value;
	std::uint64_t checksum{0};
};


// Throws as ReadFile does, and for a file that is not a T.
template <typename T> Record<T> Read(const std::filesystem::path &path, std::uintmax_t limit)
{
	const Contents contents = ReadFile(path, limit);
	try
	{
		return {BinaryArchive::Read<T>(contents.bytes, kFormat), contents.checksum};
	}
	catch (const std::exception &problem)
	{
		throw std::runtime_error{std::format("{} does not read: {}", path.string(), problem.what())};
	}
}


std::string Named(const std::vector<Command::Registered> &commands, std::size_t index)
{
	return index < commands.size()
	           ? std::format("{} at version {}", commands[index].name, commands[index].version)
	           : std::string{"none"};
}


// What a content refusal adds: the first definition that differs, or nothing when the world's
// manifest does not read, which the refusal stands without.
std::string FirstDefinitionDiffering(const Identity &made, const Identity &running)
{
	try
	{
		return std::format(
		    "; the first definition that differs is {}",
		    Content::FirstDifference(
		        std::as_bytes(std::span{made.content}), std::as_bytes(std::span{running.content})));
	}
	catch (const Jettison::Serialise::FormatError &)
	{
		return {};
	}
}


// Throws naming the first field of the world's identity the running build's does not equal.
void Hold(const std::filesystem::path &root, const Identity &made, const Identity &running)
{
	const auto refuse = [&](std::string_view field, const std::string &was, const std::string &is,
	                        const std::string &detail = {})
	{
		throw std::runtime_error{std::format(
		    "the world at {} was made by another build or other content: its {} is {}, and this "
		    "one's is {}{}",
		    root.string(), field, was, is, detail)};
	};
	if (made.game != running.game)
		refuse("game", made.game, running.game);
	if (made.sdk != running.sdk)
		refuse("SDK", made.sdk, running.sdk);
	if (std::bit_cast<std::uint64_t>(made.fixedStep) != std::bit_cast<std::uint64_t>(running.fixedStep))
		refuse("fixed step", std::format("{}", made.fixedStep), std::format("{}", running.fixedStep));
	if (made.commands != running.commands)
	{
		std::size_t first = 0;
		while (first < made.commands.size() && first < running.commands.size() &&
		       made.commands[first] == running.commands[first])
			++first;
		refuse("command", Named(made.commands, first), Named(running.commands, first));
	}
	if (made.content != running.content)
		refuse(
		    "content", std::format("{:016x}", Checksum(std::span<const std::uint8_t>{made.content})),
		    std::format("{:016x}", Checksum(std::span<const std::uint8_t>{running.content})),
		    FirstDefinitionDiffering(made, running));
}


// Every digit of a 64-bit sequence, so the names sort as the batches run.
std::filesystem::path Numbered(
    const std::filesystem::path &root, std::string_view directory, std::uint64_t sequence)
{
	return root / directory / std::format("{:020}", sequence);
}


void Take(Jettison::Sim::SimWorld &world, std::uint64_t until)
{
	while (world.Clock().Step() < until)
		world.FastForward(until);
}


// Whether the path is the directory or within it, as they are on disk.
bool Within(const std::filesystem::path &path, const std::filesystem::path &directory)
{
	const auto resolved = [](const std::filesystem::path &given)
	{
		// Without the empty last element a trailing separator leaves.
		const std::filesystem::path on = std::filesystem::weakly_canonical(given);
		return on.has_filename() ? on : on.parent_path();
	};
	const std::filesystem::path inner = resolved(path);
	const std::filesystem::path outer = resolved(directory);
	return std::mismatch(outer.begin(), outer.end(), inner.begin(), inner.end()).first == outer.end();
}


// Removes the directory's files numbered below the bound, a write's leavings among them. A file that
// will not go is left: the catalogue names none of them.
void RemoveBelow(const std::filesystem::path &directory, std::uint64_t bound)
{
	std::error_code error;
	for (std::filesystem::directory_iterator entry{directory, error}, end; !error && entry != end;
	     entry.increment(error))
	{
		const std::string name = entry->path().filename().string();
		std::uint64_t sequence = 0;
		if (std::from_chars(name.data(), name.data() + name.size(), sequence).ec == std::errc{} &&
		    sequence < bound)
		{
			std::error_code ignored;
			std::filesystem::remove(entry->path(), ignored);
		}
	}
}
} // namespace


Identity RunningIdentity(Jettison::Host::Session &session)
{
	const Services services = HostedServices(session);
	Identity identity;
	identity.game = kGameRevision;
	identity.sdk = kSdkRevision;
	identity.fixedStep = session.World().FixedStep();
	identity.commands = services.requests.Commands();
	identity.content.reserve(services.content.manifest.size());
	for (const std::byte byte : services.content.manifest)
		identity.content.push_back(std::to_integer<std::uint8_t>(byte));
	return identity;
}


Journal::Journal(
    const WorldDirectory &directory, Identity running, Cadence cadence, Write write, Storage storage)
    : m_root{directory.root},
      m_write{
          write ? std::move(write)
                : Write{[](const std::filesystem::path &path, std::span<const std::byte> bytes)
                        { Jettison::Core::WriteFileDurably(path, bytes); }}},
      m_cadence{cadence}, m_storage{std::move(storage)}
{
	RegisterTypes();
	if (!m_storage.backups.empty())
	{
		if (Within(m_storage.backups, m_root))
			throw std::runtime_error{std::format(
			    "the backups at {} are within the world at {}: they go outside it",
			    m_storage.backups.string(), m_root.string())};
		// Private to its owner when it is made here, as the world's directory is.
		if (Jettison::Core::CreateDirectoriesDurably(m_storage.backups))
			std::filesystem::permissions(m_storage.backups, std::filesystem::perms::owner_all);
	}
	const std::filesystem::path identity = m_root / kIdentityName;
	const std::filesystem::path catalogue = m_root / kCatalogueName;
	if (!std::filesystem::exists(identity) && !std::filesystem::exists(catalogue) &&
	    !std::filesystem::exists(m_root / kJournalName))
	{
		std::random_device device;
		running.world = std::format("{:08x}{:08x}{:08x}{:08x}", device(), device(), device(), device());
		m_catalogue.world = running.world;
		// The identity first: a world cut off between the two has no catalogue, and is refused
		// rather than made again over whatever else is there.
		m_write(identity, Framed(BinaryArchive::Write(running, kFormat)));
		m_write(catalogue, Framed(BinaryArchive::Write(m_catalogue, kFormat)));
		return;
	}
	const Identity made = Read<Identity>(identity, kMaxIdentityBytes).value;
	Hold(m_root, made, running);
	m_catalogue = Read<Catalogue>(catalogue, kMaxCatalogueBytes).value;
	if (m_catalogue.world != made.world)
		throw std::runtime_error{
		    std::format("{} is the catalogue of another world", catalogue.string())};
	m_at = m_catalogue.step;
}


void Journal::Recover(Jettison::Sim::SimWorld &world) const
{
	std::uint64_t after = 0;
	std::uint64_t previous = 0;
	const std::vector<Kept> &snapshots = m_catalogue.snapshots;
	for (auto kept = snapshots.rbegin(); kept != snapshots.rend(); ++kept)
	{
		const std::filesystem::path path = Numbered(m_root, kSnapshotsName, kept->batch);
		try
		{
			const Contents snapshot = ReadFile(path, kMaxSnapshotBytes);
			if (snapshot.checksum != kept->snapshot)
				throw std::runtime_error{
				    std::format("{} is not the snapshot the catalogue names", path.string())};
			world.Load(snapshot.bytes);
			after = kept->batch;
			previous = kept->batchId;
			break;
		}
		catch (const std::runtime_error &problem)
		{
			// Every batch since the older snapshot is kept, and every batch of a world that has not
			// yet two; past those there is nowhere to start from.
			if (std::next(kept) == snapshots.rend() && snapshots.size() == kKeptSnapshots)
				throw;
			Jettison::Core::Log(
			    Jettison::Core::Severity::Warning, kJournal, "{}; starting from before it",
			    problem.what());
		}
	}
	for (std::uint64_t sequence = after + 1; sequence <= m_catalogue.head; ++sequence)
	{
		const std::filesystem::path path = Numbered(m_root, kJournalName, sequence);
		const auto [batch, id] = Read<Batch>(path, kMaxBatchBytes);
		const auto broken = [&]
		{
			return std::runtime_error{std::format(
			    "{} is not the batch the world's journal has next: the chain is broken",
			    path.string())};
		};
		if (batch.world != m_catalogue.world || batch.sequence != sequence ||
		    batch.previous != previous || batch.step != world.Clock().Step() || batch.steps == 0 ||
		    batch.steps > std::numeric_limits<std::uint64_t>::max() - batch.step ||
		    batch.ran.size() > kMaxBatchRequests + Command::kQueueCapacity ||
		    (sequence == m_catalogue.head && id != m_catalogue.headId))
			throw broken();
		previous = id;
		const std::uint64_t until = batch.step + batch.steps;
		for (const Ran &ran : batch.ran)
		{
			if (ran.step < world.Clock().Step() || ran.step >= until)
				throw broken();
			Take(world, ran.step);
			world.Commands().Push(ran.request);
		}
		Take(world, until);
		// What the engine raises is presentation's, and a replay presents none of it.
		(void)world.Events().Take();
	}
	if (world.Clock().Step() != m_catalogue.step)
		throw std::runtime_error{std::format(
		    "{} names step {}, where its journal ends at step {}", (m_root / kCatalogueName).string(),
		    m_catalogue.step, world.Clock().Step())};
}


void Journal::Expect(const Jettison::Sim::SimWorld &world) const
{
	if (m_uncertain)
		throw std::logic_error{"a write failed, and the world's journal takes no other"};
	if (world.Clock().Step() != m_at)
		throw std::logic_error{std::format(
		    "the world is at step {}, where its journal left it at step {}", world.Clock().Step(),
		    m_at)};
}


void Journal::Advance(Jettison::Sim::SimWorld &world, std::uint64_t steps)
{
	Expect(world);
	for (const entt::meta_any &command : world.Commands().Pending())
	{
		// Anything else queued would run now and not in a replay.
		const auto *pRequest = command.try_cast<const Command::Request>();
		JETTISON_VERIFY(pRequest != nullptr);
		m_ran.push_back({m_at, *pRequest});
	}
	Take(world, m_at + steps);
	m_at = world.Clock().Step();
	if (m_at - m_catalogue.step >= m_cadence.flushSteps || m_ran.size() >= kMaxBatchRequests)
		Flush();
	const std::uint64_t snapshot =
	    m_catalogue.snapshots.empty() ? 0 : m_catalogue.snapshots.back().step;
	if (m_at - snapshot >= m_cadence.snapshotSteps)
		Snapshot(world);
}


void Journal::Flush()
{
	if (m_uncertain)
		throw std::logic_error{"a write failed, and the world's journal takes no other"};
	if (m_at == m_catalogue.step)
		return;
	const Batch batch{m_catalogue.world, m_catalogue.head + 1,    m_catalogue.headId,
	                  m_catalogue.step,  m_at - m_catalogue.step, m_ran};
	const std::vector<std::byte> record = BinaryArchive::Write(batch, kFormat);
	Catalogue next = m_catalogue;
	next.head = batch.sequence;
	next.headId = Checksum(std::span<const std::byte>{record});
	next.step = m_at;
	Publish(std::move(next), Numbered(m_root, kJournalName, batch.sequence), Framed(record));
	m_ran.clear();
}


void Journal::Snapshot(Jettison::Sim::SimWorld &world)
{
	Flush();
	// Between steps a hosted world has nothing queued but a request not yet run, which no batch
	// holds and a snapshot must not either.
	JETTISON_VERIFY(world.Commands().Pending().empty());
	std::vector<std::byte> state = world.Save();
	if (state.size() + kChecksumBytes > m_storage.snapshotBytes)
		throw std::runtime_error{std::format(
		    "the world's save is {} bytes, more than a snapshot may be ({} bytes with its checksum), "
		    "and is not written",
		    state.size(), m_storage.snapshotBytes)};
	Catalogue next = m_catalogue;
	next.snapshots.push_back(
	    {next.head, next.headId, Checksum(std::span<const std::byte>{state}), next.step});
	if (next.snapshots.size() > kKeptSnapshots)
		next.snapshots.erase(next.snapshots.begin());
	const std::vector<std::byte> file = Framed(std::move(state));
	Publish(std::move(next), Numbered(m_root, kSnapshotsName, m_catalogue.head), file);
	Prune();
	Backup(file);
}


void Journal::Stop(Jettison::Sim::SimWorld &world)
{
	Expect(world);
	if (!world.Commands().Pending().empty())
		Advance(world, 1);
	if (m_catalogue.snapshots.empty() || m_catalogue.snapshots.back().step != m_at)
		Snapshot(world);
}


void Journal::Publish(
    Catalogue next, const std::filesystem::path &path, std::span<const std::byte> file)
{
	// Until both are written: a writer that throws may have left either file in place.
	m_uncertain = true;
	m_write(path, file);
	m_write(m_root / kCatalogueName, Framed(BinaryArchive::Write(next, kFormat)));
	m_uncertain = false;
	m_catalogue = std::move(next);
}


void Journal::Prune() const
{
	// Only once the catalogue names two: until then a start may need the world's first batch.
	if (m_catalogue.snapshots.size() < kKeptSnapshots)
		return;
	const std::uint64_t older = m_catalogue.snapshots.front().batch;
	RemoveBelow(m_root / kSnapshotsName, older);
	RemoveBelow(m_root / kJournalName, older + 1);
}


void Journal::Backup(std::span<const std::byte> snapshot) const
{
	if (m_storage.backups.empty())
		return;
	const std::string prefix = m_catalogue.world + ".";
	const std::string name = std::format("{}{:020}", prefix, m_catalogue.head);
	const std::filesystem::path into = m_storage.backups / name;
	try
	{
		// One cut off at this batch before, which no catalogue names.
		std::filesystem::remove_all(into);
		const Kept &kept = m_catalogue.snapshots.back();
		const Catalogue alone{
		    m_catalogue.world, m_catalogue.head, m_catalogue.headId, m_catalogue.step, {kept}};
		m_write(
		    into / kIdentityName, Framed(ReadFile(m_root / kIdentityName, kMaxIdentityBytes).bytes));
		m_write(Numbered(into, kSnapshotsName, kept.batch), snapshot);
		m_write(into / kCatalogueName, Framed(BinaryArchive::Write(alone, kFormat)));
	}
	catch (const std::runtime_error &problem)
	{
		// The world's own files are whole: a backup that failed costs the server nothing but itself.
		Jettison::Core::Log(
		    Jettison::Core::Severity::Error, kJournal, "the backup {} was not made: {}", into.string(),
		    problem.what());
		return;
	}

	// The world's backups but this one: one with no catalogue is one cut off, and goes; of the rest,
	// the newest are kept with it. Names sort as their batches do.
	std::vector<std::filesystem::path> complete;
	std::error_code error;
	for (std::filesystem::directory_iterator entry{m_storage.backups, error}, end;
	     !error && entry != end; entry.increment(error))
	{
		const std::string found = entry->path().filename().string();
		if (found == name || !found.starts_with(prefix))
			continue;
		std::uint64_t batch = 0;
		const char *pLast = found.data() + found.size();
		const auto [pEnd, problem] = std::from_chars(found.data() + prefix.size(), pLast, batch);
		if (problem != std::errc{} || pEnd != pLast)
			continue;
		std::error_code ignored;
		if (std::filesystem::exists(entry->path() / kCatalogueName, ignored))
			complete.push_back(entry->path());
		else
			std::filesystem::remove_all(entry->path(), ignored);
	}
	std::ranges::sort(complete);
	const std::size_t others = m_storage.kept == 0 ? 0 : m_storage.kept - 1;
	for (std::size_t index = 0; index + others < complete.size(); ++index)
	{
		std::error_code ignored;
		std::filesystem::remove_all(complete[index], ignored);
	}
}


Journal Restore(
    const std::filesystem::path &backup, const WorldDirectory &directory, Identity running,
    Jettison::Sim::SimWorld &world)
{
	RegisterTypes();
	const std::filesystem::path &root = directory.root;
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator{root})
		if (entry.path().filename() != kLockName)
			throw std::runtime_error{std::format(
			    "the world at {} is not empty: a backup is restored only into an empty directory",
			    root.string())};
	// Refused before anything is written: another build's, or a catalogue not the world's.
	const Identity made = Read<Identity>(backup / kIdentityName, kMaxIdentityBytes).value;
	Hold(backup, made, running);
	const Catalogue catalogue = Read<Catalogue>(backup / kCatalogueName, kMaxCatalogueBytes).value;
	if (catalogue.world != made.world)
		throw std::runtime_error{
		    std::format("{} is the catalogue of another world", (backup / kCatalogueName).string())};

	// What a start may need: each snapshot named and every batch after the oldest, or every batch of
	// a world with none. The catalogue last.
	std::vector<std::pair<std::filesystem::path, std::uintmax_t>> files{
	    {kIdentityName, kMaxIdentityBytes}};
	for (const Kept &kept : catalogue.snapshots)
		files.emplace_back(Numbered({}, kSnapshotsName, kept.batch), kMaxSnapshotBytes);
	const std::uint64_t first = catalogue.snapshots.empty() ? 1 : catalogue.snapshots.front().batch + 1;
	for (std::uint64_t sequence = first; sequence <= catalogue.head; ++sequence)
		files.emplace_back(Numbered({}, kJournalName, sequence), kMaxBatchBytes);
	files.emplace_back(kCatalogueName, kMaxCatalogueBytes);
	try
	{
		for (const auto &[name, limit] : files)
			Jettison::Core::WriteFileDurably(root / name, Framed(ReadFile(backup / name, limit).bytes));
		Journal journal{directory, std::move(running)};
		journal.Recover(world);
		return journal;
	}
	catch (const std::runtime_error &)
	{
		// The catalogue first: whatever else will not go, a server refuses a world without one.
		std::error_code ignored;
		std::filesystem::remove(root / kCatalogueName, ignored);
		std::filesystem::remove(root / kIdentityName, ignored);
		std::filesystem::remove_all(root / kSnapshotsName, ignored);
		std::filesystem::remove_all(root / kJournalName, ignored);
		throw;
	}
}
} // namespace Manifest::Host
