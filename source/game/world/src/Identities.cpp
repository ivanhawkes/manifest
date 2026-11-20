#include <manifest/world/Identities.h>

#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>

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
		case Kind::Ship: return "Ship";
		case Kind::Order: return "Order";
		case Kind::Broker:
		case Kind::BrokerAccount:
		case Kind::VenueAccount:
		case Kind::Region:
		case Kind::CompanyAccount:
		case Kind::PublicAccount:
		case Kind::HouseholdAccount:
		case Kind::Employer:
		case Kind::EmployerAccount:
		case Kind::Household:
		case Kind::Market: break;
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


void Identities::Destroy(entt::registry &registry, entt::entity entity)
{
	const Identity &identity = registry.get<const Identity>(entity);
	JETTISON_VERIFY(m_index.erase({identity.kind, identity.id}) == 1);
	registry.destroy(entity);
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
    Jettison::Serialise::BinaryReader &reader, const entt::registry &loaded,
    std::span<const entt::meta_any> /*queued*/)
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
	for (const auto [entity, stock] : loaded.view<const Stock>().each())
		if (const std::optional<std::string> problem = Broken(stock))
			reader.Fail(*problem);
	for (const auto [entity, cargo] : loaded.view<const Cargo>().each())
		if (const std::optional<std::string> problem = Broken(cargo))
			reader.Fail(*problem);
	for (const auto [entity, systems] : loaded.view<const Systems>().each())
		if (const std::optional<std::string> problem = Broken(systems))
			reader.Fail(*problem);
	for (const auto [entity, production] : loaded.view<const Production>().each())
		if (const std::optional<std::string> problem = Broken(production))
			reader.Fail(*problem);
	for (const auto [entity, pantry] : loaded.view<const Pantry>().each())
		if (const std::optional<std::string> problem = Broken(pantry))
			reader.Fail(*problem);
	for (const auto [entity, sales] : loaded.view<const Sales>().each())
		if (const std::optional<std::string> problem = Broken(sales))
			reader.Fail(*problem);
	for (const auto [entity, reports] : loaded.view<const Reports>().each())
	{
		if (const std::optional<std::string> problem = Broken(reports))
			reader.Fail(*problem);
		for (const Report &report : reports.reports)
			if (!index.contains({Kind::Market, report.venueId}))
				reader.Fail(
				    std::format(
				        "a market report is of '{}', a venue the world does not hold", report.venueId));
	}
	// Every reserved tote backs the sell orders of its owner there, and the reverse.
	std::map<std::tuple<Kind, std::string, std::string, std::string>, std::uint64_t> reserved;
	for (const auto [entity, order] : loaded.view<const Order>().each())
	{
		if (const std::optional<std::string> problem = Broken(order))
			reader.Fail(*problem);
		const Kind account = AccountOf(order.ownerKind);
		if (!index.contains({order.ownerKind, order.ownerId}) ||
		    !index.contains({account, order.ownerId}) ||
		    !index.contains({Kind::VenueAccount, order.venueId}))
			reader.Fail(
			    std::format(
			        "order {} names an owner or venue the world does not hold", order.sequence));
		if (order.side == Side::Sell)
			reserved[{order.ownerKind, order.ownerId, order.venueId, order.itemId}] += order.remaining;
	}
	for (const auto [entity, identity, stock] : loaded.view<const Identity, const Stock>().each())
		for (const Holding &holding : stock.holdings)
		{
			const auto backed =
			    reserved.find({identity.kind, identity.id, holding.venueId, holding.itemId});
			const std::uint64_t ordered = backed == reserved.end() ? 0 : backed->second;
			if (holding.reserved != ordered)
				reader.Fail(
				    std::format(
				        "'{}' reserves {} totes of '{}' at '{}' for sell orders of {}", identity.id,
				        holding.reserved, holding.itemId, holding.venueId, ordered));
			if (backed != reserved.end())
				reserved.erase(backed);
		}
	if (!reserved.empty())
		reader.Fail("a sell order's totes are reserved in no holding of its owner");
	if (m_holds)
		for (const auto [entity, ship, cargo] : loaded.view<const Ship, const Cargo>().each())
		{
			// Broken has bounded the sum, so it cannot wrap.
			std::uint64_t carried = 0;
			for (const Lot &lot : cargo.lots)
				carried += lot.totes;
			carried += cargo.opened.size();
			const std::optional<std::uint64_t> hold = m_holds(ship, loaded.try_get<Systems>(entity));
			if (!hold)
				reader.Fail(
				    std::format(
				        "a ship of hull '{}' has systems it cannot carry, or no hold", ship.hullId));
			else if (carried > *hold)
				reader.Fail(
				    std::format(
				        "a ship of hull '{}' carries {} totes, more than its hold", ship.hullId,
				        carried));
		}
	for (const auto [entity, ship, lifeSupport] : loaded.view<const Ship, const LifeSupport>().each())
	{
		if (const std::optional<std::string> problem = Broken(lifeSupport))
			reader.Fail(*problem);
		if (!lifeSupport.connectedTo.empty() &&
		    !index.contains({Kind::Broker, lifeSupport.connectedTo}))
			reader.Fail(
			    std::format(
			        "a ship is connected to '{}', a venue the world does not hold",
			        lifeSupport.connectedTo));
		if (!m_installed)
			continue;
		const auto installed = m_installed->find(ship.hullId);
		const Installed none;
		const Installed &hull = installed == m_installed->end() ? none : installed->second;
		bool over = lifeSupport.tanks.size() != hull.tanks.size() ||
		            lifeSupport.cartridges.size() != hull.cartridges;
		for (std::size_t tank = 0; !over && tank < hull.tanks.size(); ++tank)
			over = lifeSupport.tanks[tank] > hull.tanks[tank];
		for (const double cartridge : lifeSupport.cartridges)
			over = over || cartridge > m_freshCartridge;
		if (over)
			reader.Fail(
			    std::format(
			        "a ship of hull '{}' holds tanks or cartridges its hull does not, or more in them",
			        ship.hullId));
	}
	return [this, counters = std::move(counters), index = std::move(index)]() mutable
	{
		m_counters = std::move(counters);
		m_index = std::move(index);
	};
}
} // namespace Manifest::World
