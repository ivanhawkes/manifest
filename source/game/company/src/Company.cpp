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
#include <variant>
#include <vector>

#include <jettison/core/Assert.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/SimWorld.h>

#include <manifest/trade/Trade.h>
#include <manifest/world/State.h>

namespace Manifest::Company
{
namespace
{
using Command::Actor;
using Command::Result;
using Jettison::Sim::SimWorld;
using World::Kind;


constexpr double kSecondsPerDay = 86400.0;


// What the commands and the system run on: the fleet ships are parked through, and the scenario's
// items, the totes each hull holds, the hulls, each venue's life-support connections and how much of
// the life-support goods a tote holds, looked up once.
struct Rules
{
	World::Identities &identities;
	const Travel::Fleet &fleet;
	const Content::ContentSet &content;
	const Content::ScenarioDefinition &scenario;
	std::map<std::string, const Core::ItemNode *, std::less<>> items;
	std::map<std::string, std::uint64_t, std::less<>> holds;
	std::map<std::string, const Core::ShipClassSpec *, std::less<>> hulls;
	std::map<std::string, int, std::less<>> connections;
	double oxygenPerTote{0.0};
	double cartridgesPerTote{0.0};
	std::uint64_t windowSteps{0};
};


// The ships connected to the venue's life support.
int Connected(const entt::registry &registry, std::string_view venueId)
{
	int connected = 0;
	for (const auto [entity, lifeSupport] : registry.view<const World::LifeSupport>().each())
		connected += lifeSupport.connectedTo == venueId ? 1 : 0;
	return connected;
}


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
	const std::string &venueId = rules.scenario.starterVenue;
	const entt::entity pad = rules.fleet.FreePad(registry, venueId, rules.scenario.starterHull);
	if (pad == entt::null)
		return Result::NoPadFree;
	if (Connected(registry, venueId) >= rules.connections.at(venueId))
		return Result::NoServiceFree;

	const std::string companyId = Issued(identities, Kind::Company);
	const std::string shipId = Issued(identities, Kind::Ship);
	const entt::entity principal = Created(world, identities, Kind::Principal, provision.principalId);
	registry.emplace<World::Principal>(principal, provision.displayName);
	registry.emplace<World::Reports>(principal);
	Trade::Seed(world, identities, rules.content, principal, rules.windowSteps);
	const entt::entity company = Created(world, identities, Kind::Company, companyId);
	registry.emplace<World::Company>(company, provision.principalId, provision.companyName);
	registry.emplace<World::Stock>(company);
	registry.emplace<World::Account>(
	    Created(world, identities, Kind::CompanyAccount, companyId),
	    rules.content.tuning.company.startingCredits);
	const entt::entity ship = Created(world, identities, Kind::Ship, shipId);
	registry.emplace<World::Ship>(ship, companyId, rules.scenario.starterHull, venueId);
	registry.emplace<World::Cargo>(ship);
	const Core::ShipClassSpec &hull = *rules.hulls.at(rules.scenario.starterHull);
	registry.emplace<World::LifeSupport>(
	    ship, hull.oxygenTanks.value_or(std::vector<double>{}),
	    std::vector<double>(
	        static_cast<std::size_t>(hull.scrubberCartridges.value_or(0)),
	        rules.content.tuning.lifeSupport.cartridgeOccupantDays),
	    venueId);
	rules.fleet.Park(world, ship, pad, rules.scenario.starterHull, companyId);
	return Result::Succeeded;
}


auto HoldingKey(const World::Holding &holding)
{
	return std::tie(holding.venueId, holding.itemId);
}


// The sender's company's ship, or why a command on it is refused.
std::variant<entt::entity, Result> Owned(
    const entt::registry &registry, const Rules &rules, const Actor &actor, const std::string &shipId)
{
	const entt::entity ship = rules.identities.Find(Kind::Ship, shipId);
	if (ship == entt::null)
		return Result::UnknownTarget;
	const entt::entity company =
	    rules.identities.Find(Kind::Company, registry.get<World::Ship>(ship).companyId);
	JETTISON_VERIFY(company != entt::null);
	if (registry.get<World::Company>(company).principalId != actor.id)
		return Result::NotAuthorised;
	return ship;
}


Result Moved(SimWorld &world, const Rules &rules, const Actor &actor, const Move &move, bool aboard)
{
	entt::registry &registry = world.Registry();
	const auto owned = Owned(registry, rules, actor, move.shipId);
	if (const Result *refused = std::get_if<Result>(&owned))
		return *refused;
	const entt::entity shipEntity = std::get<entt::entity>(owned);
	const World::Ship &ship = registry.get<World::Ship>(shipEntity);
	const entt::entity company = rules.identities.Find(Kind::Company, ship.companyId);
	if (ship.dockedAt.empty())
		return Result::NotDocked;
	if (ship.dockedAt != move.venueId)
		return Result::WrongVenue;
	const auto item = rules.items.find(move.itemId);
	if (item == rules.items.end())
		return Result::UnknownTarget;
	if (move.totes == 0)
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
		if (!held || holding->available < move.totes)
			return Result::InsufficientStock;
		const auto hold = rules.holds.find(ship.hullId);
		std::uint64_t free = hold == rules.holds.end() ? 0 : hold->second;
		for (const World::Lot &aboardLot : cargo.lots)
			free -= std::min(free, aboardLot.totes);
		free -= std::min<std::uint64_t>(free, cargo.opened.size());
		if (move.totes > free)
			return Result::CapacityExceeded;

		holding->available -= move.totes;
		if (holding->available == 0 && holding->reserved == 0)
			stock.holdings.erase(holding);
		if (carried)
			lot->totes += move.totes;
		else
			cargo.lots.insert(lot, {move.itemId, move.totes});
		return Result::Succeeded;
	}

	if (!carried || lot->totes < move.totes)
		return Result::InsufficientStock;
	// A resting buy order keeps room for what it may still fill.
	std::uint64_t warehoused = held ? holding->available + holding->reserved : 0;
	for (const auto [entity, order] : registry.view<const World::Order>().each())
		if (order.side == World::Side::Buy && order.ownerKind == Kind::Company &&
		    order.ownerId == ship.companyId && order.venueId == move.venueId &&
		    order.itemId == move.itemId)
			warehoused += order.remaining;
	if (warehoused > Core::kMaxWarehouseTotes || move.totes > Core::kMaxWarehouseTotes - warehoused)
		return Result::CapacityExceeded;

	lot->totes -= move.totes;
	if (lot->totes == 0)
		cargo.lots.erase(lot);
	if (held)
		holding->available += move.totes;
	else
		stock.holdings.insert(holding, {move.venueId, move.itemId, move.totes, 0});
	return Result::Succeeded;
}


// How much of the item is aboard, in its unit: its opened tote's and its whole totes'.
double Aboard(const World::Cargo &cargo, std::string_view itemId, double perTote)
{
	double aboard = 0.0;
	if (const auto opened = std::ranges::find(cargo.opened, itemId, &World::Opened::itemId);
	    opened != cargo.opened.end())
		aboard = opened->quantity;
	if (const auto lot = std::ranges::find(cargo.lots, itemId, &World::Lot::itemId);
	    lot != cargo.lots.end())
		aboard += static_cast<double>(lot->totes) * perTote;
	return aboard;
}


// Takes the quantity of the item from the hold, at most Aboard's: its opened tote's first, then whole
// totes opened one at a time. What the last tote opened has left stays opened, unless it is below
// `least`, which is thrown away.
void Take(World::Cargo &cargo, const std::string &itemId, double perTote, double quantity, double least)
{
	const auto opened =
	    std::ranges::lower_bound(cargo.opened, itemId, std::less<>{}, &World::Opened::itemId);
	const bool wasOpened = opened != cargo.opened.end() && opened->itemId == itemId;
	double left = wasOpened ? opened->quantity : 0.0;
	const auto lot = std::ranges::find(cargo.lots, itemId, &World::Lot::itemId);
	while (left < quantity && lot != cargo.lots.end() && lot->totes > 0)
	{
		--lot->totes;
		left += perTote;
	}
	if (lot != cargo.lots.end() && lot->totes == 0)
		cargo.lots.erase(lot);
	left = std::max(0.0, left - quantity);
	if (left > 0.0 && left >= least)
	{
		if (wasOpened)
			opened->quantity = left;
		else
			cargo.opened.insert(opened, {itemId, left});
	}
	else if (wasOpened)
		cargo.opened.erase(opened);
}


Result Refilled(SimWorld &world, const Rules &rules, const Actor &actor, const Service &service)
{
	entt::registry &registry = world.Registry();
	const auto owned = Owned(registry, rules, actor, service.shipId);
	if (const Result *refused = std::get_if<Result>(&owned))
		return *refused;
	const entt::entity ship = std::get<entt::entity>(owned);
	World::LifeSupport &lifeSupport = registry.get<World::LifeSupport>(ship);
	const std::vector<double> capacities = rules.hulls.at(registry.get<World::Ship>(ship).hullId)
	                                           ->oxygenTanks.value_or(std::vector<double>{});
	JETTISON_VERIFY(capacities.size() == lifeSupport.tanks.size());
	double room = 0.0;
	for (std::size_t tank = 0; tank < capacities.size(); ++tank)
		room += capacities[tank] - lifeSupport.tanks[tank];
	if (room <= 0.0)
		return Result::CapacityExceeded;
	World::Cargo &cargo = registry.get<World::Cargo>(ship);
	const std::string &oxygen = rules.content.tuning.lifeSupport.oxygenItemId;
	const double aboard = Aboard(cargo, oxygen, rules.oxygenPerTote);
	if (aboard <= 0.0)
		return Result::InsufficientStock;

	double given = std::min(room, aboard);
	Take(cargo, oxygen, rules.oxygenPerTote, given, 0.0);
	for (std::size_t tank = 0; tank < capacities.size() && given > 0.0; ++tank)
	{
		const double space = capacities[tank] - lifeSupport.tanks[tank];
		const double poured = std::min(space, given);
		lifeSupport.tanks[tank] = poured == space ? capacities[tank] : lifeSupport.tanks[tank] + poured;
		given -= poured;
	}
	return Result::Succeeded;
}


Result Replaced(SimWorld &world, const Rules &rules, const Actor &actor, const Service &service)
{
	entt::registry &registry = world.Registry();
	const auto owned = Owned(registry, rules, actor, service.shipId);
	if (const Result *refused = std::get_if<Result>(&owned))
		return *refused;
	const entt::entity ship = std::get<entt::entity>(owned);
	World::LifeSupport &lifeSupport = registry.get<World::LifeSupport>(ship);
	if (lifeSupport.cartridges.empty())
		return Result::CapacityExceeded;
	World::Cargo &cargo = registry.get<World::Cargo>(ship);
	const std::string &cartridge = rules.content.tuning.lifeSupport.cartridgeItemId;
	if (Aboard(cargo, cartridge, rules.cartridgesPerTote) < 1.0)
		return Result::InsufficientStock;

	// A piece is a whole cartridge: less than one left of a tote is none.
	Take(cargo, cartridge, rules.cartridgesPerTote, 1.0, 1.0);
	*std::ranges::min_element(lifeSupport.cartridges) =
	    rules.content.tuning.lifeSupport.cartridgeOccupantDays;
	return Result::Succeeded;
}


// Draws the need from the reserve's tanks or cartridges in order, never more than they hold.
void Draw(std::vector<double> &reserve, double need)
{
	for (double &left : reserve)
	{
		const double drawn = std::min(left, need);
		left -= drawn;
		need -= drawn;
		if (need <= 0.0)
			return;
	}
}


// One step of every ship's life support (kLifeSupportSystem), in entity order.
void Breathe(entt::registry &registry, const Rules &rules, double step)
{
	std::vector<entt::entity> ships;
	for (const auto [ship, lifeSupport] : registry.view<const World::LifeSupport>().each())
		ships.push_back(ship);
	std::ranges::sort(ships, {}, [](entt::entity ship) { return entt::to_integral(ship); });

	std::map<std::string, int, std::less<>> connected;
	for (const entt::entity ship : ships)
	{
		World::LifeSupport &lifeSupport = registry.get<World::LifeSupport>(ship);
		if (lifeSupport.connectedTo.empty())
			continue;
		if (registry.get<const World::Ship>(ship).dockedAt == lifeSupport.connectedTo)
			++connected[lifeSupport.connectedTo];
		else
			lifeSupport.connectedTo.clear();
	}
	const Content::LifeSupportTuning &tuning = rules.content.tuning.lifeSupport;
	for (const entt::entity ship : ships)
	{
		World::LifeSupport &lifeSupport = registry.get<World::LifeSupport>(ship);
		const World::Ship &record = registry.get<const World::Ship>(ship);
		if (lifeSupport.connectedTo.empty() && !record.dockedAt.empty())
			if (int &taken = connected[record.dockedAt]; taken < rules.connections.at(record.dockedAt))
			{
				lifeSupport.connectedTo = record.dockedAt;
				++taken;
			}
		if (!lifeSupport.connectedTo.empty())
			continue;
		const double occupantDays =
		    rules.hulls.at(record.hullId)->occupants.value_or(0) * step / kSecondsPerDay;
		Draw(lifeSupport.tanks, occupantDays * tuning.oxygenKilogramsPerOccupantDay);
		Draw(lifeSupport.cartridges, occupantDays);
	}
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
		    .Field<&Move::totes>("totes", kSaved);
		RegisterType<Service>("Manifest.Company.Service").Field<&Service::shipId>("shipId", kSaved);
		return true;
	}();
	(void)registered;
}


void RegisterCommands(
    SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Travel::Fleet &fleet, const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	auto rules = std::make_shared<Rules>(identities, fleet, content, selected->second);
	rules->windowSteps = Trade::SalesWindowSteps(world, content);
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
			const double totes = volume->second;
			if (totes < 0.0 || totes > static_cast<double>(Core::kMaxHoldTotes) ||
			    std::trunc(totes) != totes)
				throw std::invalid_argument{std::format(
				    "hull '{}' has a CargoVolume of {}, not a whole number of totes up to {}",
				    hull.hullId, volume->second, Core::kMaxHoldTotes)};
			rules->holds.emplace(hull.hullId, static_cast<std::uint64_t>(totes));
		}
	identities.SetHolds(rules->holds);

	std::map<std::string, World::Installed, std::less<>> installed;
	for (const Core::ShipClassSpec &hull : content.hulls)
	{
		rules->hulls.emplace(hull.hullId, &hull);
		installed.emplace(
		    hull.hullId, World::Installed{
		                     hull.oxygenTanks.value_or(std::vector<double>{}),
		                     static_cast<std::size_t>(hull.scrubberCartridges.value_or(0))});
	}
	const Content::LifeSupportTuning &lifeSupport = content.tuning.lifeSupport;
	identities.SetLifeSupport(std::move(installed), lifeSupport.cartridgeOccupantDays);
	for (const Content::VenueDefinition &venue : selected->second.venues)
		rules->connections.emplace(venue.venueId, venue.lifeSupportCapacity);
	// Content holds both to items of the game mode traded in their units.
	const auto perTote = [&content](const std::string &itemId)
	{
		const auto item = std::ranges::find(content.gameMode.items, itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(item != content.gameMode.items.end());
		return item->unitsPerCubicMetre / static_cast<double>(Core::kTotesPerIbc);
	};
	rules->oxygenPerTote = perTote(lifeSupport.oxygenItemId);
	rules->cartridgesPerTote = perTote(lifeSupport.cartridgeItemId);

	requests.Register<Provision>(
	    std::string{kProvision}, kVersion, Command::ActorKind::Administrator,
	    [rules](SimWorld &target, const Actor & /*actor*/, const Provision &provision)
	    { return Provisioned(target, *rules, provision); });
	requests.Register<Move>(
	    std::string{kLoad}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Move &move)
	    { return Moved(target, *rules, actor, move, true); });
	requests.Register<Move>(
	    std::string{kUnload}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Move &move)
	    { return Moved(target, *rules, actor, move, false); });
	requests.Register<Service>(
	    std::string{kRefill}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Service &service)
	    { return Refilled(target, *rules, actor, service); });
	requests.Register<Service>(
	    std::string{kReplace}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Service &service)
	    { return Replaced(target, *rules, actor, service); });

	world.RegisterSystem(
	    {.name = std::string{kLifeSupportSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access =
	         Jettison::Ecs::Access::Of<World::LifeSupport, World::Ship>().Writing<World::LifeSupport>(),
	     .after = {std::string{Travel::kSystem}},
	     .run = [rules](entt::registry &registry, double step) { Breathe(registry, *rules, step); }});
}
} // namespace Manifest::Company
