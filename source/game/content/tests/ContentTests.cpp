#include "TestHelpers.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <jettison/reflect/Reflect.h>
#include <jettison/serialise/Archive.h>
#include <jettison/sim/DefinitionRegistry.h>
#include <jettison/vfs/FileSystem.h>

#include <manifest/config/SimulationConfig.h>
#include <manifest/content/Content.h>
#include <manifest/content/ContentIdentity.h>
#include <manifest/content/Definitions.h>

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


template <typename T> const T &Get(const Loaded &loaded, std::string_view id)
{
	const T *pDefinition = loaded.registry.Find<T>(RegistryName(id));
	REQUIRE_MESSAGE(pDefinition != nullptr, id);
	return *pDefinition;
}


// The content set over the staged package, with the overlay mounted above it.
ContentSet LoadWith(const Overlay &overlay, std::string_view mode = "full")
{
	const std::array<Jettison::Vfs::Mount, 2> sources{
	    {{"base", kModDirectory}, {"overlay", overlay.Root()}}};
	return LoadContent(sources, mode);
}


// The overlay's one file breaks a rule, and the content set is refused, saying which.
void CheckRefused(std::string_view path, std::string_view text, std::string_view expected)
{
	CAPTURE(path);
	CAPTURE(text);
	const Overlay overlay;
	overlay.Write(std::string{path}, text);
	try
	{
		(void)LoadWith(overlay);
		FAIL("loaded content that should be refused with ", expected);
	}
	catch (const ContentError &error)
	{
		MANIFEST_CHECK_CONTAINS(std::string{error.what()}, expected);
	}
}


// A format of two versions, standing in for a production type's first change: version 2 renamed
// width to size.
struct Widget
{
	std::string widgetId;
	int size{0};
};


DefinitionType WidgetType()
{
	using namespace Jettison::Reflect;
	static const bool registered = []
	{
		RegisterType<Widget>("Manifest.Test.Widget", Attribute::Moddable)
		    .Field<&Widget::widgetId>("widgetId", Attribute::Saved | Attribute::Moddable)
		    .Field<&Widget::size>("size", Attribute::Saved | Attribute::Moddable);
		return true;
	}();
	(void)registered;

	Jettison::Serialise::YamlMigrations chain{"Manifest.Test.Widget", {2}};
	chain.Add(
	    1,
	    [](YAML::Node &node)
	    {
		    // Only what the document has: a patch that leaves width out must not gain a size.
		    const YAML::Node width = std::as_const(node)["width"];
		    if (!width)
			    return;
		    node["size"] = width;
		    node.remove("width");
	    });
	return {
	    std::move(chain), "widgets", entt::resolve<Widget>(),
	    [](const entt::meta_any &widget) -> std::string_view
	    { return widget.cast<const Widget &>().widgetId; }};
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


TEST_CASE(
    "The content set holds each mode in its order, with the defaults one field takes from another")
{
	const Loaded loaded{kModDirectory};
	for (const std::string_view mode : {"full", "simple"})
	{
		CAPTURE(mode);
		const Overlay none;
		const ContentSet content = LoadWith(none, mode);
		const GameModeDefinition &record = *loaded.registry.Find<GameModeDefinition>(mode);

		REQUIRE(content.ages.size() == record.ages.size());
		for (std::size_t index = 0; index < record.ages.size(); ++index)
			CHECK(content.ages[index].ageId == record.ages[index].ageId);
		REQUIRE(content.gameMode.items.size() == record.items.size());
		for (std::size_t index = 0; index < record.items.size(); ++index)
			CHECK(content.gameMode.items[index].itemId == record.items[index].itemId);
		REQUIRE(content.gameMode.skills.size() == record.skills.size());
		for (std::size_t index = 0; index < record.skills.size(); ++index)
		{
			const Manifest::Core::SkillNode &skill = content.gameMode.skills[index];
			CHECK(skill.skillId == record.skills[index].skillId);
			const std::string &authored =
			    Get<Manifest::Core::SkillNode>(loaded, skill.skillId).displayName;
			CHECK(skill.displayName == (authored.empty() ? skill.skillId : authored));
		}
	}

	const Overlay none;
	const ContentSet content = LoadWith(none);
	CHECK(content.ages.front().ageId == "AgeStone");
	CHECK(content.categories.size() == loaded.registry.Names<Manifest::Core::CategoryNode>().size());
	for (const Manifest::Core::CategoryNode &category : content.categories)
	{
		CAPTURE(category.categoryId);
		const auto &authored = Get<Manifest::Core::CategoryNode>(loaded, category.categoryId);
		CHECK(category.type == (authored.type.empty() ? authored.categoryId : authored.type));
		CHECK(
		    category.description ==
		    (authored.description.empty() ? authored.displayName : authored.description));
	}
	CHECK(content.names.PoolNames() == loaded.registry.Names<NamePoolDefinition>());
	CHECK(content.names.Find("latin") == Get<NamePoolDefinition>(loaded, "latin").names);
	CHECK(content.identity.manifest == BuildContentIdentity(loaded.registry).manifest);

	// The full mode is every record: the ones no mode selects are counted here too.
	CHECK(loaded.registry.Names<Manifest::Core::ItemNode>().size() == content.gameMode.items.size());
	CHECK(loaded.registry.Names<Manifest::Core::SkillNode>().size() == content.gameMode.skills.size());
	CHECK(loaded.registry.Names<Manifest::Core::AgeNode>().size() == content.ages.size());
	CHECK(loaded.registry.Names<GameModeDefinition>().size() == 2);
}


TEST_CASE("Each rule the content breaks is refused, naming the record, the field and its layers")
{
	const std::string_view ages = "manifest/definitions/ages/";
	const std::string_view items = "manifest/definitions/items/";
	const std::string_view skills = "manifest/definitions/skills/";
	const auto patch = [](std::string_view directory, std::string_view name)
	{ return std::format("{}{}.patch.yaml", directory, name); };

	CheckRefused(
	    patch(ages, "age-stone"), "version: 1\ndisplayName: ''\n",
	    "manifest/definitions/ages/age-stone.yaml: Manifest.Age 'AgeStone': displayName is required "
	    "(from base, overlay (patch))");
	CheckRefused(
	    patch("manifest/definitions/categories/", "category-metals"), "version: 1\ndisplayName: ''\n",
	    "Manifest.Category 'CategoryMetals': displayName is required");

	const std::string copper = patch(items, "item-copper");
	CheckRefused(
	    copper, "version: 1\ncategoryId: CategoryNowhere\n",
	    "categoryId 'CategoryNowhere' names no category");
	CheckRefused(copper, "version: 1\nageId: AgeNowhere\n", "ageId 'AgeNowhere' names no age");
	CheckRefused(
	    copper, "version: 1\nunitType: ~\n", "Manifest.Item 'ItemCopper': unitType is required");
	CheckRefused(copper, "version: 1\nunitsPerCubicMetre: 0\n", "unitsPerCubicMetre must be above 0");
	CheckRefused(copper, "version: 1\ndensity: -1\n", "density must be above 0");
	CheckRefused(copper, "version: 1\npackFactor: 1.5\n", "packFactor must be in (0, 1]");
	CheckRefused(copper, "version: 1\npackFactor: 0\n", "packFactor must be in (0, 1]");
	for (const std::string_view price : {"ten", "-1", "1e3", "999999999999999"})
		CheckRefused(
		    copper, std::format("version: 1\nbasePrice: \"{}\"\n", price),
		    std::format("basePrice '{}' is not a nonnegative price", price));

	const std::string mining = patch(skills, "skill-mining");
	// Nothing requires publishing, so its tiers are its own rule alone.
	CheckRefused(
	    patch(skills, "skill-publishing"), "version: 1\nmaxTiers: 0\n", "maxTiers must be at least 1");
	CheckRefused(mining, "version: 1\ngridX: .inf\n", "gridX must be finite");
	CheckRefused(
	    mining, "version: 1\nresourceCostPerTier: 0\n", "resourceCostPerTier must be at least 1");
	CheckRefused(
	    mining, "version: 1\nnodeType: Tech\n", "nodeType is Tech, and the skillId names no age");
	CheckRefused(
	    mining, "version: 1\ntargetEffectID: Teleport\n", "targetEffectID 'Teleport' names no effect");
	CheckRefused(
	    mining, "version: 1\nrequires: [{nodeId: SkillNowhere}]\n",
	    "requires 'SkillNowhere' names no skill");
	CheckRefused(
	    patch(skills, "skill-smelting"),
	    "version: 1\nrequires: [{nodeId: SkillMining, requiredTier: 9}]\n",
	    "requires 'SkillMining' asks for tier 9, of 3");
	CheckRefused(
	    mining, "version: 1\nrequires: [{nodeId: SkillMining}]\n",
	    "manifest/definitions/game-modes/full.yaml: Manifest.GameMode 'full': skills ");

	const std::string latin = patch("manifest/definitions/name-pools/", "latin");
	CheckRefused(latin, "version: 1\nnames: []\n", "Manifest.NamePool 'latin': names holds none");
	CheckRefused(latin, "version: 1\nnames: [Aaron, '']\n", "names holds an empty name");
	CheckRefused(
	    "manifest/definitions/name-pools/coptic.yaml", "version: 1\npool: Coptic\nnames: [Abba]\n",
	    "Manifest.NamePool 'Coptic': pool must be the record's name");

	const std::string simple = patch("manifest/definitions/game-modes/", "simple");
	CheckRefused(
	    simple, "version: 1\nitems: !append [{itemId: ItemNowhere}]\n",
	    "Manifest.GameMode 'simple': items 'ItemNowhere' names nothing");
	CheckRefused(
	    simple, "version: 1\nitems: !append [{itemId: ItemCopper}]\n",
	    "items selects 'ItemCopper' twice");
	CheckRefused(
	    simple, "version: 1\nages: [{ageId: AgeNowhere}]\n", "ages 'AgeNowhere' names nothing");
	CheckRefused(
	    simple, "version: 1\nskills: [{skillId: AgeStone}, {skillId: AgeStone}]\n",
	    "skills selects 'AgeStone' twice");

	const Overlay none;
	CHECK_THROWS_WITH_AS(
	    (void)LoadWith(none, "hard"), "manifest/definitions/game-modes: there is no game mode 'hard'",
	    ContentError);
}


TEST_CASE("A record that does not read is refused by the registry: a version, a field, an orphan patch")
{
	const auto refused = [](std::string_view path, std::string_view text)
	{
		const Overlay overlay;
		overlay.Write(std::string{path}, text);
		CHECK_THROWS_AS((void)LoadWith(overlay), FormatError);
	};
	refused(
	    "manifest/definitions/ages/age-stone.yaml",
	    "version: 2\nageId: AgeStone\ndisplayName: Stone\n");
	refused("manifest/definitions/ages/age-stone.patch.yaml", "version: 1\ncolour: grey\n");
	refused("manifest/definitions/ages/age-golden.patch.yaml", "version: 1\ndisplayName: Golden\n");
}


TEST_CASE("A later layer's whole file replaces a record, and a patch merges into it")
{
	const Overlay overlay;
	overlay.Write(
	    "manifest/definitions/ages/age-stone.yaml",
	    "version: 1\nageId: AgeStone\ndisplayName: Flint\n");
	overlay.Write(
	    "manifest/definitions/items/item-copper.patch.yaml", "version: 1\nbasePrice: \"12.25\"\n");
	overlay.Write(
	    "manifest/definitions/game-modes/simple.patch.yaml",
	    "version: 1\nitems: !append [{itemId: ItemAluminium}]\n");
	const ContentSet full = LoadWith(overlay);
	CHECK(full.ages.front().displayName == "Flint");
	CHECK(full.ages.front().description.empty());
	const auto copper = std::find_if(
	    full.gameMode.items.begin(), full.gameMode.items.end(),
	    [](const Manifest::Core::ItemNode &item) { return item.itemId == "ItemCopper"; });
	REQUIRE(copper != full.gameMode.items.end());
	CHECK(copper->basePrice == "12.25");
	CHECK(copper->displayName == "Copper");

	const ContentSet simple = LoadWith(overlay, "simple");
	CHECK(simple.gameMode.items.back().itemId == "ItemAluminium");
}


TEST_CASE("A type's migrations bring an earlier file, and an earlier patch, to its version")
{
	const DefinitionType widgets = WidgetType();
	const Overlay overlay;
	overlay.Write("widgets/small.yaml", "version: 1\nwidgetId: small\nwidth: 3\n");
	overlay.Write("widgets/large.yaml", "version: 2\nwidgetId: large\nsize: 8\n");
	overlay.Write("widgets/large.patch.yaml", "version: 1\nwidth: 9\n");
	overlay.Write("widgets/plain.yaml", "version: 2\nwidgetId: plain\nsize: 5\n");
	overlay.Write("widgets/plain.patch.yaml", "version: 1\nwidgetId: plain\n");

	Jettison::Vfs::FileSystem files;
	files.AddMount({"test", overlay.Root()});
	Jettison::Sim::DefinitionRegistry registry{files};
	LoadDefinitions(registry, {&widgets, 1});
	CHECK(registry.Find<Widget>("small")->size == 3);
	CHECK(registry.Find<Widget>("large")->size == 9);
	CHECK(registry.Find<Widget>("plain")->size == 5);

	overlay.Write("widgets/future.yaml", "version: 3\nwidgetId: future\nsize: 1\n");
	CHECK_THROWS_AS(LoadDefinitions(registry, {&widgets, 1}), FormatError);
	CHECK(registry.Find<Widget>("small")->size == 3);
}


TEST_CASE("A price keeps its authored text")
{
	const Loaded loaded{kModDirectory};
	// Authored unquoted as 10.0: a float on the way would have made it "10".
	CHECK(Get<Manifest::Core::ItemNode>(loaded, "ItemCopper").basePrice == "10.0");
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
			const Jettison::Serialise::FormatVersion version = definition.migrations.Current();
			const std::string text = Jettison::Serialise::YamlArchive::Write(record, version);
			const entt::meta_any read =
			    Jettison::Serialise::YamlArchive::Read(text, definition.type, version);
			CHECK(
			    Jettison::Serialise::BinaryArchive::Write(read, version) ==
			    Jettison::Serialise::BinaryArchive::Write(record, version));
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
		const auto &mining = Get<Manifest::Core::SkillNode>(loaded, "SkillMining");
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
	const auto &mining = Get<Manifest::Core::SkillNode>(loaded, "SkillMining");
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
