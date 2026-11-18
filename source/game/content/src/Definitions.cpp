#include <manifest/content/Definitions.h>

#include <format>
#include <string>
#include <vector>

#include <jettison/reflect/Reflect.h>
#include <jettison/sim/DefinitionRegistry.h>

namespace Manifest::Content
{
namespace
{
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
		using namespace Jettison::Reflect;
		constexpr Attribute kDefined = Attribute::Saved | Attribute::Moddable;

		RegisterEnum<TradeUnit>("Manifest.TradeUnit")
		    .Value<TradeUnit::Gram>("Gram")
		    .Value<TradeUnit::Kilogram>("Kilogram")
		    .Value<TradeUnit::Litre>("Litre")
		    .Value<TradeUnit::Kilolitre>("Kilolitre")
		    .Value<TradeUnit::Unit>("Unit")
		    .Value<TradeUnit::Sheet>("Sheet")
		    .Value<TradeUnit::SquareMetre>("SquareMetre");
		RegisterEnum<Core::NodeType>("Manifest.NodeType")
		    .Value<Core::NodeType::Tech>("Tech")
		    .Value<Core::NodeType::Skill>("Skill");

		RegisterType<AgeDefinition>("Manifest.Age", Attribute::Moddable)
		    .Field<&AgeDefinition::ageId>("ageId", kDefined)
		    .Field<&AgeDefinition::displayName>("displayName", kDefined)
		    .Field<&AgeDefinition::description>("description", kDefined);
		RegisterType<CategoryDefinition>("Manifest.Category", Attribute::Moddable)
		    .Field<&CategoryDefinition::categoryId>("categoryId", kDefined)
		    .Field<&CategoryDefinition::displayName>("displayName", kDefined)
		    .Field<&CategoryDefinition::type>("type", kDefined)
		    .Field<&CategoryDefinition::description>("description", kDefined);
		RegisterType<ItemDefinition>("Manifest.Item", Attribute::Moddable)
		    .Field<&ItemDefinition::itemId>("itemId", kDefined)
		    .Field<&ItemDefinition::categoryId>("categoryId", kDefined)
		    .Field<&ItemDefinition::ageId>("ageId", kDefined)
		    .Field<&ItemDefinition::displayName>("displayName", kDefined)
		    .Field<&ItemDefinition::description>("description", kDefined)
		    .Field<&ItemDefinition::unitsPerCubicMetre>("unitsPerCubicMetre", kDefined)
		    .Field<&ItemDefinition::basePrice>("basePrice", kDefined)
		    .Field<&ItemDefinition::unitType>("unitType", kDefined)
		    .Field<&ItemDefinition::density>("density", kDefined)
		    .Field<&ItemDefinition::packFactor>("packFactor", kDefined);
		RegisterType<Core::Prerequisite>("Manifest.Prerequisite")
		    .Field<&Core::Prerequisite::nodeId>("nodeId", kDefined)
		    .Field<&Core::Prerequisite::requiredTier>("requiredTier", kDefined);
		RegisterType<SkillDefinition>("Manifest.Skill", Attribute::Moddable)
		    .Field<&SkillDefinition::skillId>("skillId", kDefined)
		    .Field<&SkillDefinition::displayName>("displayName", kDefined)
		    .Field<&SkillDefinition::description>("description", kDefined)
		    .Field<&SkillDefinition::nodeType>("nodeType", kDefined)
		    .Field<&SkillDefinition::requiredSkills>("requires", kDefined)
		    .Field<&SkillDefinition::maxTiers>("maxTiers", kDefined)
		    .Field<&SkillDefinition::resourceCostPerTier>("resourceCostPerTier", kDefined)
		    .Field<&SkillDefinition::targetEffectID>("targetEffectID", kDefined)
		    .Field<&SkillDefinition::exclusiveGroupId>("exclusiveGroupId", kDefined)
		    .Field<&SkillDefinition::gridX>("gridX", kDefined)
		    .Field<&SkillDefinition::gridY>("gridY", kDefined);
		RegisterType<NamePoolDefinition>("Manifest.NamePool", Attribute::Moddable)
		    .Field<&NamePoolDefinition::pool>("pool", kDefined)
		    .Field<&NamePoolDefinition::names>("names", kDefined);

		RegisterType<AgeSelection>("Manifest.AgeSelection")
		    .Field<&AgeSelection::ageId>("ageId", kDefined);
		RegisterType<ItemSelection>("Manifest.ItemSelection")
		    .Field<&ItemSelection::itemId>("itemId", kDefined);
		RegisterType<SkillSelection>("Manifest.SkillSelection")
		    .Field<&SkillSelection::skillId>("skillId", kDefined);
		RegisterType<GameModeDefinition>("Manifest.GameMode", Attribute::Moddable)
		    .Field<&GameModeDefinition::ages>("ages", kDefined)
		    .Field<&GameModeDefinition::items>("items", kDefined)
		    .Field<&GameModeDefinition::skills>("skills", kDefined);
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
		    {"Manifest.Age", "manifest/definitions/ages", kVersion1, entt::resolve<AgeDefinition>(),
		     &IdOf<AgeDefinition, &AgeDefinition::ageId>},
		    {"Manifest.Category", "manifest/definitions/categories", kVersion1,
		     entt::resolve<CategoryDefinition>(),
		     &IdOf<CategoryDefinition, &CategoryDefinition::categoryId>},
		    {"Manifest.Item", "manifest/definitions/items", kVersion1, entt::resolve<ItemDefinition>(),
		     &IdOf<ItemDefinition, &ItemDefinition::itemId>},
		    {"Manifest.Skill", "manifest/definitions/skills", kVersion1,
		     entt::resolve<SkillDefinition>(), &IdOf<SkillDefinition, &SkillDefinition::skillId>},
		    {"Manifest.NamePool", "manifest/definitions/name-pools", kVersion1,
		     entt::resolve<NamePoolDefinition>(), &IdOf<NamePoolDefinition, &NamePoolDefinition::pool>},
		    {"Manifest.GameMode", "manifest/definitions/game-modes", kVersion1,
		     entt::resolve<GameModeDefinition>(), nullptr},
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


void LoadDefinitions(Jettison::Sim::DefinitionRegistry &registry)
{
	for (const DefinitionType &definition : DefinitionTypes())
	{
		registry.Load(definition.type, definition.directory, definition.version);
		if (definition.id == nullptr)
			continue;
		for (const std::string &name : registry.Names(definition.type))
		{
			const std::string_view id = definition.id(*registry.Find(definition.type, name));
			if (RegistryName(id) != name)
				throw Jettison::Serialise::FormatError{std::format(
				    "Definitions: {}/{}.yaml holds {} '{}', whose file would be {}.yaml",
				    definition.directory, name, definition.name, id, RegistryName(id))};
		}
	}
}
} // namespace Manifest::Content
