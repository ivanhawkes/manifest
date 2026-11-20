#include <manifest/query/Query.h>

#include <algorithm>

#include <jettison/core/Assert.h>

namespace Manifest::Query
{
Snapshot Observe(
    const Jettison::Sim::SimWorld &world, const World::Identities &identities,
    const Command::Actor &actor)
{
	const entt::registry &registry = world.Registry();
	Snapshot snapshot{world.Clock().Step(), {}};
	for (const auto &[entity, identity, company] :
	     registry.view<const World::Identity, const World::Company>().each())
	{
		if (actor.kind != Command::ActorKind::Administrator && company.principalId != actor.id)
			continue;
		const entt::entity account = identities.Find(World::Kind::CompanyAccount, identity.id);
		JETTISON_VERIFY(account != entt::null);
		snapshot.companies.push_back(
		    {identity.id,
		     company.displayName,
		     registry.get<World::Account>(account).balance,
		     registry.get<World::Stock>(entity).holdings,
		     {},
		     {}});
	}
	std::ranges::sort(snapshot.companies, {}, &CompanyView::companyId);

	for (const auto &[entity, identity, ship] :
	     registry.view<const World::Identity, const World::Ship>().each())
	{
		const auto owner =
		    std::ranges::lower_bound(snapshot.companies, ship.companyId, {}, &CompanyView::companyId);
		if (owner != snapshot.companies.end() && owner->companyId == ship.companyId)
		{
			const auto *pJourney = registry.try_get<World::Journey>(entity);
			const auto *pSystems = registry.try_get<World::Systems>(entity);
			const World::Cargo &cargo = registry.get<World::Cargo>(entity);
			owner->ships.push_back(
			    {identity.id, ship.hullId, ship.dockedAt, cargo.lots,
			     pJourney ? std::optional{*pJourney} : std::nullopt, cargo.opened,
			     registry.get<World::LifeSupport>(entity), identities.Hold(ship, pSystems).value_or(0),
			     pSystems ? pSystems->fitted : std::vector<World::Fitting>{}});
		}
	}
	for (const auto &[entity, identity, order] :
	     registry.view<const World::Identity, const World::Order>().each())
	{
		if (order.ownerKind != World::Kind::Company)
			continue;
		const auto owner =
		    std::ranges::lower_bound(snapshot.companies, order.ownerId, {}, &CompanyView::companyId);
		if (owner != snapshot.companies.end() && owner->companyId == order.ownerId)
			owner->orders.push_back({identity.id, order});
	}
	for (CompanyView &company : snapshot.companies)
	{
		std::ranges::sort(company.ships, {}, &ShipView::shipId);
		std::ranges::sort(
		    company.orders, {}, [](const OrderView &view) { return view.order.sequence; });
	}

	const entt::entity principal = actor.kind == Command::ActorKind::Principal
	                                   ? identities.Find(World::Kind::Principal, actor.id)
	                                   : entt::null;
	if (const auto *pReports =
	        principal == entt::null ? nullptr : registry.try_get<World::Reports>(principal))
		for (const World::Report &report : pReports->reports)
			snapshot.markets.push_back(
			    {report, report.source == World::Source::Docked && report.observed == snapshot.step});
	return snapshot;
}
} // namespace Manifest::Query
