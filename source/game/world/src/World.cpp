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
	world.RegisterComponent<LifeSupport>();
	world.RegisterComponent<Systems>();
	world.RegisterComponent<Broker>();
	world.RegisterComponent<Order>();
	world.RegisterComponent<Production>();
	world.RegisterComponent<Pantry>();
	world.RegisterComponent<Sales>();
	world.RegisterComponent<Reports>();
	world.RegisterComponent<Policy>();
	world.RegisterComponent<Job>();
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
		    region.civilisationId, region.governorId, region.population, region.health,
		    region.happiness);
		CreateAccount(world, identities, Kind::PublicAccount, region.regionId, region.publicFunds);
		CreateAccount(
		    world, identities, Kind::HouseholdAccount, region.regionId, region.householdFunds);
		const entt::entity household =
		    CreateOrRefuse(world, identities, Kind::Household, region.regionId);
		registry.emplace<Stock>(household);
		registry.emplace<Pantry>(household);
		const entt::entity employer =
		    CreateOrRefuse(world, identities, Kind::Employer, region.regionId);
		registry.emplace<Stock>(employer);
		registry.emplace<Production>(employer);
		CreateAccount(world, identities, Kind::EmployerAccount, region.regionId, region.employerFunds);
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
		registry.emplace<Sales>(CreateOrRefuse(world, identities, Kind::Market, venue.venueId));
	}
}
} // namespace Manifest::World
