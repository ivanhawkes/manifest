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
#include <stdexcept>
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
#include <jettison/transit/Profile.h>
#include <jettison/vfs/FileSystem.h>
#include <manifest/space/Cruise.h>

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
	CheckRefused(
	    copper, "version: 1\nunitsPerCubicMetre: 8.96\n",
	    "unitsPerCubicMetre must be the 7.616 a packed cube holds by its density and packFactor");
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
	for (const std::string_view volume : {"18446744073709551616", "400000001", "160.5"})
		CheckRefused(
		    starter, std::format("version: 1\nbaseStats: {{CargoVolume: {}}}\n", volume),
		    "CargoVolume must be a whole number of totes up to 400000000");
	CheckRefused(
	    starter, "version: 1\nfitableEffects: [Teleport]\n",
	    "fitableEffects 'Teleport' names no effect");
	CheckRefused(starter, "version: 1\noccupants: -1\n", "occupants must not be negative");
	CheckRefused(
	    "manifest/definitions/game-modes/manifest.patch.yaml",
	    "version: 1\ncruise: {manned: {boostAcceleration: 9, boostSeconds: 60, sustainedAcceleration: "
	    "10}}\n",
	    "Cruise drive 'manifest manned' has a profile");

	// Jettison's ship and station records, checked as the engine checks them (D-062).
	CheckRefused(
	    "manifest/definitions/dock-hulls/hull-lone-trader-starter.patch.yaml",
	    "version: 3\nlength: 12\n",
	    "Dock.VehicleDefinition 'hull-lone-trader-starter': dock Vehicle 'hull-lone-trader-starter' is "
	    "12 "
	    "x "
	    "6.4 x 1.8 m, larger than its berth's largest vehicle, 9.8 x 6.4 x 1.8 m");
	CheckRefused(
	    "manifest/definitions/pads/skiff.patch.yaml", "version: 2\nberthSize: [4, 10]\n",
	    "Dock.BerthClassDefinition 'skiff': dock Berth class 'skiff' has a largest vehicle");
	CheckRefused(
	    "manifest/definitions/stations/highport.patch.yaml", "version: 2\npiers: []\n",
	    "Dock.PlatformLayout 'highport': dock Platform layout 'highport'");
	CheckRefused(
	    "manifest/definitions/dock-hulls/hull-nowhere.yaml",
	    "version: 3\ndisplayName: Nowhere\nlength: 5\nbeam: 3\nheight: 1\nberth: skiff\n",
	    "Dock.VehicleDefinition 'hull-nowhere': dock is the dock hull of no Manifest.Hull of its name");
	CheckRefused(
	    "manifest/definitions/hulls/hull-extra.yaml",
	    "version: 1\nhullId: HullExtra\nclassName: Extra\nbaseStats: {CargoVolume: 1}\ndriveId: "
	    "DriveTorch\n",
	    "Manifest.Hull 'HullExtra': dock has no Dock.VehicleDefinition 'hull-extra'");
	CheckRefused(
	    starter, "version: 1\ndriveId: DriveNowhere\n", "driveId 'DriveNowhere' names no drive");

	// A drive class (D-108).
	const std::string torch = patch("manifest/definitions/drive-classes/", "drive-torch");
	CheckRefused(
	    torch, "version: 1\nexhaustKms: 0\n",
	    "Manifest.Drive 'DriveTorch': exhaustKms must be above 0, or left out for a drive with no "
	    "reaction mass");
	CheckRefused(
	    torch, "version: 1\npelletItemId: ItemCopper\n",
	    "pelletItemId 'ItemCopper' names no item traded by the kilogram");
	CheckRefused(
	    torch, "version: 1\npelletMilligramsPerTonneKms: -1\n",
	    "pelletMilligramsPerTonneKms must be above 0");
	CheckRefused(torch, "version: 1\ncurveExponent: -1\n", "curveExponent must not be below 0");
	CheckRefused(
	    torch, "version: 1\nexclusionRadius: 1999\n",
	    "exclusionRadius must be finite and at least a station's reach, 2000 m");
	CheckRefused(
	    torch, "version: 1\nbodyExclusionRadius: 0\n",
	    "bodyExclusionRadius must be above 0, or left out for a drive that may thrust near a populated "
	    "body");
	CheckRefused(
	    torch,
	    "version: 1\ncruise: {manned: {boostAcceleration: 9, boostSeconds: 60, sustainedAcceleration: "
	    "10}, unmanned: {boostAcceleration: 9, boostSeconds: 0, sustainedAcceleration: 9}}\n",
	    "Cruise drive 'drive-torch manned' has a profile");
	CheckRefused(
	    starter, "version: 1\nscrubberCartridges: 0\n", "scrubberCartridges must be at least 1");
	CheckRefused(starter, "version: 1\noxygenTanks: []\n", "oxygenTanks holds none");
	CheckRefused(
	    starter, "version: 1\noxygenTanks: [10, 0]\n", "oxygenTanks a capacity must be above 0");
	CheckRefused(starter, "version: 1\nmass: 0\n", "mass must be above 0");

	// Above 0 as a double, and 0 as the float a body keeps.
	CheckRefused(starter, "version: 1\nmass: 1e-100\n", "mass must be above 0");

	// A float, and its box's inertia past what Jolt's float arithmetic holds.
	CheckRefused(
	    starter, "version: 1\nmass: 1e38\n",
	    "mass gives its 9.8 x 6.4 x 1.8 m box an inertia past 1e+18 kg m^2");
	CheckRefused(
	    starter, "version: 1\nmass: null\n",
	    "starterHull 'HullLoneTraderStarter' has no mass to fly with");

	// A company's offer (D-096).
	const std::string offer = "manifest/definitions/offers/offer-cargo-holds.patch.yaml";
	CheckRefused(
	    offer, "version: 1\ncost: 0\n",
	    "Manifest.Offer 'OfferCargoHolds': cost must be at least 1 credit");
	CheckRefused(offer, "version: 1\nmaxTier: 4\n", "maxTier must be from 1 to its skill's 3");
	CheckRefused(offer, "version: 1\nmaxTier: 0\n", "maxTier must be from 1 to its skill's 3");
	CheckRefused(offer, "version: 1\nskillId: SkillNothing\n", "skillId 'SkillNothing' names no skill");
	CheckRefused(
	    offer, "version: 1\nskillId: SkillYardDrydock\n",
	    "skillId 'SkillYardDrydock' is no ship system");
	CheckRefused(
	    "manifest/definitions/offers/offer-bare.yaml",
	    "version: 1\nofferId: OfferBare\nskillId: SkillShipCargoHolds\ncost: 1\nmaxTier: 1\n",
	    "Manifest.Offer 'OfferBare': target is required");

	const std::string balance = "manifest/definitions/tuning/balance.patch.yaml";
	CheckRefused(
	    balance, "version: 1\neconomy: {intervalSeconds: 0}\n",
	    "Manifest.Tuning 'balance': economy.intervalSeconds must be above 0");
	CheckRefused(
	    balance, "version: 1\neconomy: {wageTaxRate: 1}\n", "economy.wageTaxRate must be in [0, 1)");
	CheckRefused(
	    balance, "version: 1\neconomy: {intervalSeconds: 31536001}\n",
	    "economy.intervalSeconds must be above 0 and at most a year");
	CheckRefused(
	    balance, "version: 1\neconomy: {wageTaxRate: 1e-300}\n",
	    "economy.wageTaxRate must be 0 or one credit in a whole number of credits");
	CheckRefused(
	    balance, "version: 1\ncompany: {startingCredits: 0}\n",
	    "company.startingCredits must be above 0");
	CheckRefused(
	    balance, "version: 1\ncompany: {foundingPrice: 0}\n", "company.foundingPrice must be above 0");
	const std::string_view guild =
	    "{projectId: ProjectSmeltersGuild, inputs: [{itemId: ItemCopper, totes: 3}], "
	    "workDays: 3";
	CheckRefused(
	    balance, std::format("version: 1\nprojects: [{}}}]\n", guild),
	    "'ProjectSmeltersGuild' must unlock a recipe or teach a skill, one of the two");
	CheckRefused(
	    balance,
	    std::format(
	        "version: 1\nprojects: [{}, teachesSkill: SkillSmelting, unlocksRecipe: RecipeBronze}}]\n",
	        guild),
	    "'ProjectSmeltersGuild' must unlock a recipe or teach a skill, one of the two");
	CheckRefused(
	    balance, std::format("version: 1\nprojects: [{}, teachesSkill: SkillNone}}]\n", guild),
	    "'ProjectSmeltersGuild' teaches 'SkillNone', which names no skill");
	CheckRefused(
	    balance, "version: 1\nmarket: {listingFeeBps: 10001}\n",
	    "market.listingFeeBps must be in [0, 10000]");
	const auto classes = [](std::string_view skiff)
	{
		return std::format(
		    "version: 1\ncharges:\n  padClasses:\n    - {}\n    - {{padClass: shuttle}}\n"
		    "    - {{padClass: cutter}}\n    - {{padClass: corvette}}\n    - {{padClass: frigate}}\n"
		    "    - {{padClass: destroyer}}\n    - {{padClass: cruiser}}\n"
		    "    - {{padClass: battlecruiser}}\n    - {{padClass: battleship}}\n"
		    "    - {{padClass: dreadnought}}\n    - {{padClass: titan}}\n",
		    skiff);
	};
	CheckRefused(balance, classes("{padClass: skiff, dockingFee: -1}"), "'skiff' has a fee below 0");
	CheckRefused(
	    balance, classes("{padClass: skiff, berthFeePerHour: -1}"), "'skiff' has a fee below 0");
	CheckRefused(balance, classes("{padClass: dinghy}"), "'dinghy' names no pad class");
	CheckRefused(balance, classes("{padClass: corvette}"), "'corvette' is charged twice");
	CheckRefused(
	    balance,
	    "version: 1\ncharges:\n  padClasses: [{padClass: shuttle}, {padClass: cutter}, "
	    "{padClass: corvette}, {padClass: frigate}, {padClass: destroyer}, {padClass: cruiser}, "
	    "{padClass: battlecruiser}, {padClass: battleship}, {padClass: dreadnought}, "
	    "{padClass: titan}]\n",
	    "pad class 'skiff' has no fees");
	CheckRefused(
	    balance, "version: 1\ncharges: {berthFeePerOccupantHour: -1}\n",
	    "charges.berthFeePerOccupantHour must not be below 0");
	CheckRefused(
	    balance, "version: 1\ncharges: {arrearsRateBps: 3501}\n",
	    "charges.arrearsRateBps must be in [0, charges.maxArrearsRateBps]");
	CheckRefused(
	    balance, "version: 1\ncharges: {seizureGraceDays: -1}\n",
	    "charges.seizureGraceDays must be in [0, 36500]");
	CheckRefused(
	    balance, "version: 1\ncharges: {pricing: {lowerBelowBps: 8000}}\n",
	    "charges.pricing needs 0 <= lowerBelowBps <= raiseAboveBps <= 10000");
	CheckRefused(
	    balance, "version: 1\ncharges: {pricing: {maxLevelBps: 9999}}\n",
	    "charges.pricing needs stepBps in [0, 1000000] and 0 <= minLevelBps <= 10000 <= maxLevelBps "
	    "<= 1000000");
	CheckRefused(
	    balance, "version: 1\nmarket: {orderLifeSeconds: -1}\n",
	    "market.orderLifeSeconds must be finite and not below 0");
	CheckRefused(
	    balance, "version: 1\nmarket: {salesWindowSeconds: 0}\n",
	    "market.salesWindowSeconds must be above 0");
	CheckRefused(
	    balance, "version: 1\nbrokers: {treasury: -1}\n", "brokers.treasury must be in [0, 2^62]");
	CheckRefused(
	    balance, "version: 1\nbrokers: {treasury: 4611686018427387905}\n",
	    "brokers.treasury must be in [0, 2^62]");
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
	    balance,
	    "version: 1\noptionalGoods: !merge=bucketId\n  - {bucketId: BucketLuxury, demandShare: 2}\n",
	    "'BucketLuxury' demandShare must be in (0, 1]");
	CheckRefused(
	    balance,
	    "version: 1\noptionalGoods: !merge=bucketId\n  - {bucketId: BucketLuxury, categories: "
	    "[{categoryId: CategoryNone}]}\n",
	    "'BucketLuxury' category 'CategoryNone' names nothing");
	CheckRefused(
	    balance, "version: 1\ncondition: {initial: 101}\n", "condition.initial must be in [0, 100]");
	CheckRefused(
	    balance, "version: 1\ncondition: {workRateFloor: 2}\n",
	    "condition.workRateFloor must be in [0, 1]");
	CheckRefused(
	    balance, "version: 1\neconomy: {wageTaxRate: 0.3}\n",
	    "economy.wageTaxRate must be 0 or one credit in a whole number of credits");
	CheckRefused(
	    balance, "version: 1\nproduction: {batchSeconds: 0}\n",
	    "production.batchSeconds must be above 0");
	CheckRefused(
	    balance, "version: 1\nproduction: {wageShare: 1}\n", "production.wageShare must be in [0, 1)");
	CheckRefused(
	    balance, "version: 1\nlifeSupport: {oxygenItemId: ItemNothing}\n",
	    "'ItemNothing' names no item traded in its unit");

	// Water is by the kilolitre, and a tank holds kilograms.
	CheckRefused(
	    balance, "version: 1\nlifeSupport: {oxygenItemId: ItemDrinkingWater}\n",
	    "'ItemDrinkingWater' names no item traded in its unit");
	CheckRefused(
	    balance, "version: 1\nlifeSupport: {cartridgeItemId: ItemOxygen}\n",
	    "'ItemOxygen' names no item traded in its unit");
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
	    "ItemBronze, totes: 0}]}\n",
	    "'ItemBronze' must be a count of totes above 0");
	CheckRefused(
	    balance,
	    "version: 1\nrecipes: !merge=recipeId\n  - {recipeId: RecipeBronze, outputs: [{itemId: "
	    "ItemBronze, totes: 9223372036854775808}]}\n",
	    "'ItemBronze' must be a count of totes above 0 and within a warehouse");
	CheckRefused(
	    balance,
	    "version: 1\nprojects: !merge=projectId\n  - {projectId: ProjectBronzeMetallurgy, inputs: "
	    "[{itemId: ItemTin, totes: 1}, {itemId: ItemTin, totes: 2}]}\n",
	    "'ItemTin' is given twice");
	CheckRefused(
	    balance,
	    "version: 1\nprojects: !merge=projectId\n  - {projectId: ProjectBronzeMetallurgy, workDays: "
	    "0}\n",
	    "'ProjectBronzeMetallurgy' workDays must be above 0");
	CheckRefused(
	    balance,
	    "version: 1\nprojects: !merge=projectId\n  - {projectId: ProjectBronzeMetallurgy, "
	    "priorityPremium: -0.5}\n",
	    "'ProjectBronzeMetallurgy' priorityPremium must be 0 or above");
	CheckRefused(
	    balance, "version: 1\nprocurement: {aiBidDays: 8}\n",
	    "needs 1 <= minBidDays <= aiBidDays <= maxBidDays and the same of the delivery days");
	CheckRefused(
	    balance, "version: 1\nprocurement: {minDeliveryDays: 0}\n",
	    "needs 1 <= minBidDays <= aiBidDays <= maxBidDays and the same of the delivery days");
	CheckRefused(
	    balance, "version: 1\nprocurement: {keptClosed: 0}\n",
	    "needs maxOpenTenders, maxBids and keptClosed above 0");

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

	// A file named ".yaml" has no record name: the engine refuses it before any record is read.
	for (const std::string_view path :
	     {"manifest/definitions/name-pools/.yaml", "manifest/definitions/game-modes/.yaml"})
	{
		CAPTURE(path);
		const Overlay overlay;
		overlay.Write(std::string{path}, "version: 1\n");
		CHECK_THROWS_WITH_AS(
		    (void)LoadWith(overlay), doctest::Contains("has no extension to patch beside"),
		    std::invalid_argument);
	}
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
	    astralis, "version: 1\nhulls: !append [{hullId: HullNowhere}]\n",
	    "hulls 'HullNowhere' names nothing");
	CheckRefused(
	    astralis, "version: 1\ndrives: !append [{driveId: DriveNowhere}]\n",
	    "drives 'DriveNowhere' names nothing");
	CheckRefused(
	    astralis, "version: 1\nhulls: !append [{hullId: HullFreighterMedium}]\n",
	    "hulls 'HullFreighterMedium' flies drive 'DriveHaulerFusion', which the mode does not select");
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

	// One system, two planets, ten regions each served by a station of its own, nine governed, of
	// mixed ages (docs/content.md, "Scenarios"), with the essentials every region trades (W04), the
	// life-support goods a ship's resupply takes (W09) and the wellness goods (W19); and each planet's
	// yards, a dry dock serving its port region (W22b).
	const ScenarioDefinition &trading = content.scenarios.at("trading");
	CHECK(trading.items.size() == 26);
	CHECK(trading.systems.size() == 1);
	CHECK(trading.planets.size() == 2);
	REQUIRE(trading.regions.size() == 10);
	CHECK(trading.venues.size() == 12);
	CHECK(
	    std::ranges::count_if(
	        trading.regions,
	        [](const RegionDefinition &region) { return !region.governorId.empty(); }) == 9);
	std::set<std::string> ages;
	for (const CivilisationDefinition &civilisation : trading.civilisations)
		ages.insert(civilisation.ageId);
	CHECK(ages.size() >= 8);
	for (const VenueDefinition &venue : trading.venues)
		CHECK(venue.regions.size() == 1);

	// Each region's staples by its kind of world (D-105): a developed one makes half again its need,
	// the others less, a mining colony none.
	const auto &staples =
	    std::ranges::find(content.tuning.needs, "NeedStapleFood", &NeedDefinition::needId);
	REQUIRE(staples != content.tuning.needs.end());
	for (const RegionDefinition &region : trading.regions)
	{
		CAPTURE(region.regionId);
		double made = 0.0;
		for (const VenueDefinition &venue : trading.venues)
			for (const MarketEntry &entry : venue.market)
				if (entry.regionId == region.regionId &&
				    std::ranges::find(staples->items, entry.itemId, &ItemSelection::itemId) !=
				        staples->items.end())
					made += entry.production;
		const double share = made / (staples->perThousandPerDay * region.population / 1'000.0);
		CHECK((share == 0.0 || (share >= 0.5 && share <= 0.6) || share == doctest::Approx(1.5)));
	}

	// Complementary: each good is made at a station and wanted at another.
	const auto needed = [&](const std::string &itemId)
	{
		return std::ranges::any_of(
		    content.tuning.needs,
		    [&](const NeedDefinition &need)
		    {
			    return std::ranges::find(need.items, itemId, &ItemSelection::itemId) !=
			           need.items.end();
		    });
	};

	// A torch's fuels are the ships' (D-108): every station of both scenarios makes reaction mass,
	// 10,000 kg a day, its broker seeded with 800 totes, about nine days, and deuterium for the hauler,
	// 200 kg a day with 16 totes, two medium freighters' magazines (W22b); one station of each planet
	// refines the torch's pellets, 2 kg a day; no household wants any of them.
	const auto yards = [](const VenueDefinition &venue) { return venue.layout == "drydock"; };
	for (const ScenarioDefinition *scenario : {&trading, &content.scenarios.at("governance")})
		for (const VenueDefinition &venue : scenario->venues)
		{
			CAPTURE(venue.venueId);
			const auto mass = std::ranges::find(venue.market, "ItemReactionMass", &MarketEntry::itemId);
			REQUIRE(mass != venue.market.end());
			CHECK(mass->production == (yards(venue) ? 100000.0 : 10000.0));
			CHECK(mass->demand == 0.0);
			CHECK(mass->stock == (yards(venue) ? 8000 : 800));
			CHECK(mass->regionId == venue.regions[0].regionId);
			const auto deuterium =
			    std::ranges::find(venue.market, "ItemDeuteriumPellets", &MarketEntry::itemId);
			REQUIRE(deuterium != venue.market.end());
			CHECK(deuterium->production == 200.0);
			CHECK(deuterium->demand == 0.0);
			CHECK(deuterium->stock == 16);
			const auto pellets =
			    std::ranges::find(venue.market, "ItemFusionPellets", &MarketEntry::itemId);
			const bool refinery = venue.venueId == "VenueOreDeepsStation" ||
			                      venue.venueId == "VenueBrineSaltworksStation" ||
			                      venue.venueId == "VenueCassiterDock";
			CHECK((pellets != venue.market.end()) == refinery);
			if (pellets != venue.market.end())
			{
				CHECK(pellets->production == 2.0);
				CHECK(pellets->demand == 0.0);
			}
		}
	for (const ItemSelection &item : trading.items)
	{
		CAPTURE(item.itemId);
		if (item.itemId == "ItemReactionMass" || item.itemId == "ItemFusionPellets" ||
		    item.itemId == "ItemDeuteriumPellets" || item.itemId == "ItemPulseUnits")
			continue;

		// Each venue trades an essential, which each region needs.
		if (needed(item.itemId))
		{
			for (const VenueDefinition &venue : trading.venues)
				CHECK(
				    std::ranges::find(venue.market, item.itemId, &MarketEntry::itemId) !=
				    venue.market.end());
			continue;
		}
		int made = 0;
		int wanted = 0;
		for (const VenueDefinition &venue : trading.venues)
			for (const MarketEntry &entry : venue.market)
				if (entry.itemId == item.itemId)
				{
					made += entry.production > 0.0 ? 1 : 0;
					wanted += entry.demand > 0.0 ? 1 : 0;
				}
		CHECK(made >= 1);
		CHECK(wanted >= 1);
		for (const VenueDefinition &venue : trading.venues)
			for (const MarketEntry &entry : venue.market)
				CHECK_FALSE(
				    (entry.itemId == item.itemId && entry.production > 0.0 && entry.demand > 0.0));
	}

	// The yards (W22b): the Port Authority's dry docks, serving each planet's port region with the
	// ships' fuels, pulse units among them, and its bulk goods; the pulse carrier docks there alone,
	// on their battleship pads, and the medium freighter at every venue.
	for (const VenueDefinition &venue : trading.venues)
	{
		CAPTURE(venue.venueId);
		CHECK(Manifest::Content::PadFits(content, venue.venueId, "HullPulseCarrier") == yards(venue));
		CHECK(Manifest::Content::PadFits(content, venue.venueId, "HullFreighterMedium"));
		if (!yards(venue))
			continue;
		CHECK(venue.operatorId == "OrganisationAnvilPortAuthority");
		CHECK(venue.lifeSupportCapacity == 12);
		const auto units = std::ranges::find(venue.market, "ItemPulseUnits", &MarketEntry::itemId);
		REQUIRE(units != venue.market.end());
		CHECK(units->production == 10000.0);
		CHECK(units->stock == 30000);
		const Jettison::Dock::PlatformLayout &layout = content.layouts.at(venue.layout);
		for (const Jettison::Dock::BerthLayout &berth : layout.berths)
		{
			CHECK(berth.features == std::vector<std::string>{"dry-dock"});
			CHECK(
			    Jettison::Dock::Fits(
			        content.padClasses, content.dockHulls.at("hull-pulse-carrier"), berth.berthClass) ==
			    (berth.berthClass == "battleship"));
		}
		CHECK(std::ranges::count_if(layout.berths, &Jettison::Dock::BerthLayout::display) == 2);
	}
	CHECK(std::ranges::count_if(trading.venues, yards) == 2);

	// Several independent regions on one planet: two ungoverned and two AI neighbours, served by one
	// station, and the Bronze project's region.
	const ScenarioDefinition &governance = content.scenarios.at("governance");
	int ungoverned = 0;
	int neighbours = 0;
	for (const RegionDefinition &region : governance.regions)
		if (region.planetId == "PlanetForge")
			(region.governorId.empty() ? ungoverned : neighbours) += 1;
	CHECK(ungoverned == 2);
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


TEST_CASE("The Manifest mode cruises by acceleration, by occupants, and Astralis by its drives (D-090)")
{
	const Loaded loaded{kModDirectory};
	const Overlay none;
	const ContentSet manifest = LoadWith(none);
	const ContentSet astralis = LoadWith(none, "astralis");
	REQUIRE(!manifest.drives.empty());
	for (const auto &[name, drive] : astralis.drives)
	{
		CAPTURE(name);
		CHECK(drive == *loaded.registry.Find<Manifest::Space::CruiseDrive>(name));
		CHECK_FALSE(drive.profile);
	}

	// The starter and the compact carry one; the long-haul trader's occupants are open, so manned.
	const auto profile = [&](const ContentSet &content, std::string_view hull)
	{
		const Manifest::Space::CruiseDrive &drive = content.drives.at(std::string{hull});
		REQUIRE(drive.profile);
		return *drive.profile;
	};
	const double g = 9.80665;
	for (const std::string_view hull :
	     {"hull-lone-trader-starter", "hull-compact-single", "hull-trader-long-haul"})
	{
		CAPTURE(hull);
		const Jettison::Transit::MotionProfile manned = profile(manifest, hull);
		CHECK(manned.boostAcceleration == doctest::Approx(2.0 * g));
		CHECK(manned.boostSeconds == doctest::Approx(4.0 * 3600.0));
		CHECK(manned.sustainedAcceleration == doctest::Approx(g));
	}

	// A local trip, between two highports on opposite sides of one planet, is at 2 g throughout: the
	// boost lasts any trip under about a million kilometres.
	const Manifest::Space::CruiseDrive starter = manifest.drives.at("hull-lone-trader-starter");
	CHECK(
	    Manifest::Space::CruiseSeconds(starter, 8.4e7) ==
	    doctest::Approx(2.0 * std::sqrt(8.4e7 / (2.0 * g))));
	CHECK(
	    Manifest::Space::CruiseSeconds(starter, 1.0e9) ==
	    doctest::Approx(2.0 * std::sqrt(1.0e9 / (2.0 * g))));

	// A hull with no one aboard, 12 g throughout.
	const Overlay crewless;
	crewless.Write(
	    "manifest/definitions/hulls/hull-lone-trader-starter.patch.yaml", "version: 1\noccupants: 0\n");
	const Jettison::Transit::MotionProfile unmanned =
	    profile(LoadWith(crewless), "hull-lone-trader-starter");
	CHECK(unmanned.boostAcceleration == doctest::Approx(12.0 * g));
	CHECK(unmanned.sustainedAcceleration == doctest::Approx(12.0 * g));
	CHECK(
	    Manifest::Space::CruiseSeconds({.profile = unmanned}, 3.8e11) ==
	    doctest::Approx(2.0 * std::sqrt(3.8e11 / (12.0 * g))));
}


TEST_CASE("Each hull docks at the highport on the pads its class and size fit (D-058, D-062)")
{
	namespace Dock = Jettison::Dock;
	const Loaded loaded{kModDirectory};
	Dock::BerthClasses classes;
	for (const std::string &name : loaded.registry.Names<Dock::BerthClassDefinition>())
		classes.emplace(name, *loaded.registry.Find<Dock::BerthClassDefinition>(name));
	CHECK(classes.size() == 11);
	const auto hull = [&](std::string_view id) -> const Dock::VehicleDefinition &
	{
		const auto *pHull = loaded.registry.Find<Dock::VehicleDefinition>(RegistryName(id));
		REQUIRE_MESSAGE(pHull != nullptr, id);
		return *pHull;
	};
	const Dock::VehicleDefinition &starter = hull("HullLoneTraderStarter");
	CHECK(starter.berthClass == "skiff");
	CHECK(
	    glm::vec3{starter.length, starter.beam, starter.height} == classes.at("skiff").largestVehicle);
	const auto *pLayout = loaded.registry.Find<Dock::PlatformLayout>("highport");
	REQUIRE(pLayout != nullptr);
	const auto pads = [&](const Dock::VehicleDefinition &docked)
	{
		std::size_t count = 0;
		for (const Dock::BerthLayout &pad : pLayout->berths)
			count += Dock::Fits(classes, docked, pad.berthClass) ? 1 : 0;
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
	    "manifest/definitions/game-modes/manifest.patch.yaml",
	    "version: 1\nhulls: [{hullId: HullTraderLongHaul}]\n",
	    "starterHull 'HullLoneTraderStarter' is not one of mode 'manifest''s hulls");
	CheckRefused(
	    trading, "version: 1\nsystems: !append [{systemId: SystemAnvil}]\n",
	    "systems 'SystemAnvil' is given twice");
	CheckRefused(
	    trading,
	    "version: 1\nplanets: !merge=planetId\n  - {planetId: PlanetOre, systemId: SystemNowhere}\n",
	    "planets 'PlanetOre' system 'SystemNowhere' names nothing");
	CheckRefused(
	    trading,
	    "version: 1\nplanets: !append [{planetId: PlanetNowhere, systemId: SystemAnvil, displayName: "
	    "Nowhere}]\n",
	    "planets 'PlanetNowhere' is no planet or moon of system 'SystemAnvil'");
	CheckRefused(
	    trading,
	    "version: 1\nplanets: !merge=planetId\n  - {planetId: PlanetOre, systemId: SystemAnvil}\n"
	    "venues: !append [{venueId: StarAnvil, systemId: SystemAnvil, operatorId: "
	    "OrganisationAnvilPortAuthority, layout: highport}]\n",
	    "venues 'StarAnvil' is no station of system 'SystemAnvil'");
	CheckRefused(
	    trading, "version: 1\nsystems: !append [{systemId: SystemFar}]\n",
	    "systems 'SystemFar' has no Space.StarSystem 'system-far' that resolves");
	CheckRefused(
	    trading,
	    "version: 1\nsystems: !merge=systemId\n  - {systemId: SystemAnvil, connections: [{systemId: "
	    "SystemFar}]}\n",
	    "systems 'SystemAnvil' connects to 'SystemFar', which is no other system of the scenario");
	CheckRefused(
	    trading,
	    "version: 1\nsystems: !merge=systemId\n  - {systemId: SystemAnvil, connections: [{systemId: "
	    "SystemAnvil}]}\n",
	    "systems 'SystemAnvil' connects to 'SystemAnvil', which is no other system of the scenario");
	CheckRefused(
	    "manifest/definitions/systems/system-anvil.patch.yaml",
	    "version: 1\nbodies: !merge=name\n  - {name: VenueBrineHighport, parent: PlanetNowhere}\n",
	    "VenueBrineHighport");
	CheckRefused(
	    "manifest/definitions/systems/system-anvil.patch.yaml",
	    "version: 1\nbodies: !merge=name\n  - {name: VenueBrineHighport, parent: PlanetOre}\n",
	    "'VenueBrineHighport' station does not orbit its planet 'PlanetBrine'");
	CheckRefused(
	    "manifest/definitions/systems/system-anvil.patch.yaml",
	    "version: 1\nbodies: !merge=name\n  - {name: VenueBrineHighport, layout: highport}\n",
	    "'VenueBrineHighport' station names a layout of its own; the venue names it");
	CheckRefused(
	    "manifest/definitions/drives/hull-compact-single.patch.yaml", "version: 1\nexponent: 2\n",
	    "hull-compact-single");
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
	    trading, region("governorId: OrganisationNowhere"),
	    "'RegionBrineCoast' governor 'OrganisationNowhere' names no organisation");
	CheckRefused(trading, region("population: -1"), "'RegionBrineCoast' population must be finite");
	CheckRefused(trading, region("publicFunds: -1"), "'RegionBrineCoast' funds must be in [0, 2^62]");
	CheckRefused(trading, region("employerFunds: -1"), "'RegionBrineCoast' funds must be in [0, 2^62]");
	CheckRefused(
	    trading, region("householdFunds: 4611686018427387905"),
	    "'RegionBrineCoast' funds must be in [0, 2^62]");
	const auto ore = [](std::string_view market)
	{
		return std::format(
		    "version: 1\nvenues: !merge=venueId\n  - venueId: VenueOreHighport\n    market: {}\n",
		    market);
	};
	CheckRefused(
	    trading, ore("!merge=itemId\n      - {itemId: ItemCopper, regionId: ''}"),
	    "'VenueOreHighport' market 'ItemCopper' production names no region");
	CheckRefused(
	    trading, ore("!merge=itemId\n      - {itemId: ItemCopper, regionId: RegionBrineCoast}"),
	    "'VenueOreHighport' market 'ItemCopper' region 'RegionBrineCoast' is not one it serves");

	// Every region trades the essentials, at the least (W04).
	CheckRefused(
	    trading, ore("[{itemId: ItemCopper, regionId: RegionOreHighlands, production: 2, stock: 1}]"),
	    "'VenueOreHighport' serves a region but trades no 'ItemRice', which 'NeedStapleFood' needs");
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
	CheckRefused(trading, "version: 1\nstarterHull: ''\n", "starterHull is required");
	CheckRefused(
	    trading, "version: 1\nstarterVenue: VenueNowhere\n",
	    "starterVenue 'VenueNowhere' names none of the scenario's venues");


	const std::string farSystem = "manifest/definitions/systems/system-far.yaml";

	// A second system of the trading scenario's, a planet and a station in it.
	const std::string far =
	    "version: 1\nname: SystemFar\nbodies:\n  - {name: StarFar, kind: Star, radius: 7e8, "
	    "gravitationalParameter: 1.3e20}\n  - {name: PlanetFar, kind: Planet, parent: StarFar, radius: "
	    "6e6, gravitationalParameter: 4e14, orbit: {semiMajorAxis: 1e11}}\n  - {name: "
	    "VenueBrineHighport, kind: Station, parent: PlanetFar, radius: 763, orbit: {semiMajorAxis: "
	    "4.2e7}}\n";
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
	      "PlanetFar}\n"},
	     {farSystem, far}},
	    "'VenueBrineHighport' planet 'PlanetFar' is not in its system");
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
	      "VenueBrineHighport, systemId: SystemFar, planetId: ''}\n"},
	     {farSystem, far}},
	    "'VenueBrineHighport' region 'RegionBrineCoast' is not on its planet");

	// A port serves the regions of its own planet, not another in its system.
	CheckRefused(
	    {{trading, venue("planetId: PlanetOre")},
	     {"manifest/definitions/systems/system-anvil.patch.yaml",
	      "version: 1\nbodies: !merge=name\n  - {name: VenueBrineHighport, parent: PlanetOre}\n"}},
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
	    trading, venue("market: [{itemId: ItemSalt, stock: 40000000001}]"),
	    "'VenueBrineHighport' market 'ItemSalt' stock is more than a warehouse holds, 40000000000 "
	    "totes");

	// A grant's hull flies and fits a pad of its venue (W22b).
	CheckRefused(
	    trading,
	    "version: 1\ngrants: !merge=grantId\n  - {grantId: GrantPulseCarrier, venueId: "
	    "VenueOreHighport}\n",
	    "'GrantPulseCarrier' hull 'HullPulseCarrier' fits no pad of venue 'VenueOreHighport'");
	CheckRefused(
	    trading,
	    "version: 1\ngrants: !merge=grantId\n  - {grantId: GrantPulseCarrier, hullId: HullHauler}\n",
	    "'GrantPulseCarrier' hull 'HullHauler' names no hull with a mass to fly with");

	// The starter venue must be an operational public station that docks the starter and has life
	// support to spare (D-047, D-089); each lack alone refuses it. Its operator is one of the
	// scenario's organisations, none a player's, so it is neutral.
	constexpr std::string_view kNotStarter =
	    "starterVenue 'VenueOreHighport' must be an operational public station that docks "
	    "'HullLoneTraderStarter' with life support to spare";
	for (const std::string_view lack :
	     {"public: false", "operational: false", "lifeSupportCapacity: 0"})
	{
		CAPTURE(lack);
		CheckRefused(
		    trading,
		    std::format(
		        "version: 1\nvenues: !merge=venueId\n  - {{venueId: VenueOreHighport, {}}}\n", lack),
		    kNotStarter);
	}

	// A layout whose only pad is of a class the starter does not fit, by size though not by rank.
	CheckRefused(
	    {{"manifest/definitions/pads/tender.yaml",
	      "version: 2\nrank: 0\nlargestVehicle: [5, 3, 1]\nberthSize: [4, 6]\n"},
	     {"manifest/definitions/stations/tender-dock.yaml",
	      "version: 2\ndisplayName: Tender dock\ndecks: [{centre: [0, 0, 0], size: [40, 10, 40]}]\n"
	      "berths: [{class: tender, position: [0, 5, 0]}]\n"},
	     {trading, "version: 1\nvenues: !merge=venueId\n  - {venueId: VenueOreHighport, layout: "
	               "tender-dock}\n"},
	     {"manifest/definitions/tuning/balance.patch.yaml",
	      "version: 1\ncharges: {padClasses: !append [{padClass: tender}]}\n"}},
	    kNotStarter);

	// The scenario's traders take a pad and a connection each and leave a player one: four leave the
	// starter's four connections none.
	std::string traders = "version: 1\ntraders:\n";
	for (const std::string_view trader : {"Nia", "Oto", "Pim", "Quy"})
		traders +=
		    std::format("  - {{principalId: {0}, displayName: {0}, companyName: {0} Co}}\n", trader);
	CheckRefused(trading, traders, kNotStarter);
	CheckRefused(
	    trading,
	    "version: 1\ntraders:\n  - {principalId: Nia, displayName: Nia, companyName: Nia Co}\n"
	    "  - {principalId: Nia, displayName: Nia, companyName: Nia Co}\n",
	    "traders 'Nia' is given twice");
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

	// What a refusal names: the first definition held otherwise, or by one alone, or the scenario.
	CHECK(FirstDifference(base.manifest, base.manifest).empty());
	CHECK(FirstDifference(base.manifest, price.manifest) == "Manifest.Item item-copper");
	CHECK(
	    FirstDifference(base.manifest, added.manifest) ==
	    "Manifest.Age age-golden, which only this content has");
	CHECK(
	    FirstDifference(added.manifest, base.manifest) ==
	    "Manifest.Age age-golden, which only the world's content has");
	CHECK(
	    FirstDifference(
	        base.manifest, BuildContentIdentity(Loaded{kModDirectory}.registry, "trading").manifest) ==
	    "the scenario");
}


TEST_CASE("A cubic metre holds an item's units per cubic metre, a finite amount above 0 in every unit")
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

	// Copper by the tonne, 7.616 to the packed cubic metre: the starter's 160 totes hold 152.32 t.
	const auto copper = std::ranges::find(
	    content.gameMode.items, std::string{"ItemCopper"}, &Manifest::Core::ItemNode::itemId);
	REQUIRE(copper != content.gameMode.items.end());
	CHECK(copper->unitsPerCubicMetre == 7.616);
}
