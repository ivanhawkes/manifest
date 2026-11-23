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
		case Kind::Tender: return "Tender";
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
		case Kind::Market:
		case Kind::Project:
		case Kind::TenderAccount:
		case Kind::Bid:
		case Kind::BondAccount: break;
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


void Identities::CheckProject(
    Jettison::Serialise::BinaryReader &reader, const Project &project, const Stock &stock) const
{
	const auto needs = m_projects->find(project.projectId);
	if (needs == m_projects->end())
		reader.Fail(std::format("'{}' names no project", project.projectId));
	const ProjectNeeds &needed = needs->second;
	const bool done = project.phase == ProjectPhase::Done;
	if (done ? project.worked != needed.workSteps : project.worked >= needed.workSteps)
		reader.Fail(
		    std::format(
		        "the project '{}' is worked {} of its {} steps", project.projectId, project.worked,
		        needed.workSteps));
	std::map<std::string, std::uint64_t, std::less<>> held;
	for (const Holding &holding : stock.holdings)
		held[holding.itemId] += holding.available + holding.reserved;
	for (const auto &[itemId, totes] : held)
	{
		const auto input = needed.inputs.find(itemId);
		if (project.phase != ProjectPhase::Tendering || input == needed.inputs.end() ||
		    totes > input->second)
			reader.Fail(
			    std::format(
			        "the project '{}' holds {} totes of '{}', more than it needs", project.projectId,
			        totes, itemId));
	}
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
	for (const auto [entity, fees] : loaded.view<const Fees>().each())
	{
		if (const std::optional<std::string> problem = Broken(fees))
			reader.Fail(*problem);
		if (!loaded.all_of<Pricing>(entity))
			reader.Fail("a venue's fees have no pricing level");
	}
	for (const auto [entity, arrears] : loaded.view<const Arrears>().each())
	{
		if (const std::optional<std::string> problem = Broken(arrears))
			reader.Fail(*problem);
		if (const auto *pShip = loaded.try_get<const Ship>(entity); !pShip || pShip->dockedAt.empty())
			reader.Fail("arrears are owed by no docked ship");
	}
	for (const auto [entity, policy] : loaded.view<const Policy>().each())
		if (const std::optional<std::string> problem = Broken(policy))
			reader.Fail(*problem);
	for (const auto [entity, job] : loaded.view<const Job>().each())
		if (const std::optional<std::string> problem = Broken(job))
			reader.Fail(*problem);
	for (const auto [entity, knowledge] : loaded.view<const Knowledge>().each())
		if (const std::optional<std::string> problem = Broken(knowledge))
			reader.Fail(*problem);
	for (const auto [entity, facilities] : loaded.view<const Facilities>().each())
		if (const std::optional<std::string> problem = Broken(facilities))
			reader.Fail(*problem);
	for (const auto [entity, project] : loaded.view<const Project>().each())
	{
		if (const std::optional<std::string> problem = Broken(project))
			reader.Fail(*problem);
		const auto *pStock = loaded.try_get<const Stock>(entity);
		if (pStock == nullptr)
			reader.Fail(std::format("the project '{}' holds no stock", project.projectId));
		const auto region = index.find({Kind::Region, project.regionId});
		if (region == index.end() || !loaded.all_of<Knowledge, Facilities>(region->second))
			reader.Fail(
			    std::format(
			        "the project '{}' is of '{}', a region the world does not hold", project.projectId,
			        project.regionId));
		if (m_projects)
			CheckProject(reader, project, *pStock);
	}
	for (const auto [entity, identity, tender] : loaded.view<const Identity, const Tender>().each())
	{
		if (const std::optional<std::string> problem = Broken(tender))
			reader.Fail(*problem);
		if (!index.contains({Kind::Region, tender.regionId}) ||
		    (!tender.projectId.empty() &&
		     !index.contains({Kind::Project, ProjectId(tender.regionId, tender.projectId)})))
			reader.Fail(
			    std::format(
			        "the tender '{}' is of a region or project the world does not hold", identity.id));
		const auto account = index.find({Kind::TenderAccount, identity.id});
		const auto *pAccount =
		    account == index.end() ? nullptr : loaded.try_get<const Account>(account->second);
		// Open, its account holds its budget; awarded, its winner's total less what it was paid.
		const bool open = tender.state == TenderState::Open;
		const bool awarded = tender.state == TenderState::Awarded;
		if (open || awarded
		        ? pAccount == nullptr ||
		              pAccount->balance != (open ? tender.budget : tender.price - Owed(tender))
		        : account != index.end())
			reader.Fail(
			    std::format(
			        "the tender '{}' reserves other than its budget while open, or its total less "
			        "what it paid once awarded",
			        identity.id));
		const auto bond = index.find({Kind::BondAccount, identity.id});
		const auto *pBond = bond == index.end() ? nullptr : loaded.try_get<const Account>(bond->second);
		if (awarded ? pBond == nullptr || pBond->balance != tender.bond ||
		                  !index.contains({Kind::Company, tender.companyId})
		            : bond != index.end())
			reader.Fail(
			    std::format(
			        "the tender '{}' holds other than its bond, or is awarded to no company",
			        identity.id));
	}
	for (const auto [entity, identity, account] : loaded.view<const Identity, const Account>().each())
		if ((identity.kind == Kind::TenderAccount || identity.kind == Kind::BondAccount) &&
		    !index.contains({Kind::Tender, identity.id}))
			reader.Fail(std::format("'{}' reserves money for no tender", identity.id));
	for (const auto [entity, bid] : loaded.view<const Bid>().each())
	{
		if (const std::optional<std::string> problem = Broken(bid))
			reader.Fail(*problem);
		const auto tender = index.find({Kind::Tender, bid.tenderId});
		const auto *pTender =
		    tender == index.end() ? nullptr : loaded.try_get<const Tender>(tender->second);
		if (pTender == nullptr || pTender->state != TenderState::Open || bid.total > pTender->budget ||
		    !index.contains({Kind::Company, bid.companyId}) ||
		    !index.contains({Kind::Bid, BidId(bid.tenderId, bid.companyId)}))
			reader.Fail(
			    std::format(
			        "a bid of '{}' on '{}' is of no company, no open tender or past its budget",
			        bid.companyId, bid.tenderId));
	}
	for (const auto [entity, paid] : loaded.view<const Paid>().each())
	{
		if (const std::optional<std::string> problem = Broken(paid))
			reader.Fail(*problem);
		if (!loaded.all_of<Company>(entity))
			reader.Fail("last prices are paid by no company");
	}
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
	for (const auto [entity, ship, fuel] : loaded.view<const Ship, const Fuel>().each())
	{
		if (const std::optional<std::string> problem = Broken(fuel))
			reader.Fail(*problem);
		if (!m_fuelTanks)
			continue;
		const std::optional<Fuel> tanks = m_fuelTanks(ship, loaded.try_get<Systems>(entity));
		if (!tanks || fuel.reactionMass > tanks->reactionMass || fuel.pellets > tanks->pellets)
			reader.Fail(
			    std::format(
			        "a ship of hull '{}' holds {} kg of reaction mass and {} kg of pellets, more than "
			        "its "
			        "tanks",
			        ship.hullId, fuel.reactionMass, fuel.pellets));
	}
	return [this, counters = std::move(counters), index = std::move(index)]() mutable
	{
		m_counters = std::move(counters);
		m_index = std::move(index);
	};
}
} // namespace Manifest::World
