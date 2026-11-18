#include "TestHelpers.h"

#include <atomic>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/config/SimulationConfig.h>
#include <manifest/content/ContentIdentity.h>
#include <manifest/content/Definitions.h>
#include <manifest/universe/Universe.h>

using namespace Manifest::Content;
using Jettison::Serialise::FormatError;

namespace
{
const std::filesystem::path kModDirectory{MANIFEST_MOD_DIRECTORY};


// A registry over the staged content, with any overlay mounted above it. The file system outlives
// the registry, which reads through it.
struct Loaded
{
	explicit Loaded(const std::filesystem::path &content, const std::filesystem::path &overlay = {})
	{
		files.AddMount({"base", content});
		if (!overlay.empty())
			files.AddMount({"overlay", overlay});
		LoadDefinitions(registry);
	}

	Jettison::Vfs::FileSystem files;
	Jettison::Sim::DefinitionRegistry registry{files};
};


// A directory of files for a case to mount over the staged content.
class Overlay
{
  public:
	Overlay()
	{
		static std::atomic<int> count{0};
		m_root = std::filesystem::temp_directory_path() /
		         std::format("manifest-content-tests-{}-{}", getpid(), count++);
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
	}


	~Overlay()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_root, ignored);
	}


	Overlay(const Overlay &) = delete;


	Overlay &operator=(const Overlay &) = delete;


	void Write(const std::string &virtualPath, std::string_view text) const
	{
		const std::filesystem::path path = m_root / virtualPath;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream{path} << text;
	}


	const std::filesystem::path &Root() const
	{
		return m_root;
	}

  private:
	std::filesystem::path m_root;
};


std::string LegacyPath(std::string_view file)
{
	return (kModDirectory / "manifest" / "core-config" / file).string();
}


// The legacy loader keeps numbers as their text, which is parsed here as the engine's reader parses
// it: correctly rounded, so the two agree to the bit.
double Parse(const std::string &text)
{
	double value = 0.0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	REQUIRE(error == std::errc{});
	REQUIRE(end == text.data() + text.size());
	return value;
}


template <typename T> const T &Get(const Loaded &loaded, std::string_view id)
{
	const T *pDefinition = loaded.registry.Find<T>(RegistryName(id));
	REQUIRE_MESSAGE(pDefinition != nullptr, id);
	return *pDefinition;
}


void CheckItem(const ItemDefinition &item, const Manifest::Core::ItemNode &legacy)
{
	CHECK(item.itemId == legacy.itemId);
	CHECK(item.categoryId == legacy.categoryId);
	CHECK(item.ageId == legacy.ageId);
	CHECK(item.displayName == legacy.displayName);
	CHECK(item.description == legacy.description);
	CHECK(item.basePrice == legacy.basePrice);
	REQUIRE(item.unitType.has_value());
	CHECK(Jettison::Reflect::EnumName(entt::forward_as_meta(*item.unitType)) == legacy.unitType);
	CHECK(item.unitsPerCubicMetre == Parse(legacy.unitsPerCubicMetre));
	CHECK(item.density == Parse(legacy.density));
	CHECK(item.packFactor == Parse(legacy.packFactor));
}


void CheckSkill(const SkillDefinition &skill, const Manifest::Core::SkillNode &legacy)
{
	CHECK(skill.skillId == legacy.skillId);
	CHECK(skill.displayName == legacy.displayName);
	CHECK(skill.description == legacy.description);
	CHECK(skill.nodeType == legacy.nodeType);
	CHECK(skill.maxTiers == legacy.maxTiers);
	CHECK(skill.resourceCostPerTier == legacy.resourceCostPerTier);
	CHECK(skill.targetEffectID == legacy.targetEffectID);
	CHECK(skill.exclusiveGroupId == legacy.exclusiveGroupId);
	CHECK(skill.gridX == legacy.gridX);
	CHECK(skill.gridY == legacy.gridY);
	REQUIRE(skill.requiredSkills.size() == legacy.requiredSkills.size());
	for (std::size_t index = 0; index < skill.requiredSkills.size(); ++index)
	{
		CHECK(skill.requiredSkills[index].nodeId == legacy.requiredSkills[index].nodeId);
		CHECK(skill.requiredSkills[index].requiredTier == legacy.requiredSkills[index].requiredTier);
	}
}
} // namespace


TEST_CASE("A record's name is its ID in kebab case, as data/build.sh names the file")
{
	CHECK(RegistryName("ItemCopper") == "item-copper");
	CHECK(RegistryName("ItemLPG") == "item-lpg");
	CHECK(RegistryName("ItemAbcLPGGas") == "item-abc-lpg-gas");
	CHECK(RegistryName("AgeAI") == "age-ai");
	CHECK(RegistryName("AgePre-industrial") == "age-pre-industrial");
	CHECK(RegistryName("Item2Bc") == "item2-bc");
	CHECK(RegistryName("latin") == "latin");
}


TEST_CASE("The records reproduce both game modes the legacy loader reads, in order")
{
	const Loaded loaded{kModDirectory};
	Manifest::Core::Ages legacyAges;
	std::string error;
	REQUIRE(Manifest::Core::LoadConfigAges(LegacyPath("ages.yaml"), legacyAges, error));

	for (const std::string_view mode : {"full", "simple"})
	{
		CAPTURE(mode);
		Manifest::Core::GameModeNode legacy;
		REQUIRE(
		    Manifest::Core::LoadConfigGameMode(
		        LegacyPath(std::format("{}.yaml", mode)), legacy, error));
		const auto *pMode = loaded.registry.Find<GameModeDefinition>(mode);
		REQUIRE(pMode != nullptr);

		REQUIRE(pMode->ages.size() == legacyAges.size());
		for (std::size_t index = 0; index < legacyAges.size(); ++index)
		{
			const auto &age = Get<AgeDefinition>(loaded, pMode->ages[index].ageId);
			CHECK(age.ageId == legacyAges[index].ageId);
			CHECK(age.displayName == legacyAges[index].displayName);
			CHECK(age.description == legacyAges[index].description);
		}

		REQUIRE(pMode->items.size() == legacy.items.size());
		for (std::size_t index = 0; index < legacy.items.size(); ++index)
			CheckItem(Get<ItemDefinition>(loaded, pMode->items[index].itemId), legacy.items[index]);

		REQUIRE(pMode->skills.size() == legacy.skills.size());
		for (std::size_t index = 0; index < legacy.skills.size(); ++index)
			CheckSkill(
			    Get<SkillDefinition>(loaded, pMode->skills[index].skillId), legacy.skills[index]);
	}

	// The full mode is every record: the ones no mode selects are counted here too.
	const GameModeDefinition &full = *loaded.registry.Find<GameModeDefinition>("full");
	CHECK(loaded.registry.Names<ItemDefinition>().size() == full.items.size());
	CHECK(loaded.registry.Names<SkillDefinition>().size() == full.skills.size());
	CHECK(loaded.registry.Names<AgeDefinition>().size() == full.ages.size());
	CHECK(loaded.registry.Names<GameModeDefinition>().size() == 2);
}


TEST_CASE("Categories and name pools are those the legacy loaders read")
{
	const Loaded loaded{kModDirectory};
	std::string error;

	Manifest::Core::Categories legacyCategories;
	REQUIRE(
	    Manifest::Core::LoadConfigCategories(LegacyPath("categories.yaml"), legacyCategories, error));
	CHECK(loaded.registry.Names<CategoryDefinition>().size() == legacyCategories.size());
	for (const Manifest::Core::CategoryNode &legacy : legacyCategories)
	{
		const auto &category = Get<CategoryDefinition>(loaded, legacy.categoryId);
		CHECK(category.categoryId == legacy.categoryId);
		CHECK(category.displayName == legacy.displayName);
		CHECK(category.type == legacy.type);
		CHECK(category.description == legacy.description);
	}

	Manifest::Core::UniverseNamePools legacyPools;
	REQUIRE(
	    Manifest::Core::LoadUniverseNamePools(
	        (kModDirectory / "manifest" / "universe" / "solar-system-names.yaml").string(), legacyPools,
	        error));
	CHECK(loaded.registry.Names<NamePoolDefinition>() == legacyPools.PoolNames());
	for (const std::string &pool : legacyPools.PoolNames())
	{
		const auto &definition = Get<NamePoolDefinition>(loaded, pool);
		CHECK(definition.pool == pool);
		CHECK(definition.names == legacyPools.Find(pool));
	}
}


TEST_CASE("A price keeps its authored text")
{
	const Loaded loaded{kModDirectory};
	// Authored unquoted as 10.0: a float on the way would have made it "10".
	CHECK(Get<ItemDefinition>(loaded, "ItemCopper").basePrice == "10.0");
}


TEST_CASE("Every record reads back as it is written")
{
	const Loaded loaded{kModDirectory};
	for (const DefinitionType &definition : DefinitionTypes())
	{
		for (const std::string &name : loaded.registry.Names(definition.type))
		{
			CAPTURE(name);
			const entt::meta_any &record = *loaded.registry.Find(definition.type, name);
			const std::string text =
			    Jettison::Serialise::YamlArchive::Write(record, definition.version);
			const entt::meta_any read =
			    Jettison::Serialise::YamlArchive::Read(text, definition.type, definition.version);
			CHECK(
			    Jettison::Serialise::BinaryArchive::Write(read, definition.version) ==
			    Jettison::Serialise::BinaryArchive::Write(record, definition.version));
		}
	}
}


TEST_CASE("A record under a name its ID does not give is refused")
{
	const Overlay overlay;
	overlay.Write(
	    "manifest/definitions/ages/age-golden.yaml",
	    "version: 1\nageId: AgeStone\ndisplayName: Golden Age\n");
	CHECK_THROWS_AS(Loaded(kModDirectory, overlay.Root()), FormatError);
}


TEST_CASE("A patch's null gives the field its default, and a whole file's null is refused")
{
	{
		const Overlay overlay;
		overlay.Write(
		    "manifest/definitions/skills/skill-mining.patch.yaml", "version: 1\ndescription: ~\n");
		const Loaded loaded{kModDirectory, overlay.Root()};
		const auto &mining = Get<SkillDefinition>(loaded, "SkillMining");
		CHECK(mining.description.empty());
		CHECK(mining.maxTiers == 3);
	}
	{
		const Overlay overlay;
		overlay.Write(
		    "manifest/definitions/skills/skill-mining.yaml",
		    "version: 1\nskillId: SkillMining\ntargetEffectID: ~\n");
		CHECK_THROWS_AS(Loaded(kModDirectory, overlay.Root()), FormatError);
	}
}


TEST_CASE("A field a record leaves out keeps the schema's default")
{
	const Overlay overlay;
	overlay.Write(
	    "manifest/definitions/skills/skill-mining.yaml", "version: 1\nskillId: SkillMining\n");
	const Loaded loaded{kModDirectory, overlay.Root()};
	const auto &mining = Get<SkillDefinition>(loaded, "SkillMining");
	CHECK(mining.nodeType == Manifest::Core::NodeType::Skill);
	CHECK(mining.maxTiers == 1);
	CHECK(mining.resourceCostPerTier == 1);
	CHECK(mining.requiredSkills.empty());
	CHECK(mining.targetEffectID.empty());
	CHECK(mining.gridX == 0.0f);
}


TEST_CASE("The directory and the pack give the same manifest, and so the same fingerprint")
{
	const ContentIdentity directory = BuildContentIdentity(Loaded{kModDirectory}.registry);
	const ContentIdentity pack = BuildContentIdentity(Loaded{MANIFEST_MOD_PACK}.registry);
	CHECK(directory.manifest == pack.manifest);
	CHECK(directory.fingerprint == pack.fingerprint);
	CHECK(directory.fingerprint != 0);
	CHECK(BuildContentIdentity(Loaded{kModDirectory}.registry).manifest == directory.manifest);
}


TEST_CASE("A changed price, a reordered selection or an unselected definition changes the manifest")
{
	const ContentIdentity base = BuildContentIdentity(Loaded{kModDirectory}.registry);
	const auto identity = [](std::string_view path, std::string_view text)
	{
		const Overlay overlay;
		overlay.Write(std::string{path}, text);
		return BuildContentIdentity(Loaded{kModDirectory, overlay.Root()}.registry);
	};

	const ContentIdentity price = identity(
	    "manifest/definitions/items/item-copper.patch.yaml", "version: 1\nbasePrice: \"10.5\"\n");
	CHECK(price.manifest != base.manifest);
	CHECK(price.fingerprint != base.fingerprint);

	const ContentIdentity order = identity(
	    "manifest/definitions/game-modes/simple.patch.yaml",
	    "version: 1\nitems: [{itemId: ItemSilver}, {itemId: ItemGold}]\n");
	CHECK(order.manifest != base.manifest);
	CHECK(
	    order.manifest != identity(
	                          "manifest/definitions/game-modes/simple.patch.yaml",
	                          "version: 1\nitems: [{itemId: ItemGold}, {itemId: ItemSilver}]\n")
	                          .manifest);

	const ContentIdentity added = identity(
	    "manifest/definitions/ages/age-golden.yaml",
	    "version: 1\nageId: AgeGolden\ndisplayName: Golden\n");
	CHECK(added.manifest != base.manifest);
}
