#include <manifest/world/Identities.h>

#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <jettison/core/Assert.h>

namespace Manifest::World
{
namespace
{
std::string_view IssuedPrefix(Kind kind)
{
	switch (kind)
	{
		case Kind::Principal: return "Principal";
		case Kind::Company: return "Company";
		case Kind::Organisation: return "Organisation";
		case Kind::Region:
		case Kind::CompanyAccount:
		case Kind::PublicAccount:
		case Kind::HouseholdAccount: break;
	}
	return {};
}
} // namespace


entt::entity Identities::Create(entt::registry &registry, Kind kind, std::string id)
{
	if (id.empty() || m_index.contains({kind, id}))
		return entt::null;
	const entt::entity created = registry.create();
	registry.emplace<Identity>(created, kind, id);
	m_index.emplace(std::pair{kind, std::move(id)}, created);
	return created;
}


entt::entity Identities::Find(Kind kind, const std::string &id) const
{
	const auto found = m_index.find({kind, id});
	return found == m_index.end() ? entt::null : found->second;
}


std::optional<std::string> Identities::Issue(Kind kind)
{
	const std::string_view prefix = IssuedPrefix(kind);
	JETTISON_VERIFY(!prefix.empty());
	std::uint64_t &issued = m_counters.issued[kind];
	while (issued < std::numeric_limits<std::uint64_t>::max())
	{
		std::string id = std::format("{}{}", prefix, ++issued);
		if (!m_index.contains({kind, id}))
			return id;
	}
	return std::nullopt;
}


void Identities::Save(Jettison::Serialise::BinaryWriter &writer) const
{
	writer.Write(m_counters);
}


std::function<void()> Identities::Load(
    Jettison::Serialise::BinaryReader &reader, const entt::registry &loaded)
{
	IdentityCounters counters;
	reader.Read(counters);
	std::map<std::pair<Kind, std::string>, entt::entity> index;
	for (const auto [entity, identity] : loaded.view<const Identity>().each())
	{
		if (identity.id.empty())
			reader.Fail("an entity's identity has no ID");
		if (!index.emplace(std::pair{identity.kind, identity.id}, entity).second)
			reader.Fail(std::format("two entities are named '{}'", identity.id));
	}
	return [this, counters = std::move(counters), index = std::move(index)]() mutable
	{
		m_counters = std::move(counters);
		m_index = std::move(index);
	};
}
} // namespace Manifest::World
