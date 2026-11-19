#include <manifest/company/Company.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>

#include <jettison/core/Assert.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/world/State.h>

namespace Manifest::Company
{
namespace
{
using Command::Actor;
using Command::Result;
using Jettison::Sim::SimWorld;
using World::Kind;


// What the commands run on: the scenario's items and the IBCs each hull holds, looked up once.
struct Rules
{
	World::Identities &identities;
	const Content::ContentSet &content;
	const Content::ScenarioDefinition &scenario;
	std::map<std::string, const Core::ItemNode *, std::less<>> items;
	std::map<std::string, std::uint64_t, std::less<>> holds;
};


// A new ID of the kind. An exhausted counter stops the step: no rejection can give back the IDs
// issued before it.
std::string Issued(World::Identities &identities, Kind kind)
{
	std::optional<std::string> issued = identities.Issue(kind);
	if (!issued)
		throw std::overflow_error{"an ID counter is exhausted"};
	return *std::move(issued);
}


entt::entity Created(SimWorld &world, World::Identities &identities, Kind kind, std::string id)
{
	const entt::entity created = identities.Create(world.Registry(), kind, std::move(id));
	JETTISON_VERIFY(created != entt::null);
	return created;
}


Result Provisioned(SimWorld &world, Rules &rules, const Provision &provision)
{
	if (provision.principalId.empty())
		return Result::MalformedPayload;
	World::Identities &identities = rules.identities;
	if (identities.Find(Kind::Principal, provision.principalId) != entt::null)
		return Result::AlreadyProvisioned;
	entt::registry &registry = world.Registry();
	const auto home = std::ranges::find_if(
	    rules.scenario.regions,
	    [&](const Content::RegionDefinition &region)
	    {
		    const entt::entity entity = identities.Find(Kind::Region, region.regionId);
		    return region.playerHome && entity != entt::null &&
		           registry.get<World::Region>(entity).governorId.empty();
	    });
	if (home == rules.scenario.regions.end())
		return Result::NoHomeRegion;

	const std::string companyId = Issued(identities, Kind::Company);
	const std::string polityId = Issued(identities, Kind::Organisation);
	const std::string shipId = Issued(identities, Kind::Ship);
	registry.emplace<World::Principal>(
	    Created(world, identities, Kind::Principal, provision.principalId), provision.displayName);
	const entt::entity company = Created(world, identities, Kind::Company, companyId);
	registry.emplace<World::Company>(company, provision.principalId, provision.companyName);
	registry.emplace<World::Stock>(company);
	registry.emplace<World::Account>(
	    Created(world, identities, Kind::CompanyAccount, companyId),
	    rules.content.tuning.company.startingCredits);
	registry.emplace<World::Organisation>(
	    Created(world, identities, Kind::Organisation, polityId), Content::OrganisationKind::Polity,
	    provision.displayName, provision.principalId);
	registry.get<World::Region>(identities.Find(Kind::Region, home->regionId)).governorId = polityId;
	const entt::entity ship = Created(world, identities, Kind::Ship, shipId);
	registry.emplace<World::Ship>(
	    ship, companyId, rules.scenario.starterHull, rules.content.homeVenues.at(home->regionId));
	registry.emplace<World::Cargo>(ship);
	return Result::Succeeded;
}


auto HoldingKey(const World::Holding &holding)
{
	return std::tie(holding.venueId, holding.itemId);
}


Result Moved(SimWorld &world, const Rules &rules, const Actor &actor, const Move &move, bool aboard)
{
	entt::registry &registry = world.Registry();
	const entt::entity shipEntity = rules.identities.Find(Kind::Ship, move.shipId);
	if (shipEntity == entt::null)
		return Result::UnknownTarget;
	const World::Ship &ship = registry.get<World::Ship>(shipEntity);
	const entt::entity company = rules.identities.Find(Kind::Company, ship.companyId);
	JETTISON_VERIFY(company != entt::null);
	if (registry.get<World::Company>(company).principalId != actor.id)
		return Result::NotAuthorised;
	if (ship.dockedAt.empty())
		return Result::NotDocked;
	if (ship.dockedAt != move.venueId)
		return Result::WrongVenue;
	const auto item = rules.items.find(move.itemId);
	if (item == rules.items.end())
		return Result::UnknownTarget;
	if (move.containers == 0)
		return Result::InvalidQuantity;

	World::Stock &stock = registry.get<World::Stock>(company);
	World::Cargo &cargo = registry.get<World::Cargo>(shipEntity);
	const World::Holding key{move.venueId, move.itemId};
	const auto holding = std::ranges::lower_bound(
	    stock.holdings, HoldingKey(key), std::less<>{},
	    [](const World::Holding &held) { return HoldingKey(held); });
	const bool held = holding != stock.holdings.end() && HoldingKey(*holding) == HoldingKey(key);
	const auto lot =
	    std::ranges::lower_bound(cargo.lots, move.itemId, std::less<>{}, &World::Lot::itemId);
	const bool carried = lot != cargo.lots.end() && lot->itemId == move.itemId;

	if (aboard)
	{
		if (!held || holding->available < move.containers)
			return Result::InsufficientStock;
		const auto hold = rules.holds.find(ship.hullId);
		std::uint64_t free = hold == rules.holds.end() ? 0 : hold->second;
		for (const World::Lot &aboardLot : cargo.lots)
			free -= std::min(free, aboardLot.containers);
		if (move.containers > free)
			return Result::CapacityExceeded;

		holding->available -= move.containers;
		if (holding->available == 0 && holding->reserved == 0)
			stock.holdings.erase(holding);
		if (carried)
			lot->containers += move.containers;
		else
			cargo.lots.insert(lot, {move.itemId, move.containers});
		return Result::Succeeded;
	}

	if (!carried || lot->containers < move.containers)
		return Result::InsufficientStock;
	const std::uint64_t warehoused = held ? holding->available + holding->reserved : 0;
	if (move.containers > Core::kMaxWarehouseIbcs - warehoused)
		return Result::CapacityExceeded;

	lot->containers -= move.containers;
	if (lot->containers == 0)
		cargo.lots.erase(lot);
	if (held)
		holding->available += move.containers;
	else
		stock.holdings.insert(holding, {move.venueId, move.itemId, move.containers, 0});
	return Result::Succeeded;
}
} // namespace


void RegisterTypes()
{
	static const bool registered = []
	{
		Command::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<Provision>("Manifest.Company.Provision")
		    .Field<&Provision::principalId>("principalId", kSaved)
		    .Field<&Provision::displayName>("displayName", kSaved)
		    .Field<&Provision::companyName>("companyName", kSaved);
		RegisterType<Move>("Manifest.Company.Move")
		    .Field<&Move::shipId>("shipId", kSaved)
		    .Field<&Move::venueId>("venueId", kSaved)
		    .Field<&Move::itemId>("itemId", kSaved)
		    .Field<&Move::containers>("containers", kSaved);
		return true;
	}();
	(void)registered;
}


void RegisterCommands(
    Command::Requests &requests, World::Identities &identities, const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	auto rules = std::make_shared<Rules>(identities, content, selected->second);
	for (const Content::ItemSelection &selection : selected->second.items)
	{
		const auto item =
		    std::ranges::find(content.gameMode.items, selection.itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(item != content.gameMode.items.end());
		rules->items.emplace(selection.itemId, &*item);
	}
	// Content refuses these too; a set built some other way is refused here before the cast.
	for (const Core::ShipClassSpec &hull : content.hulls)
		if (const auto volume = hull.baseStats.find(Core::EffectStat::CargoVolume);
		    volume != hull.baseStats.end())
		{
			// NaN fails the last comparison.
			if (volume->second < 0.0 || volume->second > static_cast<double>(Core::kMaxHoldIbcs) ||
			    std::trunc(volume->second) != volume->second)
				throw std::invalid_argument{std::format(
				    "hull '{}' has a CargoVolume of {}, not a whole number of IBCs up to {}",
				    hull.hullId, volume->second, Core::kMaxHoldIbcs)};
			rules->holds.emplace(hull.hullId, static_cast<std::uint64_t>(volume->second));
		}
	identities.SetHolds(rules->holds);

	requests.Register<Provision>(
	    std::string{kProvision}, kVersion, Command::ActorKind::Administrator,
	    [rules](SimWorld &world, const Actor & /*actor*/, const Provision &provision)
	    { return Provisioned(world, *rules, provision); });
	requests.Register<Move>(
	    std::string{kLoad}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &world, const Actor &actor, const Move &move)
	    { return Moved(world, *rules, actor, move, true); });
	requests.Register<Move>(
	    std::string{kUnload}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &world, const Actor &actor, const Move &move)
	    { return Moved(world, *rules, actor, move, false); });
}
} // namespace Manifest::Company
