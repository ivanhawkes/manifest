#include "TestHelpers.h"

#include <manifest/host/Start.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <jettison/host/Session.h>
#include <jettison/sim/SimWorld.h>

using Jettison::Host::Phase;
using Jettison::Host::StartupError;

namespace
{
// A copy of the staged content, directory and pack, that a case may damage.
class Content
{
  public:
	Content()
	{
		static std::atomic<int> count{0};
		m_root = std::filesystem::temp_directory_path() /
		         std::format("manifest-host-tests-{}-{}", getpid(), count++);
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		std::filesystem::copy(
		    MANIFEST_MOD_DIRECTORY, Directory(), std::filesystem::copy_options::recursive);
		std::filesystem::copy_file(MANIFEST_MOD_PACK, Pack());
	}


	~Content()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_root, ignored);
	}


	Content(const Content &) = delete;


	Content &operator=(const Content &) = delete;


	std::filesystem::path Directory() const
	{
		return m_root / "manifest";
	}


	std::filesystem::path Pack() const
	{
		return m_root / "manifest.pack";
	}


	void Put(const std::string &path, const std::string &text) const
	{
		std::ofstream{Directory() / path, std::ios::binary | std::ios::trunc} << text;
	}


	void Remove(const std::string &path) const
	{
		std::filesystem::remove(Directory() / path);
	}


	std::vector<char> PackBytes() const
	{
		std::ifstream in{Pack(), std::ios::binary};
		return {std::istreambuf_iterator<char>{in}, {}};
	}


	void PutPack(const std::vector<char> &bytes) const
	{
		std::ofstream{Pack(), std::ios::binary | std::ios::trunc}.write(
		    bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}

  private:
	std::filesystem::path m_root;
};


template <typename T> T ReadAt(const std::vector<char> &bytes, std::size_t at)
{
	T value{};
	std::memcpy(&value, bytes.data() + at, sizeof value);
	return value;
}


// Where a file's stored bytes are in a pack, from its index (vfs/Pack.h has the layout).
struct Stored
{
	std::uint8_t method{};
	std::uint64_t offset{};
	std::uint64_t size{};
};


Stored FindStored(const std::vector<char> &pack, std::string_view path)
{
	const auto count = ReadAt<std::uint32_t>(pack, 12);
	auto at = static_cast<std::size_t>(ReadAt<std::uint64_t>(pack, 16));
	for (std::uint32_t file = 0; file < count; ++file)
	{
		const auto length = ReadAt<std::uint32_t>(pack, at);
		const std::string_view name{pack.data() + at + 4, length};
		at += 4 + length;
		const Stored stored{
		    ReadAt<std::uint8_t>(pack, at), ReadAt<std::uint64_t>(pack, at + 1),
		    ReadAt<std::uint64_t>(pack, at + 9)};
		if (name == path)
			return stored;
		at += 25;
	}
	FAIL("the pack has no ", path);
	return {};
}


void CheckFails(const std::filesystem::path &content, Phase phase, std::string_view cause)
{
	try
	{
		(void)Manifest::Host::Start(content);
		FAIL("started from ", content);
	}
	catch (const StartupError &error)
	{
		CHECK(error.FailedPhase() == phase);
		CHECK(error.Source() == content);
		MANIFEST_CHECK_CONTAINS(error.what(), cause);
	}
}


void CheckFailsDescribed(const std::string &description, Phase phase, std::string_view cause)
{
	const Content content;
	content.Put("mod.yaml", description);
	CheckFails(content.Directory(), phase, cause);
}
} // namespace


TEST_CASE("The staged directory and pack both start Manifest with an empty world, and shut down")
{
	for (const std::filesystem::path content : {MANIFEST_MOD_DIRECTORY, MANIFEST_MOD_PACK})
	{
		CAPTURE(content);
		const auto session = Manifest::Host::Start(content);
		REQUIRE(session->Running());
		REQUIRE(session->Packages().size() == 1);
		REQUIRE(session->Packages().front().description);
		CHECK(session->Packages().front().description->name == "Manifest");
		const entt::registry &registry = session->World().Registry();
		CHECK(registry.storage<entt::entity>()->size() == 0);
		session->Shutdown();
		CHECK_FALSE(session->Running());
		session->Shutdown();
	}
}


TEST_CASE("A session starts after one that failed, and beside another")
{
	CheckFails("/nonexistent/manifest.pack", Phase::Mount, "");
	const auto first = Manifest::Host::Start(MANIFEST_MOD_DIRECTORY);
	const auto second = Manifest::Host::Start(MANIFEST_MOD_PACK);
	CHECK(first->Running());
	CHECK(second->Running());
}


TEST_CASE("A source that is not there, or a pack whose index is damaged, fails to mount")
{
	CheckFails("/nonexistent/manifest", Phase::Mount, "");

	const Content content;
	std::vector<char> pack = content.PackBytes();
	const std::uint64_t past = pack.size();
	std::memcpy(pack.data() + 16, &past, sizeof past);
	content.PutPack(pack);
	CheckFails(content.Pack(), Phase::Mount, "");
}


TEST_CASE("A damaged required file in the pack fails start-up as it is read")
{
	const Content content;
	std::vector<char> pack = content.PackBytes();
	const Stored stored = FindStored(pack, "manifest/definitions/game-modes/full.yaml");
	// A stored file has no checksum; a compressed one has its frame's.
	REQUIRE(stored.method == 1);
	pack[stored.offset + stored.size / 2] ^= 0x5a;
	content.PutPack(pack);
	CheckFails(content.Pack(), Phase::Content, "");
}


TEST_CASE("A description that does not read, or is of another format version, fails")
{
	CheckFailsDescribed("version: 1\nname: [Manifest\n", Phase::Content, "");
	CheckFailsDescribed(
	    "version: 2\nname: Manifest\nrelease: 0.0.1\nengine: 0.1.0\n", Phase::Content, "version");
}


TEST_CASE("Manifest's description must name it and give its release and engine requirement")
{
	{
		const Content content;
		content.Remove("mod.yaml");
		CheckFails(content.Directory(), Phase::Content, "it has no mod.yaml");
	}
	CheckFailsDescribed("version: 1\nrelease: 0.0.1\nengine: 0.1.0\n", Phase::Content, "gives no name");
	CheckFailsDescribed(
	    "version: 1\nname: ''\nrelease: 0.0.1\nengine: 0.1.0\n", Phase::Content, "gives no name");
	CheckFailsDescribed(
	    "version: 1\nname: Elsewhere\nrelease: 0.0.1\nengine: 0.1.0\n", Phase::Content,
	    "the package 'Elsewhere', not Manifest");
	CheckFailsDescribed(
	    "version: 1\nname: Manifest\nengine: 0.1.0\n", Phase::Content, "gives no release");
	CheckFailsDescribed(
	    "version: 1\nname: Manifest\nrelease: 0.0.1\n", Phase::Content, "gives no engine requirement");
}


TEST_CASE("A newer engine requirement, or a required package not given, fails")
{
	CheckFailsDescribed(
	    "version: 1\nname: Manifest\nrelease: 0.0.1\nengine: 99.0.0\n", Phase::Content,
	    "requires engine 99.0.0");
	CheckFailsDescribed(
	    "version: 1\nname: Manifest\nrelease: 0.0.1\nengine: 0.1.0\nrequires: [base]\n", Phase::Content,
	    "requires the package 'base'");
}


TEST_CASE("Content that is missing, cannot be read or breaks a rule fails")
{
	{
		const Content content;
		content.Remove("manifest/definitions/game-modes/full.yaml");
		// The scenarios name it first.
		CheckFails(content.Directory(), Phase::Content, "gameMode 'full' names no game mode");
	}
	{
		const Content content;
		content.Put("manifest/definitions/ages/age-stone.patch.yaml", "version: 1\ndisplayName: ''\n");
		CheckFails(
		    content.Directory(), Phase::Content,
		    "manifest/definitions/ages/age-stone.yaml: Manifest.Age 'AgeStone': displayName is "
		    "required (from manifest, manifest (patch))");
	}

	const Content content;
	const std::filesystem::path file =
	    content.Directory() / "manifest/definitions/game-modes/full.yaml";
	std::filesystem::permissions(file, std::filesystem::perms::none);
	// Root reads it anyway, as a CI container may run the suite.
	if (std::ifstream{file}.is_open())
		MESSAGE("running as a user that reads a file without permissions; not checked");
	else
		CheckFails(content.Directory(), Phase::Content, "full.yaml");
}
