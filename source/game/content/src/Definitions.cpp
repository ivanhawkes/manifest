#include <manifest/content/Definitions.h>

#include <format>
#include <string>
#include <vector>

#include <jettison/dock/Definitions.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/DefinitionRegistry.h>

#include <manifest/effects/ShipEffectEngine.h>

namespace Manifest::Content
{
namespace
{
using Jettison::Serialise::YamlMigrations;

constexpr Jettison::Serialise::FormatVersion kVersion1{1};


template <typename T, auto Member> std::string_view IdOf(const entt::meta_any &definition)
{
	return definition.cast<const T &>().*Member;
}


bool IsUpper(char character)
{
	return character >= 'A' && character <= 'Z';
}


bool IsLowerOrDigit(char character)
{
	return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9');
}


bool IsLower(char character)
{
	return character >= 'a' && character <= 'z';
}
} // namespace


void RegisterTypes()
{
	static const bool registered = []
	{
		Jettison::Dock::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kDefined = Attribute::Saved | Attribute::Moddable;

		RegisterEnum<Core::TradeUnit>("Manifest.TradeUnit")
		    .Value<Core::TradeUnit::Carat>("Carat")
		    .Value<Core::TradeUnit::TroyOunce>("TroyOunce")
		    .Value<Core::TradeUnit::Kilogram>("Kilogram")
		    .Value<Core::TradeUnit::Tonne>("Tonne")
		    .Value<Core::TradeUnit::Litre>("Litre")
		    .Value<Core::TradeUnit::Kilolitre>("Kilolitre")
		    .Value<Core::TradeUnit::CubicMetre>("CubicMetre")
		    .Value<Core::TradeUnit::Barrel>("Barrel")
		    .Value<Core::TradeUnit::MMBtu>("MMBtu")
		    .Value<Core::TradeUnit::Unit>("Unit")
		    .Value<Core::TradeUnit::Sheet>("Sheet")
		    .Value<Core::TradeUnit::SquareMetre>("SquareMetre");
		RegisterEnum<Core::NodeType>("Manifest.NodeType")
		    .Value<Core::NodeType::Tech>("Tech")
		    .Value<Core::NodeType::Skill>("Skill");

		// Every stat, under its enumerator's name: a hull's baseStats are keyed by it.
		RegisterEnum<Core::EffectStat>("Manifest.EffectStat")
		    .Value<Core::EffectStat::ShieldFront>("ShieldFront")
		    .Value<Core::EffectStat::ShieldSide>("ShieldSide")
		    .Value<Core::EffectStat::ShieldRear>("ShieldRear")
		    .Value<Core::EffectStat::GunFront>("GunFront")
		    .Value<Core::EffectStat::GunRear>("GunRear")
		    .Value<Core::EffectStat::LaserFront>("LaserFront")
		    .Value<Core::EffectStat::LaserRear>("LaserRear")
		    .Value<Core::EffectStat::DriveThrust>("DriveThrust")
		    .Value<Core::EffectStat::ManoeuvreJets>("ManoeuvreJets")
		    .Value<Core::EffectStat::AttitudeControl>("AttitudeControl")
		    .Value<Core::EffectStat::SensorLongRange>("SensorLongRange")
		    .Value<Core::EffectStat::SensorFireControl>("SensorFireControl")
		    .Value<Core::EffectStat::SensorCommercial>("SensorCommercial")
		    .Value<Core::EffectStat::NavigationComputer>("NavigationComputer")
		    .Value<Core::EffectStat::TraderComputer>("TraderComputer")
		    .Value<Core::EffectStat::CargoVolume>("CargoVolume")
		    .Value<Core::EffectStat::FuelCapacity>("FuelCapacity")
		    .Value<Core::EffectStat::SalvageCapacity>("SalvageCapacity")
		    .Value<Core::EffectStat::LifeSupportDays>("LifeSupportDays")
		    .Value<Core::EffectStat::StealthRating>("StealthRating")
		    .Value<Core::EffectStat::EmissionsControl>("EmissionsControl")
		    .Value<Core::EffectStat::Countermeasures>("Countermeasures")
		    .Value<Core::EffectStat::CrewTraining>("CrewTraining")
		    .Value<Core::EffectStat::DamageControl>("DamageControl")
		    .Value<Core::EffectStat::SparesCapacity>("SparesCapacity")
		    .Value<Core::EffectStat::YardDockThroughput>("YardDockThroughput")
		    .Value<Core::EffectStat::YardFitSlots>("YardFitSlots")
		    .Value<Core::EffectStat::YardModularHullSlots>("YardModularHullSlots")
		    .Value<Core::EffectStat::YardRefitSlots>("YardRefitSlots")
		    .Value<Core::EffectStat::RouteRiskPremium>("RouteRiskPremium")
		    .Value<Core::EffectStat::SurveyRange>("SurveyRange")
		    .Value<Core::EffectStat::ClaimCount>("ClaimCount")
		    .Value<Core::EffectStat::RefineryThroughput>("RefineryThroughput")
		    .Value<Core::EffectStat::ClaimExclusivity>("ClaimExclusivity")
		    .Value<Core::EffectStat::MarketRiskPremium>("MarketRiskPremium")
		    .Value<Core::EffectStat::MarketClearance>("MarketClearance")
		    .Value<Core::EffectStat::MarketInsurance>("MarketInsurance")
		    .Value<Core::EffectStat::MarketDepth>("MarketDepth")
		    .Value<Core::EffectStat::MarketSpread>("MarketSpread")
		    .Value<Core::EffectStat::MarketFenceShare>("MarketFenceShare")
		    .Value<Core::EffectStat::BodyHabitatCapacity>("BodyHabitatCapacity")
		    .Value<Core::EffectStat::BodyRecyclingLoop>("BodyRecyclingLoop")
		    .Value<Core::EffectStat::TerraformAtmosphere>("TerraformAtmosphere")
		    .Value<Core::EffectStat::TerraformBiosphere>("TerraformBiosphere")
		    .Value<Core::EffectStat::MedicineCoverage>("MedicineCoverage")
		    .Value<Core::EffectStat::EducationCoverage>("EducationCoverage");
		RegisterType<Core::AgeNode>("Manifest.Age", Attribute::Moddable)
		    .Field<&Core::AgeNode::ageId>("ageId", kDefined)
		    .Field<&Core::AgeNode::displayName>("displayName", kDefined)
		    .Field<&Core::AgeNode::description>("description", kDefined);
		RegisterType<Core::CategoryNode>("Manifest.Category", Attribute::Moddable)
		    .Field<&Core::CategoryNode::categoryId>("categoryId", kDefined)
		    .Field<&Core::CategoryNode::displayName>("displayName", kDefined)
		    .Field<&Core::CategoryNode::type>("type", kDefined)
		    .Field<&Core::CategoryNode::description>("description", kDefined);
		RegisterType<Core::ItemNode>("Manifest.Item", Attribute::Moddable)
		    .Field<&Core::ItemNode::itemId>("itemId", kDefined)
		    .Field<&Core::ItemNode::categoryId>("categoryId", kDefined)
		    .Field<&Core::ItemNode::ageId>("ageId", kDefined)
		    .Field<&Core::ItemNode::displayName>("displayName", kDefined)
		    .Field<&Core::ItemNode::description>("description", kDefined)
		    .Field<&Core::ItemNode::unitsPerCubicMetre>("unitsPerCubicMetre", kDefined)
		    .Field<&Core::ItemNode::basePrice>("basePrice", kDefined)
		    .Field<&Core::ItemNode::unitType>("unitType", kDefined)
		    .Field<&Core::ItemNode::density>("density", kDefined)
		    .Field<&Core::ItemNode::packFactor>("packFactor", kDefined);
		RegisterType<Core::Prerequisite>("Manifest.Prerequisite")
		    .Field<&Core::Prerequisite::nodeId>("nodeId", kDefined)
		    .Field<&Core::Prerequisite::requiredTier>("requiredTier", kDefined);
		RegisterType<Core::SkillNode>("Manifest.Skill", Attribute::Moddable)
		    .Field<&Core::SkillNode::skillId>("skillId", kDefined)
		    .Field<&Core::SkillNode::displayName>("displayName", kDefined)
		    .Field<&Core::SkillNode::description>("description", kDefined)
		    .Field<&Core::SkillNode::nodeType>("nodeType", kDefined)
		    .Field<&Core::SkillNode::requiredSkills>("requires", kDefined)
		    .Field<&Core::SkillNode::maxTiers>("maxTiers", kDefined)
		    .Field<&Core::SkillNode::resourceCostPerTier>("resourceCostPerTier", kDefined)
		    .Field<&Core::SkillNode::targetEffectID>("targetEffectID", kDefined)
		    .Field<&Core::SkillNode::exclusiveGroupId>("exclusiveGroupId", kDefined)
		    .Field<&Core::SkillNode::gridX>("gridX", kDefined)
		    .Field<&Core::SkillNode::gridY>("gridY", kDefined);
		RegisterType<Core::ShipClassSpec>("Manifest.Hull", Attribute::Moddable)
		    .Field<&Core::ShipClassSpec::hullId>("hullId", kDefined)
		    .Field<&Core::ShipClassSpec::className>("className", kDefined)
		    .Field<&Core::ShipClassSpec::description>("description", kDefined)
		    .Field<&Core::ShipClassSpec::baseStats>("baseStats", kDefined)
		    .Field<&Core::ShipClassSpec::fitableEffects>("fitableEffects", kDefined)
		    .Field<&Core::ShipClassSpec::occupants>("occupants", kDefined)
		    .Field<&Core::ShipClassSpec::oxygenTanks>("oxygenTanks", kDefined)
		    .Field<&Core::ShipClassSpec::scrubberCartridges>("scrubberCartridges", kDefined);
		RegisterType<NamePoolDefinition>("Manifest.NamePool", Attribute::Moddable)
		    .Field<&NamePoolDefinition::pool>("pool", kDefined)
		    .Field<&NamePoolDefinition::names>("names", kDefined);

		RegisterType<AgeSelection>("Manifest.AgeSelection")
		    .Field<&AgeSelection::ageId>("ageId", kDefined);
		RegisterType<ItemSelection>("Manifest.ItemSelection")
		    .Field<&ItemSelection::itemId>("itemId", kDefined);
		RegisterType<SkillSelection>("Manifest.SkillSelection")
		    .Field<&SkillSelection::skillId>("skillId", kDefined);
		RegisterType<ItemQuantity>("Manifest.ItemQuantity")
		    .Field<&ItemQuantity::itemId>("itemId", kDefined)
		    .Field<&ItemQuantity::quantity>("quantity", kDefined);
		RegisterType<EconomyTuning>("Manifest.EconomyTuning")
		    .Field<&EconomyTuning::intervalSeconds>("intervalSeconds", kDefined)
		    .Field<&EconomyTuning::wageTaxRate>("wageTaxRate", kDefined);
		RegisterType<NeedDefinition>("Manifest.Need")
		    .Field<&NeedDefinition::needId>("needId", kDefined)
		    .Field<&NeedDefinition::weight>("weight", kDefined)
		    .Field<&NeedDefinition::perThousandPerDay>("perThousandPerDay", kDefined)
		    .Field<&NeedDefinition::unit>("unit", kDefined)
		    .Field<&NeedDefinition::items>("items", kDefined);
		RegisterType<ConditionTuning>("Manifest.ConditionTuning")
		    .Field<&ConditionTuning::initial>("initial", kDefined)
		    .Field<&ConditionTuning::healthLossPerHour>("healthLossPerHour", kDefined)
		    .Field<&ConditionTuning::happinessLossPerHour>("happinessLossPerHour", kDefined)
		    .Field<&ConditionTuning::healthRecoveryPerHour>("healthRecoveryPerHour", kDefined)
		    .Field<&ConditionTuning::happinessRecoveryPerHour>("happinessRecoveryPerHour", kDefined)
		    .Field<&ConditionTuning::workRateFloor>("workRateFloor", kDefined);
		RegisterType<LifeSupportTuning>("Manifest.LifeSupportTuning")
		    .Field<&LifeSupportTuning::oxygenKilogramsPerOccupantDay>(
		        "oxygenKilogramsPerOccupantDay", kDefined)
		    .Field<&LifeSupportTuning::cartridgeOccupantDays>("cartridgeOccupantDays", kDefined);
		RegisterType<ConcessionTuning>("Manifest.ConcessionTuning")
		    .Field<&ConcessionTuning::termHours>("termHours", kDefined);
		RegisterType<ProcurementTuning>("Manifest.ProcurementTuning")
		    .Field<&ProcurementTuning::performanceSecurityRate>("performanceSecurityRate", kDefined);
		RegisterType<ProjectDefinition>("Manifest.Project")
		    .Field<&ProjectDefinition::projectId>("projectId", kDefined)
		    .Field<&ProjectDefinition::requiredSkills>("requires", kDefined)
		    .Field<&ProjectDefinition::inputs>("inputs", kDefined)
		    .Field<&ProjectDefinition::unlocksRecipe>("unlocksRecipe", kDefined);
		RegisterType<RecipeDefinition>("Manifest.Recipe")
		    .Field<&RecipeDefinition::recipeId>("recipeId", kDefined)
		    .Field<&RecipeDefinition::inputs>("inputs", kDefined)
		    .Field<&RecipeDefinition::outputs>("outputs", kDefined);
		RegisterType<TuningDefinition>("Manifest.Tuning", Attribute::Moddable)
		    .Field<&TuningDefinition::economy>("economy", kDefined)
		    .Field<&TuningDefinition::needs>("needs", kDefined)
		    .Field<&TuningDefinition::condition>("condition", kDefined)
		    .Field<&TuningDefinition::lifeSupport>("lifeSupport", kDefined)
		    .Field<&TuningDefinition::concessions>("concessions", kDefined)
		    .Field<&TuningDefinition::procurement>("procurement", kDefined)
		    .Field<&TuningDefinition::projects>("projects", kDefined)
		    .Field<&TuningDefinition::recipes>("recipes", kDefined);
		RegisterType<GameModeDefinition>("Manifest.GameMode", Attribute::Moddable)
		    .Field<&GameModeDefinition::ages>("ages", kDefined)
		    .Field<&GameModeDefinition::items>("items", kDefined)
		    .Field<&GameModeDefinition::skills>("skills", kDefined);

		RegisterEnum<OrganisationKind>("Manifest.OrganisationKind")
		    .Value<OrganisationKind::Polity>("Polity")
		    .Value<OrganisationKind::Corporation>("Corporation");
		RegisterEnum<FacilityKind>("Manifest.FacilityKind").Value<FacilityKind::Workshop>("Workshop");
		RegisterType<SystemDefinition>("Manifest.System")
		    .Field<&SystemDefinition::systemId>("systemId", kDefined)
		    .Field<&SystemDefinition::displayName>("displayName", kDefined);
		RegisterType<PlanetDefinition>("Manifest.Planet")
		    .Field<&PlanetDefinition::planetId>("planetId", kDefined)
		    .Field<&PlanetDefinition::systemId>("systemId", kDefined)
		    .Field<&PlanetDefinition::displayName>("displayName", kDefined)
		    .Field<&PlanetDefinition::position>("position", kDefined);
		RegisterType<CivilisationDefinition>("Manifest.Civilisation")
		    .Field<&CivilisationDefinition::civilisationId>("civilisationId", kDefined)
		    .Field<&CivilisationDefinition::displayName>("displayName", kDefined)
		    .Field<&CivilisationDefinition::ageId>("ageId", kDefined);
		RegisterType<OrganisationDefinition>("Manifest.Organisation")
		    .Field<&OrganisationDefinition::organisationId>("organisationId", kDefined)
		    .Field<&OrganisationDefinition::kind>("kind", kDefined)
		    .Field<&OrganisationDefinition::displayName>("displayName", kDefined);
		RegisterType<FacilityDefinition>("Manifest.Facility")
		    .Field<&FacilityDefinition::facilityId>("facilityId", kDefined)
		    .Field<&FacilityDefinition::kind>("kind", kDefined)
		    .Field<&FacilityDefinition::operational>("operational", kDefined);
		RegisterType<ProjectSelection>("Manifest.ProjectSelection")
		    .Field<&ProjectSelection::projectId>("projectId", kDefined);
		RegisterType<RegionDefinition>("Manifest.Region")
		    .Field<&RegionDefinition::regionId>("regionId", kDefined)
		    .Field<&RegionDefinition::planetId>("planetId", kDefined)
		    .Field<&RegionDefinition::civilisationId>("civilisationId", kDefined)
		    .Field<&RegionDefinition::governorId>("governorId", kDefined)
		    .Field<&RegionDefinition::playerHome>("playerHome", kDefined)
		    .Field<&RegionDefinition::population>("population", kDefined)
		    .Field<&RegionDefinition::householdFunds>("householdFunds", kDefined)
		    .Field<&RegionDefinition::publicFunds>("publicFunds", kDefined)
		    .Field<&RegionDefinition::health>("health", kDefined)
		    .Field<&RegionDefinition::happiness>("happiness", kDefined)
		    .Field<&RegionDefinition::skills>("skills", kDefined)
		    .Field<&RegionDefinition::facilities>("facilities", kDefined)
		    .Field<&RegionDefinition::projects>("projects", kDefined);
		RegisterType<RegionSelection>("Manifest.RegionSelection")
		    .Field<&RegionSelection::regionId>("regionId", kDefined);
		RegisterType<MarketEntry>("Manifest.MarketEntry")
		    .Field<&MarketEntry::itemId>("itemId", kDefined)
		    .Field<&MarketEntry::production>("production", kDefined)
		    .Field<&MarketEntry::demand>("demand", kDefined)
		    .Field<&MarketEntry::stock>("stock", kDefined);
		RegisterType<VenueDefinition>("Manifest.Venue")
		    .Field<&VenueDefinition::venueId>("venueId", kDefined)
		    .Field<&VenueDefinition::systemId>("systemId", kDefined)
		    .Field<&VenueDefinition::planetId>("planetId", kDefined)
		    .Field<&VenueDefinition::position>("position", kDefined)
		    .Field<&VenueDefinition::operatorId>("operatorId", kDefined)
		    .Field<&VenueDefinition::isPublic>("public", kDefined)
		    .Field<&VenueDefinition::operational>("operational", kDefined)
		    .Field<&VenueDefinition::layout>("layout", kDefined)
		    .Field<&VenueDefinition::lifeSupportCapacity>("lifeSupportCapacity", kDefined)
		    .Field<&VenueDefinition::regions>("regions", kDefined)
		    .Field<&VenueDefinition::market>("market", kDefined);
		RegisterType<ScenarioDefinition>("Manifest.Scenario", Attribute::Moddable)
		    .Field<&ScenarioDefinition::gameMode>("gameMode", kDefined)
		    .Field<&ScenarioDefinition::starterHull>("starterHull", kDefined)
		    .Field<&ScenarioDefinition::items>("items", kDefined)
		    .Field<&ScenarioDefinition::systems>("systems", kDefined)
		    .Field<&ScenarioDefinition::planets>("planets", kDefined)
		    .Field<&ScenarioDefinition::civilisations>("civilisations", kDefined)
		    .Field<&ScenarioDefinition::organisations>("organisations", kDefined)
		    .Field<&ScenarioDefinition::regions>("regions", kDefined)
		    .Field<&ScenarioDefinition::venues>("venues", kDefined);
		return true;
	}();
	(void)registered;
}


std::span<const DefinitionType> DefinitionTypes()
{
	// Built after registration, since a meta_type is resolved from it.
	static const std::vector<DefinitionType> types = []
	{
		RegisterTypes();
		return std::vector<DefinitionType>{
		    {YamlMigrations{"Manifest.Age", kVersion1}, "manifest/definitions/ages",
		     entt::resolve<Core::AgeNode>(), &IdOf<Core::AgeNode, &Core::AgeNode::ageId>},
		    {YamlMigrations{"Manifest.Category", kVersion1}, "manifest/definitions/categories",
		     entt::resolve<Core::CategoryNode>(),
		     &IdOf<Core::CategoryNode, &Core::CategoryNode::categoryId>},
		    {YamlMigrations{"Manifest.Item", kVersion1}, "manifest/definitions/items",
		     entt::resolve<Core::ItemNode>(), &IdOf<Core::ItemNode, &Core::ItemNode::itemId>},
		    {YamlMigrations{"Manifest.Skill", kVersion1}, "manifest/definitions/skills",
		     entt::resolve<Core::SkillNode>(), &IdOf<Core::SkillNode, &Core::SkillNode::skillId>},
		    {YamlMigrations{"Manifest.Hull", kVersion1}, "manifest/definitions/hulls",
		     entt::resolve<Core::ShipClassSpec>(),
		     &IdOf<Core::ShipClassSpec, &Core::ShipClassSpec::hullId>},
		    {YamlMigrations{"Dock.PadClassDefinition", {Jettison::Dock::kDefinitionVersion}},
		     "manifest/definitions/pads", entt::resolve<Jettison::Dock::PadClassDefinition>(), nullptr},
		    {Jettison::Dock::HullMigrations(), "manifest/definitions/dock-hulls",
		     entt::resolve<Jettison::Dock::HullDefinition>(), nullptr},
		    {YamlMigrations{"Dock.StationLayout", {Jettison::Dock::kDefinitionVersion}},
		     "manifest/definitions/stations", entt::resolve<Jettison::Dock::StationLayout>(), nullptr},
		    {YamlMigrations{"Manifest.NamePool", kVersion1}, "manifest/definitions/name-pools",
		     entt::resolve<NamePoolDefinition>(), &IdOf<NamePoolDefinition, &NamePoolDefinition::pool>},
		    {YamlMigrations{"Manifest.GameMode", kVersion1}, "manifest/definitions/game-modes",
		     entt::resolve<GameModeDefinition>(), nullptr},
		    {YamlMigrations{"Manifest.Tuning", kVersion1}, "manifest/definitions/tuning",
		     entt::resolve<TuningDefinition>(), nullptr},
		    {YamlMigrations{"Manifest.Scenario", kVersion1}, "manifest/scenarios",
		     entt::resolve<ScenarioDefinition>(), nullptr},
		};
	}();
	return types;
}


std::string RegistryName(std::string_view id)
{
	std::string name;
	name.reserve(id.size() + 4);
	for (std::size_t index = 0; index < id.size(); ++index)
	{
		const char character = id[index];
		if (index > 0 && IsUpper(character))
		{
			const char previous = id[index - 1];
			const bool runEnds = IsUpper(previous) && index + 1 < id.size() && IsLower(id[index + 1]);
			if (IsLowerOrDigit(previous) || runEnds)
				name += '-';
		}
		name += IsUpper(character) ? static_cast<char>(character - 'A' + 'a') : character;
	}
	return name;
}


void LoadDefinitions(Jettison::Sim::DefinitionRegistry &registry, std::span<const DefinitionType> types)
{
	RegisterTypes();
	for (const DefinitionType &definition : types)
	{
		registry.Load(definition.type, definition.directory, definition.migrations);
		if (definition.id == nullptr)
			continue;
		for (const std::string &name : registry.Names(definition.type))
		{
			const std::string_view id = definition.id(*registry.Find(definition.type, name));
			if (RegistryName(id) != name)
				throw Jettison::Serialise::FormatError{std::format(
				    "Definitions: {}/{}.yaml holds {} '{}', whose file would be {}.yaml",
				    definition.directory, name, definition.migrations.Format(), id, RegistryName(id))};
		}
	}
}
} // namespace Manifest::Content
