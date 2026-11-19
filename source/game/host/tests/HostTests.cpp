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
#include <jettison/vfs/FileSystem.h>

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


Manifest::Host::Options OptionsFor(
    const std::filesystem::path &content, std::vector<std::filesystem::path> overlays = {})
{
	return {content, std::move(overlays), "trading"};
}


// The host and the validator each fail in the phase, at the source, with the same report.
void CheckFails(
    const Manifest::Host::Options &options, Phase phase, const std::filesystem::path &source,
    std::string_view cause)
{
	std::string started;
	try
	{
		(void)Manifest::Host::Start(options);
		FAIL("started from ", options.content);
	}
	catch (const StartupError &error)
	{
		CHECK(error.FailedPhase() == phase);
		CHECK(error.Source() == source);
		MANIFEST_CHECK_CONTAINS(error.what(), cause);
		started = error.what();
	}
	try
	{
		(void)Manifest::Host::Validate(options);
		FAIL("validated ", options.content);
	}
	catch (const StartupError &error)
	{
		CHECK(error.FailedPhase() == phase);
		CHECK(error.Source() == source);
		CHECK(error.what() == started);
	}
}


void CheckFails(const std::filesystem::path &content, Phase phase, std::string_view cause)
{
	CheckFails(OptionsFor(content), phase, content, cause);
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
		const auto session = Manifest::Host::Start(OptionsFor(content));
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
	const auto first = Manifest::Host::Start(OptionsFor(MANIFEST_MOD_DIRECTORY));
	const auto second = Manifest::Host::Start(OptionsFor(MANIFEST_MOD_PACK));
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
	const Stored stored = FindStored(pack, "manifest/definitions/game-modes/manifest.yaml");
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
		content.Remove("manifest/definitions/game-modes/manifest.yaml");
		// The scenarios name it first.
		CheckFails(content.Directory(), Phase::Content, "gameMode 'manifest' names no game mode");
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
	    content.Directory() / "manifest/definitions/game-modes/manifest.yaml";
	std::filesystem::permissions(file, std::filesystem::perms::none);
	// Root reads it anyway, as a CI container may run the suite.
	if (std::ifstream{file}.is_open())
		MESSAGE("running as a user that reads a file without permissions; not checked");
	else
		CheckFails(content.Directory(), Phase::Content, "manifest.yaml");
}


TEST_CASE("The options are read the same way for the host and the validator")
{
	using Manifest::Host::ParseOptions;
	const auto parse = [](std::vector<std::string_view> arguments, std::vector<std::string_view> &rest)
	{ return ParseOptions(arguments, rest); };

	std::vector<std::string_view> rest;
	const auto options = parse(
	    {"--overlay", "a", "--content", "base", "--check-startup", "--scenario", "trading", "--overlay",
	     "b"},
	    rest);
	REQUIRE(options);
	CHECK(options->content == "base");
	CHECK(options->overlays == std::vector<std::filesystem::path>{"a", "b"});
	CHECK(options->scenario == "trading");
	CHECK(rest == std::vector<std::string_view>{"--check-startup"});

	for (const std::vector<std::string_view> &refused : std::vector<std::vector<std::string_view>>{
	         {},
	         {"--content", "base"},
	         {"--scenario", "trading"},
	         {"--content", "base", "--scenario", "trading", "--content", "again"},
	         {"--content", "base", "--scenario", "trading", "--scenario", "again"},
	         {"--content", "base", "--scenario", "trading", "--overlay"},
	         {"--scenario", "trading", "--content"}})
	{
		CAPTURE(refused.size());
		std::vector<std::string_view> ignored;
		CHECK_FALSE(parse(refused, ignored));
	}
}


TEST_CASE("The validator loads what the host loads, the scenario selected in its identity")
{
	for (const std::filesystem::path content : {MANIFEST_MOD_DIRECTORY, MANIFEST_MOD_PACK})
	{
		CAPTURE(content);
		const Manifest::Content::ContentSet validated = Manifest::Host::Validate(OptionsFor(content));
		CHECK(validated.scenario == "trading");
		const auto session = Manifest::Host::Start(OptionsFor(content));
		CHECK(
		    validated.identity.manifest ==
		    Manifest::Content::BuildContentIdentity(session->Definitions(), "trading").manifest);
	}

	Manifest::Host::Options governance = OptionsFor(MANIFEST_MOD_DIRECTORY);
	governance.scenario = "governance";
	CHECK(
	    Manifest::Host::Validate(governance).identity.manifest !=
	    Manifest::Host::Validate(OptionsFor(MANIFEST_MOD_DIRECTORY)).identity.manifest);

	governance.scenario = "elsewhere";
	CheckFails(governance, Phase::Content, MANIFEST_MOD_DIRECTORY, "there is no scenario 'elsewhere'");
}


TEST_CASE("Overlays mount over the package in order, each by its mod.yaml name")
{
	const std::filesystem::path overlays = MANIFEST_HOST_OVERLAYS;
	const Manifest::Host::Options options = OptionsFor(MANIFEST_MOD_DIRECTORY, {overlays / "copper"});
	const auto session = Manifest::Host::Start(options);
	REQUIRE(session->Files().Mounts().size() == 2);
	CHECK(session->Files().Mounts()[0].name == "manifest");
	CHECK(session->Files().Mounts()[1].name == "Copper");
	const Manifest::Content::ContentSet validated = Manifest::Host::Validate(options);
	CHECK(
	    validated.identity.manifest ==
	    Manifest::Content::BuildContentIdentity(session->Definitions(), "trading").manifest);
	CHECK(
	    validated.identity.manifest !=
	    Manifest::Host::Validate(OptionsFor(MANIFEST_MOD_DIRECTORY)).identity.manifest);

	// A rule an overlay breaks names the overlay among the layers read.
	CheckFails(
	    OptionsFor(MANIFEST_MOD_DIRECTORY, {overlays / "blank-age"}), Phase::Content,
	    MANIFEST_MOD_DIRECTORY,
	    "age-stone.yaml: Manifest.Age 'AgeStone': displayName is required (from manifest, BlankAge "
	    "(patch))");
	CheckFails(
	    OptionsFor(MANIFEST_MOD_DIRECTORY, {overlays / "copper", overlays / "copper"}), Phase::Mount,
	    overlays / "copper", "'Copper' is already mounted");

	const Content bare;
	bare.Remove("mod.yaml");
	CheckFails(
	    OptionsFor(MANIFEST_MOD_DIRECTORY, {bare.Directory()}), Phase::Mount, bare.Directory(),
	    "an overlay has no mod.yaml to name it");

	// An overlay's requires names the mounts before it.
	const Content needy;
	needy.Put("mod.yaml", "version: 1\nname: Needy\nrequires: [Copper]\n");
	CheckFails(
	    OptionsFor(MANIFEST_MOD_DIRECTORY, {needy.Directory(), overlays / "copper"}), Phase::Content,
	    needy.Directory(), "requires the package 'Copper'");
}
