#include "TestHelpers.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <jettison/dock/Definitions.h>
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
ContentSet LoadWith(const Overlay &overlay, std::string_view mode = "manifest")
{
	const std::array<Jettison::Vfs::Mount, 2> sources{
	    {{"base", kModDirectory}, {"overlay", overlay.Root()}}};
	return LoadContent(sources, mode);
}


// The overlay's files break a rule, and the content set is refused, saying which.
void CheckRefused(
    std::initializer_list<std::pair<std::string_view, std::string_view>> files,
    std::string_view expected)
{
	CAPTURE(expected);
	const Overlay overlay;
	for (const auto &[path, text] : files)
	{
		CAPTURE(path);
		CAPTURE(text);
		overlay.Write(std::string{path}, text);
	}
	try
	{
		(void)LoadWith(overlay);
		FAIL("loaded content that should be refused with ", expected);
	}
	catch (const ContentError &error)
	{
		INFO(std::string{error.what()});
		MANIFEST_CHECK_CONTAINS(std::string{error.what()}, expected);
	}
}


void CheckRefused(std::string_view path, std::string_view text, std::string_view expected)
{
	CheckRefused({{path, text}}, expected);
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
	for (const std::string_view mode : {"manifest", "astralis"})
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
	CHECK(content.identity.manifest == BuildContentIdentity(loaded.registry, {}).manifest);

	// The Manifest mode is every record: the ones no mode selects are counted here too.
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

	const std::string starter = patch("manifest/definitions/hulls/", "hull-lone-trader-starter");
	CheckRefused(
	    starter, "version: 1\nclassName: ''\n",
	    "Manifest.Hull 'HullLoneTraderStarter': className is required");
	CheckRefused(starter, "version: 1\nbaseStats: ~\n", "baseStats holds none");
	CheckRefused(
	    starter, "version: 1\nbaseStats: {CargoVolume: -1}\n",
	    "baseStats Cargo Volume must be finite and not below 0");
	// A hold is whole totes, none bigger than the largest hull class's envelope.
	for (const std::string_view volume : {"18446744073709551616", "11520001", "20.0625"})
		CheckRefused(
		    starter, std::format("version: 1\nbaseStats: {{CargoVolume: {}}}\n", volume),
		    "CargoVolume must be a whole number of totes up to 11520000 IBCs");
	CheckRefused(
	    starter, "version: 1\nfitableEffects: [Teleport]\n",
	    "fitableEffects 'Teleport' names no effect");
	CheckRefused(starter, "version: 1\noccupants: 0\n", "occupants must be at least 1");

	// Jettison's ship and station records, checked as the engine checks them (D-062).
	CheckRefused(
	    "manifest/definitions/dock-hulls/hull-lone-trader-starter.patch.yaml",
	    "version: 2\nlength: 12\n",
	    "Dock.HullDefinition 'hull-lone-trader-starter': dock Hull 'hull-lone-trader-starter' is 12 x "
	    "6.4 x 1.8 m, larger than its pad's largest hull, 9.8 x 6.4 x 1.8 m");
	CheckRefused(
	    "manifest/definitions/pads/skiff.patch.yaml", "version: 1\npadSize: [4, 10]\n",
	    "Dock.PadClassDefinition 'skiff': dock Pad class 'skiff' has a largest hull");
	CheckRefused(
	    "manifest/definitions/stations/highport.patch.yaml", "version: 1\nbays: []\npiers: []\n",
	    "Dock.StationLayout 'highport': dock Station layout 'highport'");
	CheckRefused(
	    "manifest/definitions/dock-hulls/hull-nowhere.yaml",
	    "version: 2\ndisplayName: Nowhere\nlength: 5\nbeam: 3\nheight: 1\npad: skiff\n",
	    "Dock.HullDefinition 'hull-nowhere': dock is the dock hull of no Manifest.Hull of its name");
	CheckRefused(
	    "manifest/definitions/hulls/hull-extra.yaml",
	    "version: 1\nhullId: HullExtra\nclassName: Extra\nbaseStats: {CargoVolume: 1}\n",
	    "Manifest.Hull 'HullExtra': dock has no Dock.HullDefinition 'hull-extra'");
	CheckRefused(
	    starter, "version: 1\nscrubberCartridges: 0\n", "scrubberCartridges must be at least 1");
	CheckRefused(starter, "version: 1\noxygenTanks: []\n", "oxygenTanks holds none");
	CheckRefused(
	    starter, "version: 1\noxygenTanks: [10, 0]\n", "oxygenTanks a capacity must be above 0");

	const std::string balance = "manifest/definitions/tuning/balance.patch.yaml";
	CheckRefused(
	    balance, "version: 1\neconomy: {intervalSeconds: 0}\n",
	    "Manifest.Tuning 'balance': economy.intervalSeconds must be above 0");
	CheckRefused(
	    balance, "version: 1\neconomy: {wageTaxRate: 1}\n", "economy.wageTaxRate must be in [0, 1)");
	CheckRefused(
	    balance, "version: 1\ncompany: {startingCredits: 0}\n",
	    "company.startingCredits must be above 0");
	CheckRefused(
	    balance, "version: 1\nmarket: {listingFeeBps: 10001}\n",
	    "market.listingFeeBps must be in [0, 10000]");
	CheckRefused(
	    balance, "version: 1\nmarket: {orderLifeSeconds: -1}\n",
	    "market.orderLifeSeconds must be finite and not below 0");
	CheckRefused(
	    balance, "version: 1\nbrokers: {treasury: -1}\n", "brokers.treasury must not be below 0");
	CheckRefused(
	    balance, "version: 1\nbrokers: {spreadBps: 20000}\n",
	    "brokers.spreadBps must be in [0, 20000)");
	CheckRefused(balance, "version: 1\nneeds: []\n", "needs holds none");
	CheckRefused(
	    balance, "version: 1\nneeds: !merge=needId\n  - {needId: NeedStapleFood, weight: 0}\n",
	    "'NeedStapleFood' weight must be above 0");
	CheckRefused(
	    balance,
	    "version: 1\nneeds: !merge=needId\n  - {needId: NeedDrinkingWater, items: [{itemId: "
	    "ItemRice}]}\n",
	    "'NeedDrinkingWater' item 'ItemRice' is not traded in the need's unit");
	CheckRefused(
	    balance,
	    "version: 1\nneeds: !merge=needId\n  - {needId: NeedStapleFood, items: [{itemId: ItemRice}, "
	    "{itemId: ItemRice}]}\n",
	    "'NeedStapleFood' selects 'ItemRice' twice");
	CheckRefused(
	    balance, "version: 1\ncondition: {initial: 101}\n", "condition.initial must be in [0, 100]");
	CheckRefused(
	    balance, "version: 1\ncondition: {workRateFloor: 2}\n",
	    "condition.workRateFloor must be in [0, 1]");
	CheckRefused(
	    balance, "version: 1\nprocurement: {performanceSecurityRate: -0.1}\n",
	    "procurement.performanceSecurityRate must be in [0, 1]");
	CheckRefused(
	    balance,
	    "version: 1\nprojects: !merge=projectId\n  - {projectId: ProjectBronzeMetallurgy, "
	    "unlocksRecipe: RecipeGold}\n",
	    "'ProjectBronzeMetallurgy' unlocks 'RecipeGold', which names no recipe");
	CheckRefused(
	    balance,
	    "version: 1\nrecipes: !merge=recipeId\n  - {recipeId: RecipeBronze, outputs: [{itemId: "
	    "ItemBronze, quantity: 0}]}\n",
	    "'ItemBronze' must be a quantity above 0");

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
	// Two skills no mode selects are still one graph.
	CheckRefused(
	    {{"manifest/definitions/skills/skill-review-a.yaml",
	      "version: 1\nskillId: SkillReviewA\nrequires: [{nodeId: SkillReviewB}]\n"},
	     {"manifest/definitions/skills/skill-review-b.yaml",
	      "version: 1\nskillId: SkillReviewB\nrequires: [{nodeId: SkillReviewA}]\n"}},
	    "manifest/definitions/skills/skill-review-a.yaml: Manifest.Skill 'SkillReviewA': requires "
	    "prerequisite cycle");
	CheckRefused(
	    mining, "version: 1\nrequires: [{nodeId: SkillMining}]\n",
	    "manifest/definitions/skills/skill-mining.yaml: Manifest.Skill 'SkillMining': requires skill "
	    "'SkillMining' requires itself (from base, overlay (patch))");
	// The whole graph holds, but the mode leaves a prerequisite out.
	CheckRefused(
	    patch("manifest/definitions/game-modes/", "astralis"),
	    "version: 1\nskills: [{skillId: AgeCopper}]\n",
	    "manifest/definitions/game-modes/astralis.yaml: Manifest.GameMode 'astralis': skills ");

	const std::string latin = patch("manifest/definitions/name-pools/", "latin");
	CheckRefused(latin, "version: 1\nnames: []\n", "Manifest.NamePool 'latin': names holds none");
	CheckRefused(latin, "version: 1\nnames: [Aaron, '']\n", "names holds an empty name");
	// A file named ".yaml" is a record whose name is empty, and so is its ID if it gives none.
	CheckRefused(
	    "manifest/definitions/name-pools/.yaml", "version: 1\nnames: [Abba]\n",
	    "Manifest.NamePool '': pool is required");
	CheckRefused(
	    "manifest/definitions/game-modes/.yaml", "version: 1\n",
	    "Manifest.GameMode '': name is required");
	CheckRefused(
	    "manifest/definitions/name-pools/coptic.yaml", "version: 1\npool: Coptic\nnames: [Abba]\n",
	    "Manifest.NamePool 'Coptic': pool must be the record's name");

	const std::string astralis = patch("manifest/definitions/game-modes/", "astralis");
	CheckRefused(
	    astralis, "version: 1\nitems: !append [{itemId: ItemNowhere}]\n",
	    "Manifest.GameMode 'astralis': items 'ItemNowhere' names nothing");
	CheckRefused(
	    astralis, "version: 1\nitems: !append [{itemId: ItemCopper}]\n",
	    "items selects 'ItemCopper' twice");
	CheckRefused(
	    astralis, "version: 1\nages: [{ageId: AgeNowhere}]\n", "ages 'AgeNowhere' names nothing");
	CheckRefused(
	    astralis, "version: 1\nskills: [{skillId: AgeStone}, {skillId: AgeStone}]\n",
	    "skills selects 'AgeStone' twice");

	const Overlay none;
	CHECK_THROWS_WITH_AS(
	    (void)LoadWith(none, "hard"), "manifest/definitions/game-modes: there is no game mode 'hard'",
	    ContentError);
}


TEST_CASE("The fixtures are the trading and governance scenarios their plans describe")
{
	const Overlay none;
	const ContentSet content = LoadWith(none);
	REQUIRE(content.scenarios.size() == 2);

	// One system, two planets each with a region of its own civilisation and a public station, and
	// eight goods (docs/stage-2-content-plan.md, "Initial scenario").
	const ScenarioDefinition &trading = content.scenarios.at("trading");
	CHECK(trading.items.size() == 8);
	CHECK(trading.systems.size() == 1);
	CHECK(trading.planets.size() == 2);
	CHECK(trading.civilisations.size() == 2);
	REQUIRE(trading.regions.size() == 2);
	CHECK(trading.regions[0].civilisationId != trading.regions[1].civilisationId);
	CHECK(trading.venues.size() == 2);
	// Complementary: each good is made at one station and wanted at the other.
	for (const ItemSelection &item : trading.items)
	{
		CAPTURE(item.itemId);
		int made = 0;
		int wanted = 0;
		for (const VenueDefinition &venue : trading.venues)
			for (const MarketEntry &entry : venue.market)
				if (entry.itemId == item.itemId)
				{
					made += entry.production > 0.0 ? 1 : 0;
					wanted += entry.demand > 0.0 ? 1 : 0;
				}
		CHECK(made == 1);
		CHECK(wanted == 1);
	}

	// Several independently governed regions on one planet: a home for each player and AI
	// neighbours, served by one station, and the Bronze project's region.
	const ScenarioDefinition &governance = content.scenarios.at("governance");
	int homes = 0;
	int neighbours = 0;
	for (const RegionDefinition &region : governance.regions)
		if (region.planetId == "PlanetForge")
			(region.playerHome ? homes : neighbours) += 1;
	CHECK(homes == 2);
	CHECK(neighbours == 2);
	const auto forge =
	    std::ranges::find(governance.venues, "VenueForgeHighport", &VenueDefinition::venueId);
	REQUIRE(forge != governance.venues.end());
	CHECK(forge->regions.size() == 4);
	const auto east =
	    std::ranges::find(governance.regions, "RegionForgeEast", &RegionDefinition::regionId);
	REQUIRE(east != governance.regions.end());
	REQUIRE(east->projects.size() == 1);
	CHECK(east->projects[0].projectId == "ProjectBronzeMetallurgy");
}


TEST_CASE("Each hull docks at the highport on the pads its class and size fit (D-058, D-062)")
{
	namespace Dock = Jettison::Dock;
	const Loaded loaded{kModDirectory};
	Dock::PadClasses classes;
	for (const std::string &name : loaded.registry.Names<Dock::PadClassDefinition>())
		classes.emplace(name, *loaded.registry.Find<Dock::PadClassDefinition>(name));
	CHECK(classes.size() == 5);
	const auto hull = [&](std::string_view id) -> const Dock::HullDefinition &
	{
		const auto *pHull = loaded.registry.Find<Dock::HullDefinition>(RegistryName(id));
		REQUIRE_MESSAGE(pHull != nullptr, id);
		return *pHull;
	};
	const Dock::HullDefinition &starter = hull("HullLoneTraderStarter");
	CHECK(starter.pad == "skiff");
	CHECK(glm::vec3{starter.length, starter.beam, starter.height} == classes.at("skiff").largestHull);
	const auto *pLayout = loaded.registry.Find<Dock::StationLayout>("highport");
	REQUIRE(pLayout != nullptr);
	const auto pads = [&](const Dock::HullDefinition &docked)
	{
		std::size_t count = 0;
		for (const Dock::PadLayout &pad : pLayout->pads)
			count += Dock::Fits(classes, docked, pad.padClass) ? 1 : 0;
		return count;
	};
	// Eight skiff pads and twelve destroyer pads; a frigate is given a destroyer's.
	CHECK(pads(starter) == 20);
	CHECK(pads(hull("HullCompactSingle")) == 20);
	CHECK(pads(hull("HullTraderLongHaul")) == 12);
}


TEST_CASE("Each rule a scenario breaks is refused, naming the scenario and the entry")
{
	const std::string governance = "manifest/scenarios/governance.patch.yaml";
	const std::string trading = "manifest/scenarios/trading.patch.yaml";
	CheckRefused(
	    governance, "version: 1\ngameMode: hard\n",
	    "manifest/scenarios/governance.yaml: Manifest.Scenario 'governance': gameMode 'hard' names no "
	    "game mode");
	CheckRefused(
	    trading, "version: 1\ngameMode: astralis\n",
	    "items 'ItemLime' is not one of mode 'astralis''s items");
	CheckRefused(
	    trading, "version: 1\nitems: !append [{itemId: ItemCopper}]\n",
	    "items selects 'ItemCopper' twice");
	CheckRefused(
	    trading, "version: 1\nsystems: !append [{systemId: SystemAnvil}]\n",
	    "systems 'SystemAnvil' is given twice");
	CheckRefused(
	    trading,
	    "version: 1\nplanets: !merge=planetId\n  - {planetId: PlanetOre, systemId: SystemNowhere}\n",
	    "planets 'PlanetOre' system 'SystemNowhere' names nothing");
	CheckRefused(
	    trading,
	    "version: 1\nplanets: !merge=planetId\n  - {planetId: PlanetOre, position: [.nan, 0, 0]}\n",
	    "planets 'PlanetOre' position must be finite");
	CheckRefused(
	    trading,
	    "version: 1\ncivilisations: !merge=civilisationId\n  - {civilisationId: CivilisationOre, "
	    "ageId: "
	    "AgeNowhere}\n",
	    "civilisations 'CivilisationOre' age 'AgeNowhere' names nothing");
	CheckRefused(
	    trading,
	    "version: 1\norganisations: !merge=organisationId\n  - {organisationId: "
	    "OrganisationBrineCouncil, "
	    "kind: ~}\n",
	    "organisations 'OrganisationBrineCouncil' kind is required");

	const auto region = [](std::string_view fields)
	{
		return std::format(
		    "version: 1\nregions: !merge=regionId\n  - {{regionId: RegionBrineCoast, {}}}\n", fields);
	};
	CheckRefused(
	    trading, region("planetId: PlanetNowhere"),
	    "'RegionBrineCoast' planet 'PlanetNowhere' names nothing");
	CheckRefused(
	    trading, region("civilisationId: CivilisationNowhere"),
	    "'RegionBrineCoast' civilisation 'CivilisationNowhere' names nothing");
	CheckRefused(
	    trading, region("governorId: ''"),
	    "'RegionBrineCoast' governor '' names no organisation, and only a player's home has none");
	CheckRefused(trading, region("population: -1"), "'RegionBrineCoast' population must be finite");
	CheckRefused(trading, region("publicFunds: -1"), "'RegionBrineCoast' funds must not be below 0");
	CheckRefused(
	    trading, region("health: 101"), "'RegionBrineCoast' health and happiness must be in [0, 100]");
	CheckRefused(
	    trading, region("skills: [{skillId: SkillNowhere}]"),
	    "'RegionBrineCoast' skill 'SkillNowhere' names nothing");
	CheckRefused(
	    trading,
	    region(
	        "facilities: [{facilityId: FacilityA, kind: Workshop}, {facilityId: FacilityA, kind: "
	        "Workshop}]"),
	    "regions 'FacilityA' is given twice");
	CheckRefused(
	    trading, region("facilities: [{facilityId: FacilityA}]"),
	    "'RegionBrineCoast' facility 'FacilityA' kind is required");

	const auto east = [](std::string_view fields)
	{
		return std::format(
		    "version: 1\nregions: !merge=regionId\n  - {{regionId: RegionForgeEast, {}}}\n", fields);
	};
	CheckRefused(
	    governance, east("projects: [{projectId: ProjectNowhere}]"),
	    "'RegionForgeEast' project 'ProjectNowhere' names no project");
	CheckRefused(
	    governance, east("skills: []"),
	    "'RegionForgeEast' project 'ProjectBronzeMetallurgy' requires 'SkillSmelting', which the "
	    "region "
	    "does not know");
	CheckRefused(
	    governance,
	    east(
	        "facilities: [{facilityId: FacilityForgeEastWorkshop, kind: Workshop, operational: "
	        "false}]"),
	    "'RegionForgeEast' project 'ProjectBronzeMetallurgy' needs an operational workshop");
	// The tin is Cassiter's: without it the project has no supplier.
	CheckRefused(
	    governance,
	    "version: 1\nvenues: !merge=venueId\n  - {venueId: VenueCassiterDock, market: [{itemId: "
	    "ItemTin, "
	    "production: 0}]}\n",
	    "'RegionForgeEast' project 'ProjectBronzeMetallurgy' needs 'ItemTin', which no venue produces");

	CheckRefused(
	    trading, "version: 1\nstarterHull: HullNowhere\n", "starterHull 'HullNowhere' names no hull");
	CheckRefused(
	    trading, "version: 1\nstarterHull: ''\n",
	    "starterHull is required: 'RegionOreHighlands' is a player's home");

	const auto venue = [](std::string_view fields)
	{
		return std::format(
		    "version: 1\nvenues: !merge=venueId\n  - {{venueId: VenueBrineHighport, {}}}\n", fields);
	};
	CheckRefused(
	    trading, "version: 1\nvenues: !append [{venueId: VenueBrineHighport}]\n",
	    "venues 'VenueBrineHighport' is given twice");
	CheckRefused(
	    trading, venue("systemId: SystemNowhere"),
	    "'VenueBrineHighport' system 'SystemNowhere' names nothing");
	CheckRefused(
	    {{trading,
	      "version: 1\nsystems: !append [{systemId: SystemFar}]\nplanets: !append [{planetId: "
	      "PlanetFar, "
	      "systemId: SystemFar}]\nvenues: !merge=venueId\n  - {venueId: VenueBrineHighport, planetId: "
	      "PlanetFar}\n"}},
	    "'VenueBrineHighport' planet 'PlanetFar' is not in its system");
	CheckRefused(
	    trading, venue("position: [0, .inf, 0]"), "'VenueBrineHighport' position must be finite");
	CheckRefused(
	    trading, venue("operatorId: OrganisationNowhere"),
	    "'VenueBrineHighport' operator 'OrganisationNowhere' names no organisation");
	CheckRefused(
	    trading, venue("layout: nowhere"), "'VenueBrineHighport' layout 'nowhere' names nothing");
	CheckRefused(
	    trading, venue("lifeSupportCapacity: -1"),
	    "'VenueBrineHighport' lifeSupportCapacity must not be below 0");
	CheckRefused(
	    trading, venue("regions: [{regionId: RegionNowhere}]"),
	    "'VenueBrineHighport' region 'RegionNowhere' names nothing");
	CheckRefused(
	    trading, venue("regions: [{regionId: RegionBrineCoast}, {regionId: RegionBrineCoast}]"),
	    "'VenueBrineHighport' serves 'RegionBrineCoast' twice");
	CheckRefused(
	    {{trading,
	      "version: 1\nsystems: !append [{systemId: SystemFar}]\nvenues: !merge=venueId\n  - {venueId: "
	      "VenueBrineHighport, systemId: SystemFar, planetId: ''}\n"}},
	    "'VenueBrineHighport' region 'RegionBrineCoast' is not on its planet");
	// A port serves the regions of its own planet, not another in its system.
	CheckRefused(
	    trading, venue("planetId: PlanetOre"),
	    "'VenueBrineHighport' region 'RegionBrineCoast' is not on its planet");
	CheckRefused(
	    trading, venue("market: [{itemId: ItemBread}]"),
	    "'VenueBrineHighport' market item 'ItemBread' is not a scenario item");
	CheckRefused(
	    trading, venue("market: [{itemId: ItemSalt}, {itemId: ItemSalt}]"),
	    "'VenueBrineHighport' market lists 'ItemSalt' twice");
	CheckRefused(
	    trading, venue("market: [{itemId: ItemSalt, production: -1}]"),
	    "'VenueBrineHighport' market 'ItemSalt' rates must be finite and not below 0");
	CheckRefused(
	    trading, venue("market: [{itemId: ItemSalt, stock: 1152000001}]"),
	    "'VenueBrineHighport' market 'ItemSalt' stock must be a whole number of totes up to "
	    "1152000000 IBCs");
	CheckRefused(
	    trading, venue("market: [{itemId: ItemSalt, stock: 0.1}]"),
	    "'VenueBrineHighport' market 'ItemSalt' stock must be a whole number of totes up to "
	    "1152000000 IBCs");

	// A player's home needs an operational public station that docks the starter, serves the region
	// and has life support to spare (D-047); each lack alone refuses it.
	for (const std::string_view lack :
	     {"public: false", "operational: false", "lifeSupportCapacity: 0", "regions: []"})
	{
		CAPTURE(lack);
		CheckRefused(
		    trading,
		    std::format(
		        "version: 1\nvenues: !merge=venueId\n  - {{venueId: VenueOreHighport, {}}}\n", lack),
		    "regions 'RegionOreHighlands' is a player's home, and no operational public station "
		    "serving it "
		    "docks 'HullLoneTraderStarter' with life support to spare");
	}
	// A layout whose only pad is of a class the starter does not fit, by size though not by rank.
	CheckRefused(
	    {{"manifest/definitions/pads/tender.yaml",
	      "version: 1\nrank: 0\nlargestHull: [5, 3, 1]\npadSize: [4, 6]\n"},
	     {"manifest/definitions/stations/tender-dock.yaml",
	      "version: 1\ndisplayName: Tender dock\ndecks: [{centre: [0, 0, 0], size: [40, 10, 40]}]\n"
	      "pads: [{class: tender, position: [0, 5, 0]}]\n"},
	     {trading, "version: 1\nvenues: !merge=venueId\n  - {venueId: VenueOreHighport, layout: "
	               "tender-dock}\n"}},
	    "regions 'RegionOreHighlands' is a player's home, and no operational public station serving "
	    "it docks 'HullLoneTraderStarter' with life support to spare");
}


TEST_CASE(
    "A refusal names only the layers the registry read: the topmost whole file and patches above it")
{
	const Overlay patched;
	patched.Write("manifest/definitions/ages/age-stone.patch.yaml", "version: 1\ndisplayName: Flint\n");
	const Overlay replaced;
	replaced.Write("manifest/definitions/ages/age-stone.yaml", "version: 1\nageId: AgeStone\n");
	const Overlay repatched;
	repatched.Write("manifest/definitions/ages/age-stone.patch.yaml", "version: 1\ndescription: Old\n");
	const std::array<Jettison::Vfs::Mount, 4> sources{
	    {{"base", kModDirectory},
	     {"patched", patched.Root()},
	     {"replaced", replaced.Root()},
	     {"repatched", repatched.Root()}}};
	CHECK_THROWS_WITH_AS(
	    (void)LoadContent(sources, "manifest"),
	    "manifest/definitions/ages/age-stone.yaml: Manifest.Age 'AgeStone': displayName is required "
	    "(from replaced, repatched (patch))",
	    ContentError);
}


TEST_CASE("A hull's stats are a map, patched by stat, and every stat has its name")
{
	Overlay overlay;
	overlay.Write(
	    "manifest/definitions/hulls/hull-trader-long-haul.patch.yaml",
	    "version: 1\nbaseStats: {CargoVolume: 1000, GunRear: ~}\n");
	const ContentSet content = LoadWith(overlay);
	const auto trader = std::ranges::find(
	    content.hulls, std::string{"HullTraderLongHaul"}, &Manifest::Core::ShipClassSpec::hullId);
	REQUIRE(trader != content.hulls.end());
	CHECK(trader->baseStats.at(Manifest::Core::EffectStat::CargoVolume) == 1000.0);
	CHECK(trader->baseStats.at(Manifest::Core::EffectStat::ShieldFront) == 120.0);
	CHECK_FALSE(trader->baseStats.contains(Manifest::Core::EffectStat::GunRear));

	// The registration names each enumerator by hand: one left out could not be authored.
	RegisterTypes();
	for (const Manifest::Core::EffectStat stat : Manifest::Core::AllEffectStats())
	{
		CAPTURE(Manifest::Core::StatName(stat));
		CHECK_FALSE(Jettison::Reflect::EnumName(entt::forward_as_meta(stat)).empty());
	}
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
	    "manifest/definitions/game-modes/astralis.patch.yaml",
	    "version: 1\nitems: !append [{itemId: ItemSilver}]\n");
	const ContentSet manifest = LoadWith(overlay);
	CHECK(manifest.ages.front().displayName == "Flint");
	CHECK(manifest.ages.front().description.empty());
	const auto copper = std::find_if(
	    manifest.gameMode.items.begin(), manifest.gameMode.items.end(),
	    [](const Manifest::Core::ItemNode &item) { return item.itemId == "ItemCopper"; });
	REQUIRE(copper != manifest.gameMode.items.end());
	CHECK(copper->basePrice == "12.25");
	CHECK(copper->displayName == "Copper");

	const ContentSet astralis = LoadWith(overlay, "astralis");
	CHECK(astralis.gameMode.items.back().itemId == "ItemSilver");
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
	// Authored unquoted as 5.0: a float on the way would have made it "5".
	CHECK(Get<Manifest::Core::ItemNode>(loaded, "ItemWine").basePrice == "5.0");
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
	const ContentIdentity directory = BuildContentIdentity(Loaded{kModDirectory}.registry, {});
	const ContentIdentity pack = BuildContentIdentity(Loaded{MANIFEST_MOD_PACK}.registry, {});
	CHECK(directory.manifest == pack.manifest);
	CHECK(directory.fingerprint == pack.fingerprint);
	CHECK(directory.fingerprint != 0);
	CHECK(BuildContentIdentity(Loaded{kModDirectory}.registry, {}).manifest == directory.manifest);
}


TEST_CASE("A changed price, a reordered selection or an unselected definition changes the manifest")
{
	const ContentIdentity base = BuildContentIdentity(Loaded{kModDirectory}.registry, {});
	const auto identity = [](std::string_view path, std::string_view text)
	{
		const Overlay overlay;
		overlay.Write(std::string{path}, text);
		return BuildContentIdentity(Loaded{kModDirectory, overlay.Root()}.registry, {});
	};

	const ContentIdentity price = identity(
	    "manifest/definitions/items/item-copper.patch.yaml", "version: 1\nbasePrice: \"10.5\"\n");
	CHECK(price.manifest != base.manifest);
	CHECK(price.fingerprint != base.fingerprint);

	const ContentIdentity order = identity(
	    "manifest/definitions/game-modes/astralis.patch.yaml",
	    "version: 1\nitems: [{itemId: ItemSilver}, {itemId: ItemGold}]\n");
	CHECK(order.manifest != base.manifest);
	CHECK(
	    order.manifest != identity(
	                          "manifest/definitions/game-modes/astralis.patch.yaml",
	                          "version: 1\nitems: [{itemId: ItemGold}, {itemId: ItemSilver}]\n")
	                          .manifest);

	const ContentIdentity added = identity(
	    "manifest/definitions/ages/age-golden.yaml",
	    "version: 1\nageId: AgeGolden\ndisplayName: Golden\n");
	CHECK(added.manifest != base.manifest);
}


TEST_CASE("A selected scenario's homes provision at the first station that docks the starter")
{
	Jettison::Vfs::FileSystem files;
	files.AddMount({"base", kModDirectory});
	Jettison::Sim::DefinitionRegistry definitions{files};
	const ContentSet trading = LoadScenario(files, definitions, "trading");
	CHECK(trading.tuning.company.startingCredits == 25000);
	CHECK(
	    trading.homeVenues ==
	    std::map<std::string, std::string, std::less<>>{{"RegionOreHighlands", "VenueOreHighport"}});

	const Overlay none;
	CHECK(LoadWith(none).homeVenues.empty());
}


TEST_CASE("An IBC holds an item's units per cubic metre, a finite amount above 0 in every unit")
{
	const Overlay none;
	const ContentSet content = LoadWith(none);
	std::set<Manifest::Core::TradeUnit> units;
	for (const Manifest::Core::ItemNode &item : content.gameMode.items)
	{
		CAPTURE(item.itemId);
		units.insert(item.unitType.value());
		CHECK(std::isfinite(item.unitsPerCubicMetre));
		CHECK(item.unitsPerCubicMetre > 0.0);
	}
	CHECK(units.size() >= 5);
	// Copper by the tonne, 8.96 to the IBC: the starter's 20 IBCs hold 179.2 t.
	const auto copper = std::ranges::find(
	    content.gameMode.items, std::string{"ItemCopper"}, &Manifest::Core::ItemNode::itemId);
	REQUIRE(copper != content.gameMode.items.end());
	CHECK(copper->unitsPerCubicMetre == 8.96);
}
