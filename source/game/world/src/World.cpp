#include <manifest/world/World.h>

#include <algorithm>
#include <format>
#include <memory>
#include <stdexcept>
#include <utility>

namespace Manifest::World
{
namespace
{
entt::entity CreateOrRefuse(
    Jettison::Sim::SimWorld &world, Identities &identities, Kind kind, const std::string &id)
{
	const entt::entity created = identities.Create(world.Registry(), kind, id);
	if (created == entt::null)
		throw std::invalid_argument{std::format("the world already holds '{}'", id)};
	return created;
}


void CreateAccount(
    Jettison::Sim::SimWorld &world, Identities &identities, Kind kind, const std::string &ownerId,
    std::int64_t balance)
{
	world.Registry().emplace<Account>(CreateOrRefuse(world, identities, kind, ownerId), balance);
}
} // namespace


Identities &RegisterWorld(Jettison::Sim::SimWorld &world)
{
	world.RegisterComponent<Identity>();
	world.RegisterComponent<Principal>();
	world.RegisterComponent<Company>();
	world.RegisterComponent<Organisation>();
	world.RegisterComponent<Region>();
	world.RegisterComponent<Account>();
	world.RegisterComponent<Ship>();
	world.RegisterComponent<Journey>();
	world.RegisterComponent<Stock>();
	world.RegisterComponent<Cargo>();
	world.RegisterComponent<Broker>();
	world.RegisterComponent<Order>();
	auto identities = std::make_unique<Identities>();
	Identities &added = *identities;
	world.AddService("Manifest.Identities", std::move(identities));
	return added;
}


void CreateInitialState(
    Jettison::Sim::SimWorld &world, Identities &identities, const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	const Content::ScenarioDefinition &scenario = selected->second;
	entt::registry &registry = world.Registry();
	for (const Content::OrganisationDefinition &organisation : scenario.organisations)
	{
		// The content set holds every organisation to having a kind.
		registry.emplace<Organisation>(
		    CreateOrRefuse(world, identities, Kind::Organisation, organisation.organisationId),
		    organisation.kind.value(), organisation.displayName, "");
	}
	for (const Content::RegionDefinition &region : scenario.regions)
	{
		registry.emplace<Region>(
		    CreateOrRefuse(world, identities, Kind::Region, region.regionId), region.planetId,
		    region.civilisationId, region.governorId, region.playerHome, region.population,
		    region.health, region.happiness);
		CreateAccount(world, identities, Kind::PublicAccount, region.regionId, region.publicFunds);
		CreateAccount(
		    world, identities, Kind::HouseholdAccount, region.regionId, region.householdFunds);
	}
	for (const Content::VenueDefinition &venue : scenario.venues)
	{
		const entt::entity broker = CreateOrRefuse(world, identities, Kind::Broker, venue.venueId);
		registry.emplace<Broker>(broker, venue.operatorId);
		Stock &stock = registry.emplace<Stock>(broker);
		for (const Content::MarketEntry &entry : venue.market)
			if (entry.stock > 0)
				stock.holdings.push_back({venue.venueId, entry.itemId, entry.stock, 0});
		std::ranges::sort(stock.holdings, {}, &Holding::itemId);
		CreateAccount(
		    world, identities, Kind::BrokerAccount, venue.venueId, content.tuning.brokers.treasury);
		CreateAccount(world, identities, Kind::VenueAccount, venue.venueId, 0);
	}
}
} // namespace Manifest::World
