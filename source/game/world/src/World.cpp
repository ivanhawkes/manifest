#include <manifest/world/World.h>

#include <algorithm>
#include <format>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

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
	world.RegisterComponent<Licences>();
	world.RegisterComponent<Claims>();
	world.RegisterComponent<Stock>();
	world.RegisterComponent<Cargo>();
	world.RegisterComponent<LifeSupport>();
	world.RegisterComponent<Fuel>();
	world.RegisterComponent<Refuel>();
	world.RegisterComponent<Systems>();
	world.RegisterComponent<Broker>();
	world.RegisterComponent<Order>();
	world.RegisterComponent<Production>();
	world.RegisterComponent<Pantry>();
	world.RegisterComponent<Sales>();
	world.RegisterComponent<Reports>();
	world.RegisterComponent<Fees>();
	world.RegisterComponent<Arrears>();
	world.RegisterComponent<Policy>();
	world.RegisterComponent<Development>();
	world.RegisterComponent<Job>();
	world.RegisterComponent<Pricing>();
	world.RegisterComponent<Paid>();
	world.RegisterComponent<Knowledge>();
	world.RegisterComponent<Facilities>();
	world.RegisterComponent<Project>();
	world.RegisterComponent<Tender>();
	world.RegisterComponent<Bid>();
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
		const entt::entity created = CreateOrRefuse(world, identities, Kind::Region, region.regionId);
		registry.emplace<Region>(
		    created, region.planetId, region.civilisationId, region.governorId, region.population,
		    region.health, region.happiness, content.tuning.economy.wageTax.initialBps);
		Knowledge &knowledge = registry.emplace<Knowledge>(created);
		for (const Content::SkillSelection &skill : region.skills)
			knowledge.skills.push_back(skill.skillId);
		std::ranges::sort(knowledge.skills);
		Facilities &facilities = registry.emplace<Facilities>(created);
		for (const Content::FacilityDefinition &facility : region.facilities)
			// The content set holds every facility to having a kind.
			facilities.facilities.push_back(
			    {facility.facilityId, facility.kind.value(), facility.operational});
		std::ranges::sort(facilities.facilities, {}, &Facility::facilityId);
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

		// Queued in the scenario's order, the first its head (D-110).
		std::uint64_t queued = 0;
		for (const Content::ProjectSelection &project : region.projects)
		{
			const entt::entity entity = CreateOrRefuse(
			    world, identities, Kind::Project, ProjectId(region.regionId, project.projectId));
			registry.emplace<Project>(
			    entity, region.regionId, project.projectId,
			    queued == 0 ? ProjectPhase::Tendering : ProjectPhase::Queued, 0, queued);
			registry.emplace<Stock>(entity);
			++queued;
		}
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
		const entt::entity market = CreateOrRefuse(world, identities, Kind::Market, venue.venueId);
		registry.emplace<Sales>(market);
		Fees &fees = registry.emplace<Fees>(
		    market, std::vector<ClassFees>{}, content.tuning.charges.berthFeePerOccupantHour,
		    content.tuning.charges.arrearsRateBps);
		for (const Content::ClassChargesTuning &charged : content.tuning.charges.padClasses)
			fees.classes.push_back({charged.padClass, charged.dockingFee, charged.berthFeePerHour});
		std::ranges::sort(fees.classes, {}, &ClassFees::padClass);
		registry.emplace<Pricing>(market, kDefaultLevelBps);
	}
}
} // namespace Manifest::World
