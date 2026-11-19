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
		     {}});
	}
	std::ranges::sort(snapshot.companies, {}, &CompanyView::companyId);

	for (const auto &[entity, identity, ship] :
	     registry.view<const World::Identity, const World::Ship>().each())
	{
		const auto owner =
		    std::ranges::lower_bound(snapshot.companies, ship.companyId, {}, &CompanyView::companyId);
		if (owner != snapshot.companies.end() && owner->companyId == ship.companyId)
			owner->ships.push_back(
			    {identity.id, ship.hullId, ship.dockedAt, registry.get<World::Cargo>(entity).lots});
	}
	for (CompanyView &company : snapshot.companies)
		std::ranges::sort(company.ships, {}, &ShipView::shipId);
	return snapshot;
}
} // namespace Manifest::Query
