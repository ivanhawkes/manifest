#include <manifest/host/Journal.h>

#include <bit>
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
#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>

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

constexpr std::size_t kChecksumBytes = sizeof(std::uint64_t);
// The longest file of each kind read: nothing larger is one, so nothing larger is taken into memory.
// A batch is at most a full queue of the largest payloads, 4 MiB, and each request's names; a
// content manifest is a few hundred KiB.
constexpr std::uintmax_t kMaxCatalogueBytes = std::uintmax_t{4} * 1024;
constexpr std::uintmax_t kMaxBatchBytes = std::uintmax_t{8} * 1024 * 1024;
constexpr std::uintmax_t kMaxIdentityBytes = std::uintmax_t{64} * 1024 * 1024;

// What the catalogue holds: the committed head of the world's journal.
struct Catalogue
{
	std::string world;
	std::uint64_t head{0};
	std::uint64_t headId{0};
	std::uint64_t step{0};
};

// Steps the world takes, from `step`, and the requests run at the start of the first of them, in
// admission's order. Several steps to a batch is what lets a slow disk hold the world back by whole
// batches without a queue of them: the server commits the steps that are due, however many.
struct Batch
{
	std::string world;
	std::uint64_t sequence{0};
	// The identity of the batch before it, 0 for the first.
	std::uint64_t previous{0};
	std::uint64_t step{0};
	std::uint64_t steps{0};
	std::vector<Command::Request> requests;
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
		RegisterType<Catalogue>("Manifest.Host.Catalogue")
		    .Field<&Catalogue::world>("world", kSaved)
		    .Field<&Catalogue::head>("head", kSaved)
		    .Field<&Catalogue::headId>("headId", kSaved)
		    .Field<&Catalogue::step>("step", kSaved);
		RegisterType<Batch>("Manifest.Host.Batch")
		    .Field<&Batch::world>("world", kSaved)
		    .Field<&Batch::sequence>("sequence", kSaved)
		    .Field<&Batch::previous>("previous", kSaved)
		    .Field<&Batch::step>("step", kSaved)
		    .Field<&Batch::steps>("steps", kSaved)
		    .Field<&Batch::requests>("requests", kSaved);
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


template <typename T> struct Record
{
	T value;
	std::uint64_t checksum{0};
};


// Throws std::runtime_error naming the file for one that is missing, longer than any of its kind,
// cut short, changed since it was written or not a T.
template <typename T> Record<T> Read(const std::filesystem::path &path, std::uintmax_t limit)
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
	const std::span<const std::byte> record{bytes.data(), bytes.size() - kChecksumBytes};
	std::uint64_t checksum = 0;
	for (std::size_t index = 0; index < kChecksumBytes; ++index)
		checksum |= std::to_integer<std::uint64_t>(bytes[record.size() + index]) << (8 * index);
	if (checksum != Checksum(record))
		throw std::runtime_error{
		    std::format("{} is damaged: its checksum is not its contents'", path.string())};
	try
	{
		return {BinaryArchive::Read<T>(record, kFormat), checksum};
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


// Throws naming the first field of the world's identity the running build's does not equal.
void Hold(const std::filesystem::path &root, const Identity &made, const Identity &running)
{
	const auto refuse = [&](std::string_view field, const std::string &was, const std::string &is)
	{
		throw std::runtime_error{std::format(
		    "the world at {} was made by another build or other content: its {} is {}, and this "
		    "one's is {}",
		    root.string(), field, was, is)};
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
		    std::format("{:016x}", Checksum(std::span<const std::uint8_t>{running.content})));
}


std::filesystem::path BatchPath(const std::filesystem::path &root, std::uint64_t sequence)
{
	// Every digit of a 64-bit sequence, so the names sort as the batches run.
	return root / kJournalName / std::format("{:020}", sequence);
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


Journal::Journal(const WorldDirectory &directory, Identity running, Write write)
    : m_root{directory.root},
      m_write{
          write ? std::move(write)
                : Write{[](const std::filesystem::path &path, std::span<const std::byte> bytes)
                        { Jettison::Core::WriteFileDurably(path, bytes); }}}
{
	RegisterTypes();
	const std::filesystem::path identity = m_root / kIdentityName;
	const std::filesystem::path catalogue = m_root / kCatalogueName;
	if (!std::filesystem::exists(identity) && !std::filesystem::exists(catalogue) &&
	    !std::filesystem::exists(m_root / kJournalName))
	{
		std::random_device device;
		running.world = std::format("{:08x}{:08x}{:08x}{:08x}", device(), device(), device(), device());
		// The identity first: a world cut off between the two has no catalogue, and is refused
		// rather than made again over whatever else is there.
		m_write(identity, Framed(BinaryArchive::Write(running, kFormat)));
		m_write(catalogue, Framed(BinaryArchive::Write(Catalogue{running.world, 0, 0, 0}, kFormat)));
		m_world = std::move(running.world);
		return;
	}
	const Identity made = Read<Identity>(identity, kMaxIdentityBytes).value;
	Hold(m_root, made, running);
	const Catalogue committed = Read<Catalogue>(catalogue, kMaxCatalogueBytes).value;
	if (committed.world != made.world)
		throw std::runtime_error{
		    std::format("{} is the catalogue of another world", catalogue.string())};
	m_world = committed.world;
	m_head = committed.head;
	m_headId = committed.headId;
	m_step = committed.step;
}


void Journal::Replay(Jettison::Sim::SimWorld &world) const
{
	std::uint64_t previous = 0;
	for (std::uint64_t sequence = 1; sequence <= m_head; ++sequence)
	{
		const std::filesystem::path path = BatchPath(m_root, sequence);
		const auto [batch, id] = Read<Batch>(path, kMaxBatchBytes);
		if (batch.world != m_world || batch.sequence != sequence || batch.previous != previous ||
		    batch.step != world.Clock().Step() || batch.steps == 0 ||
		    batch.steps > std::numeric_limits<std::uint64_t>::max() - batch.step ||
		    batch.requests.size() > Command::kQueueCapacity || (sequence == m_head && id != m_headId))
			throw std::runtime_error{std::format(
			    "{} is not the batch the world's journal has next: the chain is broken",
			    path.string())};
		previous = id;
		for (const Command::Request &request : batch.requests)
			world.Commands().Push(request);
		const std::uint64_t until = batch.step + batch.steps;
		while (world.Clock().Step() < until)
			world.FastForward(until);
		// What the engine raises is presentation's, and a replay presents none of it.
		(void)world.Events().Take();
	}
	if (world.Clock().Step() != m_step)
		throw std::runtime_error{std::format(
		    "{} names step {}, where its journal ends at step {}", (m_root / kCatalogueName).string(),
		    m_step, world.Clock().Step())};
}


void Journal::Commit(Jettison::Sim::SimWorld &world, std::uint64_t steps)
{
	if (m_uncertain)
		throw std::logic_error{"a commit failed, and the world's journal takes no other"};
	if (steps == 0 || world.Clock().Step() != m_step)
		throw std::logic_error{std::format(
		    "a batch of {} steps from step {} does not follow a journal that ends at step {}", steps,
		    world.Clock().Step(), m_step)};
	Batch batch{m_world, m_head + 1, m_headId, m_step, steps, {}};
	for (const entt::meta_any &command : world.Commands().Pending())
	{
		// Anything else queued would run now and not in a replay.
		const auto *pRequest = command.try_cast<const Command::Request>();
		JETTISON_VERIFY(pRequest != nullptr);
		batch.requests.push_back(*pRequest);
	}
	const std::vector<std::byte> record = BinaryArchive::Write(batch, kFormat);
	const Catalogue next{
	    m_world, batch.sequence, Checksum(std::span<const std::byte>{record}), m_step + steps};
	// Until both are written: a writer that throws may have left either file in place.
	m_uncertain = true;
	m_write(BatchPath(m_root, batch.sequence), Framed(record));
	m_write(m_root / kCatalogueName, Framed(BinaryArchive::Write(next, kFormat)));
	m_uncertain = false;
	m_head = next.head;
	m_headId = next.headId;
	m_step = next.step;
}
} // namespace Manifest::Host
