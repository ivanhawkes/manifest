#include <manifest/company/Company.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

#include <jettison/core/Assert.h>
#include <jettison/dock/Docks.h>
#include <jettison/ecs/Scheduler.h>
#include <jettison/physics/Components.h>
#include <jettison/reflect/Reflect.h>
#include <jettison/sim/SimWorld.h>
#include <jettison/sim/Snapshot.h>

#include <manifest/content/Content.h>
#include <manifest/effects/ShipEffectEngine.h>
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
constexpr double kSecondsPerHour = 3600.0;
constexpr std::int64_t kMaxCredits = std::numeric_limits<std::int64_t>::max();


// What the commands and the system run on: the fleet ships are parked through, the effects engine
// that says what a hull's systems make of it, and the scenario's items, the hulls, the game mode's
// skills, the offers of them, each venue's life-support connections and how much of the life-support
// goods a tote holds, looked up once.
struct Rules
{
	World::Identities &identities;
	const Travel::Fleet &fleet;
	const Content::ContentSet &content;
	const Content::ScenarioDefinition &scenario;
	Core::ShipEffectEngine effects{};
	std::map<std::string, const Core::ItemNode *, std::less<>> items;
	std::map<std::string, const Core::ShipClassSpec *, std::less<>> hulls;
	std::map<std::string, const Core::SkillNode *, std::less<>> skills;
	std::map<std::string, const Content::OfferDefinition *, std::less<>> offers;
	std::map<std::string, int, std::less<>> connections;
	double oxygenPerTote{0.0};
	double cartridgesPerTote{0.0};
	double reactionMassPerTote{0.0};
	double pelletsPerTote{0.0};
	// What refuelling buys through (Trade::MakeBuyer).
	Trade::Buyer buy{};
	std::uint64_t windowSteps{0};
	// A simulation hour in the world's steps, which the berth fee is charged by.
	std::uint64_t hourSteps{0};
};


// Whether a ship of the hull can carry the systems: each a Skill node of the game mode, of a ship
// effect, at most its maxTiers, the effect and the exclusive group of none another's, the hull with a
// slot for each effect, and each prerequisite fitted among them at its tier. What Upgrade fits, and
// what a save may hold.
bool Fits(const Rules &rules, const Core::ShipClassSpec &hull, const World::Systems &systems)
{
	const auto tierOf = [&systems](std::string_view skillId)
	{
		const auto fitted = std::ranges::find(systems.fitted, skillId, &World::Fitting::skillId);
		return fitted == systems.fitted.end() ? 0 : fitted->tier;
	};
	std::set<std::string_view> effects;
	std::set<std::string_view> groups;
	for (const World::Fitting &fitting : systems.fitted)
	{
		const auto found = rules.skills.find(fitting.skillId);
		if (found == rules.skills.end())
			return false;
		const Core::SkillNode &skill = *found->second;
		const Core::EffectDefinition *pEffect = rules.effects.Catalogue().Find(skill.targetEffectID);
		if (skill.nodeType != Core::NodeType::Skill || pEffect == nullptr ||
		    pEffect->domain != Core::EffectDomain::Ship)
			return false;
		const bool prepared = std::ranges::all_of(
		    skill.requiredSkills, [&tierOf](const Core::Prerequisite &prerequisite)
		    { return tierOf(prerequisite.nodeId) >= prerequisite.requiredTier; });
		const bool excluded =
		    !skill.exclusiveGroupId.empty() && !groups.insert(skill.exclusiveGroupId).second;
		if (fitting.tier > skill.maxTiers || !effects.insert(skill.targetEffectID).second || excluded ||
		    std::ranges::find(hull.fitableEffects, skill.targetEffectID) == hull.fitableEffects.end() ||
		    !prepared)
			return false;
	}
	return true;
}


// A ship's stat: its hull's with each system's effect at its tier, 0 for one it does not list; none
// for systems the hull cannot carry (Fits). A hull the content does not have has none of any, and
// carries no system.
std::optional<double> ShipStat(
    const Rules &rules, const World::Ship &ship, const World::Systems *pSystems, Core::EffectStat stat)
{
	const World::Systems none;
	const World::Systems &systems = pSystems ? *pSystems : none;
	const auto hull = rules.hulls.find(ship.hullId);
	if (hull == rules.hulls.end())
		return systems.fitted.empty() ? std::optional<double>{0.0} : std::nullopt;
	if (!Fits(rules, *hull->second, systems))
		return std::nullopt;
	std::map<std::string, int> tiers;
	for (const World::Fitting &fitting : systems.fitted)
		tiers[rules.skills.at(fitting.skillId)->targetEffectID] = fitting.tier;
	const std::map<Core::EffectStat, double> stats =
	    rules.effects.Stats(hull->second->baseStats, tiers);
	const auto found = stats.find(stat);
	return found == stats.end() ? 0.0 : found->second;
}


// The ship's hold (World::Identities::HoldOf): its CargoVolume (ShipStat).
std::optional<std::uint64_t> Hold(
    const Rules &rules, const World::Ship &ship, const World::Systems *pSystems)
{
	const std::optional<double> volume = ShipStat(rules, ship, pSystems, Core::EffectStat::CargoVolume);
	if (!volume)
		return std::nullopt;
	const double totes = *volume;
	// NaN fails the last comparison.
	if (totes < 0.0 || totes > static_cast<double>(Core::kMaxHoldTotes) || std::trunc(totes) != totes)
		return std::nullopt;
	return static_cast<std::uint64_t>(totes);
}


// The most fuel the ship holds (World::Identities::FuelTanksOf): its reaction mass tanks, its
// FuelCapacity (ShipStat) in tonnes, and its hull's pellet magazine; none for a capacity below 0 or
// not finite.
std::optional<World::Fuel> FuelTanks(
    const Rules &rules, const World::Ship &ship, const World::Systems *pSystems)
{
	constexpr double kKilogramsPerTonne = 1000.0;
	const std::optional<double> tonnes =
	    ShipStat(rules, ship, pSystems, Core::EffectStat::FuelCapacity);
	// NaN fails the comparison.
	if (!tonnes || !(*tonnes >= 0.0) || !std::isfinite(*tonnes))
		return std::nullopt;
	const auto hull = rules.hulls.find(ship.hullId);
	return World::Fuel{
	    *tonnes * kKilogramsPerTonne,
	    hull == rules.hulls.end() ? 0.0 : hull->second->pelletMagazine.value_or(0.0)};
}


// The totes aboard, its opened totes taking a tote each.
std::uint64_t Carried(const World::Cargo &cargo)
{
	// World::Broken bounds the sum, so it cannot wrap.
	std::uint64_t carried = cargo.opened.size();
	for (const World::Lot &lot : cargo.lots)
		carried += lot.totes;
	return carried;
}


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
	const std::optional<World::Fuel> tanks = FuelTanks(rules, registry.get<World::Ship>(ship), nullptr);
	JETTISON_VERIFY(tanks.has_value());
	registry.emplace<World::Fuel>(ship, *tanks);
	rules.fleet.Park(world, ship, pad, rules.scenario.starterHull, companyId);
	return Result::Succeeded;
}


// Whether the principal directs the governor of a region.
bool Governs(const entt::registry &registry, const Rules &rules, const std::string &principalId)
{
	for (const auto [entity, region] : registry.view<const World::Region>().each())
	{
		if (region.governorId.empty())
			continue;
		const entt::entity governor = rules.identities.Find(Kind::Organisation, region.governorId);
		JETTISON_VERIFY(governor != entt::null);
		if (registry.get<const World::Organisation>(governor).principalId == principalId)
			return true;
	}
	return false;
}


Result Founded(SimWorld &world, const Rules &rules, const Actor &actor, const Found &found)
{
	entt::registry &registry = world.Registry();
	World::Identities &identities = rules.identities;
	const entt::entity region = identities.Find(Kind::Region, found.regionId);
	const entt::entity company = identities.Find(Kind::Company, found.companyId);
	if (region == entt::null || company == entt::null)
		return Result::UnknownTarget;
	if (registry.get<const World::Company>(company).principalId != actor.id)
		return Result::NotAuthorised;
	if (found.displayName.empty())
		return Result::MalformedPayload;
	if (!registry.get<const World::Region>(region).governorId.empty())
		return Result::Governed;
	if (Governs(registry, rules, actor.id))
		return Result::AlreadyFounded;
	World::Account &paying =
	    registry.get<World::Account>(identities.Find(Kind::CompanyAccount, found.companyId));
	World::Account &treasury =
	    registry.get<World::Account>(identities.Find(Kind::PublicAccount, found.regionId));
	const std::int64_t price = rules.content.tuning.company.foundingPrice;
	if (paying.balance < price)
		return Result::InsufficientFunds;
	if (treasury.balance > kMaxCredits - price)
		return Result::CapacityExceeded;

	const std::string organisationId = Issued(identities, Kind::Organisation);
	registry.emplace<World::Organisation>(
	    Created(world, identities, Kind::Organisation, organisationId),
	    Content::OrganisationKind::Polity, found.displayName, actor.id);
	registry.get<World::Region>(region).governorId = organisationId;
	paying.balance -= price;
	treasury.balance += price;
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
		std::uint64_t free =
		    rules.identities.Hold(ship, registry.try_get<World::Systems>(shipEntity)).value_or(0);
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
// `least`, which is thrown away. The totes used up, raised as Trade::Consumed.
void Take(
    SimWorld &world, World::Cargo &cargo, const std::string &itemId, double perTote, double quantity,
    double least)
{
	const auto opened =
	    std::ranges::lower_bound(cargo.opened, itemId, std::less<>{}, &World::Opened::itemId);
	const bool wasOpened = opened != cargo.opened.end() && opened->itemId == itemId;
	double left = wasOpened ? opened->quantity : 0.0;
	const auto lot = std::ranges::find(cargo.lots, itemId, &World::Lot::itemId);
	const std::uint64_t before = lot == cargo.lots.end() ? 0 : lot->totes;
	while (left < quantity && lot != cargo.lots.end() && lot->totes > 0)
	{
		--lot->totes;
		left += perTote;
	}
	std::uint64_t used = (wasOpened ? 1 : 0) + (lot == cargo.lots.end() ? 0 : before - lot->totes);
	if (lot != cargo.lots.end() && lot->totes == 0)
		cargo.lots.erase(lot);
	left = std::max(0.0, left - quantity);
	if (left > 0.0 && left >= least)
	{
		--used;
		if (wasOpened)
			opened->quantity = left;
		else
			cargo.opened.insert(opened, {itemId, left});
	}
	else if (wasOpened)
		cargo.opened.erase(opened);
	if (used > 0)
		world.Events().Emit(Trade::Consumed{itemId, used});
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
	Take(world, cargo, oxygen, rules.oxygenPerTote, given, 0.0);
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
	Take(world, cargo, cartridge, rules.cartridgesPerTote, 1.0, 1.0);
	*std::ranges::min_element(lifeSupport.cartridges) =
	    rules.content.tuning.lifeSupport.cartridgeOccupantDays;
	return Result::Succeeded;
}


// A journey's refuelling at the venue the ship is docked at (SetRefuel), within its company's account
// less the reserve.
void Refuelled(SimWorld &world, const Rules &rules, entt::entity ship, std::int64_t reserve)
{
	entt::registry &registry = world.Registry();
	if (const auto *pRefuel = registry.try_get<const World::Refuel>(ship); pRefuel && !pRefuel->enabled)
		return;
	const World::Ship &record = registry.get<World::Ship>(ship);
	const std::optional<World::Fuel> tanks =
	    rules.identities.FuelTanks(record, registry.try_get<World::Systems>(ship));
	if (!tanks || !registry.all_of<World::Fuel>(ship) || record.dockedAt.empty())
		return;
	const entt::entity company = rules.identities.Find(Kind::Company, record.companyId);
	const entt::entity account = rules.identities.Find(Kind::CompanyAccount, record.companyId);
	JETTISON_VERIFY(company != entt::null && account != entt::null);
	const auto budget = [&] { return registry.get<World::Account>(account).balance - reserve; };
	const Content::FuelTuning &fuel = rules.content.tuning.fuel;

	// Whole totes of reaction mass, straight from the warehouse into the tanks.
	const double room = tanks->reactionMass - registry.get<World::Fuel>(ship).reactionMass;
	if (const auto totes = static_cast<std::uint64_t>(std::clamp(
	        std::floor(room / rules.reactionMassPerTote), 0.0,
	        static_cast<double>(Core::kMaxWarehouseTotes)));
	    totes > 0)
		if (const std::uint64_t bought = rules.buy(
		        world, record.companyId, record.dockedAt, fuel.reactionMassItemId, totes, budget());
		    bought > 0)
		{
			World::Stock &stock = registry.get<World::Stock>(company);
			const World::Holding key{record.dockedAt, fuel.reactionMassItemId};
			const auto holding = std::ranges::lower_bound(
			    stock.holdings, HoldingKey(key), std::less<>{},
			    [](const World::Holding &held) { return HoldingKey(held); });
			JETTISON_VERIFY(
			    holding != stock.holdings.end() && HoldingKey(*holding) == HoldingKey(key) &&
			    holding->available >= bought);
			holding->available -= bought;
			if (holding->available == 0 && holding->reserved == 0)
				stock.holdings.erase(holding);
			world.Events().Emit(Trade::Consumed{fuel.reactionMassItemId, bought});
			World::Fuel &held = registry.get<World::Fuel>(ship);
			held.reactionMass = std::min(
			    tanks->reactionMass,
			    held.reactionMass + (static_cast<double>(bought) * rules.reactionMassPerTote));
		}

	// A tote of pellets into a hold with none and room for it, loaded as Load would.
	const std::uint64_t free =
	    rules.identities.Hold(record, registry.try_get<World::Systems>(ship)).value_or(0);
	if (Aboard(registry.get<World::Cargo>(ship), fuel.pelletItemId, rules.pelletsPerTote) <= 0.0 &&
	    Carried(registry.get<World::Cargo>(ship)) < free &&
	    rules.buy(world, record.companyId, record.dockedAt, fuel.pelletItemId, 1, budget()) > 0)
	{
		const Actor owner{
		    Command::ActorKind::Principal, registry.get<World::Company>(company).principalId};
		const Move move{registry.get<World::Identity>(ship).id, record.dockedAt, fuel.pelletItemId, 1};
		JETTISON_VERIFY(Moved(world, rules, owner, move, true) == Result::Succeeded);
	}

	// The magazine filled from the hold, a tote opened at a time.
	World::Cargo &cargo = registry.get<World::Cargo>(ship);
	World::Fuel &held = registry.get<World::Fuel>(ship);
	const double space = tanks->pellets - held.pellets;
	if (const double given = std::min(space, Aboard(cargo, fuel.pelletItemId, rules.pelletsPerTote));
	    given > 0.0)
	{
		Take(world, cargo, fuel.pelletItemId, rules.pelletsPerTote, given, 0.0);
		held.pellets = given == space ? tanks->pellets : held.pellets + given;
	}
}


Result RefuelSet(SimWorld &world, const Rules &rules, const Actor &actor, const SetRefuel &set)
{
	entt::registry &registry = world.Registry();
	const auto owned = Owned(registry, rules, actor, set.shipId);
	if (const Result *refused = std::get_if<Result>(&owned))
		return *refused;
	registry.emplace_or_replace<World::Refuel>(std::get<entt::entity>(owned), set.enabled);
	return Result::Succeeded;
}


Result Upgraded(SimWorld &world, const Rules &rules, const Actor &actor, const Upgrade &upgrade)
{
	entt::registry &registry = world.Registry();
	const auto owned = Owned(registry, rules, actor, upgrade.shipId);
	if (const Result *refused = std::get_if<Result>(&owned))
		return *refused;
	const entt::entity shipEntity = std::get<entt::entity>(owned);
	const auto offer = rules.offers.find(upgrade.offerId);
	if (offer == rules.offers.end())
		return Result::UnknownTarget;
	const World::Ship &ship = registry.get<World::Ship>(shipEntity);
	if (ship.dockedAt.empty())
		return Result::NotDocked;
	if (upgrade.tier == 0)
		return Result::InvalidQuantity;

	const Content::OfferDefinition &terms = *offer->second;
	const Core::SkillNode &skill = *rules.skills.at(terms.skillId);
	const World::Systems *pSystems = registry.try_get<World::Systems>(shipEntity);
	World::Systems systems = pSystems ? *pSystems : World::Systems{};
	const auto tierOf = [&systems](std::string_view skillId)
	{
		const auto fitted = std::ranges::find(systems.fitted, skillId, &World::Fitting::skillId);
		return fitted == systems.fitted.end() ? 0 : fitted->tier;
	};
	const auto fitted = static_cast<std::uint32_t>(tierOf(skill.skillId));
	if (upgrade.tier <= fitted)
		return Result::AlreadyFitted;
	if (upgrade.tier != fitted + 1 || upgrade.tier > static_cast<std::uint32_t>(terms.maxTier))
		return Result::NotFittable;

	const auto at =
	    std::ranges::lower_bound(systems.fitted, skill.skillId, {}, &World::Fitting::skillId);
	if (at != systems.fitted.end() && at->skillId == skill.skillId)
		at->tier = static_cast<int>(upgrade.tier);
	else
		systems.fitted.insert(at, {skill.skillId, static_cast<int>(upgrade.tier)});
	const auto hull = rules.hulls.find(ship.hullId);
	if (hull == rules.hulls.end() || !Fits(rules, *hull->second, systems))
		return Result::NotFittable;
	// A system that took room away could leave the cargo aboard more than the hold.
	const std::optional<std::uint64_t> hold = Hold(rules, ship, &systems);
	if (!hold || *hold < Carried(registry.get<World::Cargo>(shipEntity)))
		return Result::CapacityExceeded;
	World::Account &account =
	    registry.get<World::Account>(rules.identities.Find(Kind::CompanyAccount, ship.companyId));
	if (account.balance < terms.cost)
		return Result::InsufficientFunds;

	account.balance -= terms.cost;
	registry.emplace_or_replace<World::Systems>(shipEntity, std::move(systems));
	return Result::Succeeded;
}


// Draws the need from the reserve's tanks or cartridges in order, never more than they hold.
double Draw(std::vector<double> &reserve, double need)
{
	double drawn = 0.0;
	for (double &left : reserve)
	{
		const double each = std::min(left, need);
		left -= each;
		need -= each;
		drawn += each;
		if (need <= 0.0)
			break;
	}
	return drawn;
}


// One step's use drawn from the ship's own tanks and cartridges, added to its job while the job's lot
// is aboard (D-100).
void DrawStep(
    entt::registry &registry, entt::entity ship, World::LifeSupport &lifeSupport, double occupantDays,
    double oxygenPerOccupantDay)
{
	const double oxygen = Draw(lifeSupport.tanks, occupantDays * oxygenPerOccupantDay);
	const double cartridges = Draw(lifeSupport.cartridges, occupantDays);
	if (auto *pJob = registry.try_get<World::Job>(ship); pJob && pJob->totes > 0)
	{
		pJob->oxygenKilograms += oxygen;
		pJob->cartridgeDays += cartridges;
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
		DrawStep(registry, ship, lifeSupport, occupantDays, tuning.oxygenKilogramsPerOccupantDay);
	}
}


// The first step life support has to run in (Jettison::Sim::Quiet): the next, while a connection
// would be lost or made, as Breathe makes them; otherwise never, each step only drawing.
std::uint64_t BreatheNext(const SimWorld &world, const Rules &rules)
{
	const entt::registry &registry = world.Registry();
	std::map<std::string, int, std::less<>> connected;
	for (const auto [ship, lifeSupport, record] :
	     registry.view<const World::LifeSupport, const World::Ship>().each())
	{
		if (lifeSupport.connectedTo.empty())
			continue;
		if (record.dockedAt != lifeSupport.connectedTo)
			return world.Clock().Step();
		++connected[lifeSupport.connectedTo];
	}
	for (const auto [ship, lifeSupport, record] :
	     registry.view<const World::LifeSupport, const World::Ship>().each())
		if (lifeSupport.connectedTo.empty() && !record.dockedAt.empty() &&
		    connected[record.dockedAt] < rules.connections.at(record.dockedAt))
			return world.Clock().Step();
	return std::numeric_limits<std::uint64_t>::max();
}


// What every step passed over drew, each a Draw, as Breathe draws it.
void BreatheSkip(entt::registry &registry, const Rules &rules, double step, std::uint64_t passed)
{
	const Content::LifeSupportTuning &tuning = rules.content.tuning.lifeSupport;
	for (const auto [ship, lifeSupport, record] :
	     registry.view<World::LifeSupport, const World::Ship>().each())
	{
		if (!lifeSupport.connectedTo.empty())
			continue;
		const double occupantDays =
		    rules.hulls.at(record.hullId)->occupants.value_or(0) * step / kSecondsPerDay;
		for (std::uint64_t each = 0; each < passed; ++each)
			DrawStep(registry, ship, lifeSupport, occupantDays, tuning.oxygenKilogramsPerOccupantDay);
	}
}
// SetFees, from the principal directing the venue's operator, checked before it writes.
Result FeesSet(SimWorld &world, const Rules &rules, const Actor &actor, const SetFees &set)
{
	entt::registry &registry = world.Registry();
	const entt::entity broker = rules.identities.Find(Kind::Broker, set.venueId);
	const entt::entity market = rules.identities.Find(Kind::Market, set.venueId);
	if (broker == entt::null || market == entt::null)
		return Result::UnknownTarget;
	const entt::entity organisation =
	    rules.identities.Find(Kind::Organisation, registry.get<const World::Broker>(broker).operatorId);
	if (organisation == entt::null)
		return Result::NotAuthorised;
	const std::string &director = registry.get<const World::Organisation>(organisation).principalId;
	if (director.empty() || director != actor.id)
		return Result::NotAuthorised;
	World::Fees &posted = registry.get<World::Fees>(market);
	const World::Fees &fees = set.fees;
	if (!std::ranges::equal(
	        fees.classes, posted.classes, {}, &World::ClassFees::padClass,
	        &World::ClassFees::padClass) ||
	    std::ranges::any_of(
	        fees.classes,
	        [](const World::ClassFees &each) { return each.docking < 0 || each.berthPerHour < 0; }) ||
	    fees.berthPerOccupantHour < 0 || fees.arrearsRateBps < 0)
		return Result::InvalidQuantity;
	if (fees.arrearsRateBps > rules.content.tuning.charges.maxArrearsRateBps)
		return Result::RateTooHigh;
	posted = fees;
	return Result::Succeeded;
}


// A day's interest on the arrears at the yearly rate in basis points, compounded: added to what is
// owed in whole credits, the part of a credit left carried to the next day.
void Accrue(World::Arrears &arrears, int rateBps)
{
	const std::int64_t part = arrears.owed % World::kRateYear * rateBps + arrears.carry;
	const std::int64_t interest = arrears.owed / World::kRateYear * rateBps + part / World::kRateYear;
	arrears.owed += std::min(interest, kMaxCredits - arrears.owed);
	arrears.carry = part % World::kRateYear;
}


// The fee at the level, in basis points of it, to the nearest credit, a half up; at most kMaxCredits.
std::int64_t AtLevel(std::int64_t fee, int levelBps)
{
	constexpr std::int64_t kWhole = World::kDefaultLevelBps;
	if (levelBps > 0 && fee / kWhole >= kMaxCredits / levelBps)
		return kMaxCredits;
	return fee / kWhole * levelBps + (fee % kWhole * levelBps + kWhole / 2) / kWhole;
}


// Each AI-run venue's level and fees by its station's occupancy as the day's last step ends
// (kPricingSystem), in the scenario's order of venues.
void Price(SimWorld &world, const Rules &rules)
{
	if ((world.Clock().Step() + 1) % (rules.hourSteps * 24) != 0)
		return;
	entt::registry &registry = world.Registry();
	const Content::ChargesTuning &charges = rules.content.tuning.charges;
	const Content::PricingTuning &rule = charges.pricing;
	for (const Content::VenueDefinition &venue : rules.scenario.venues)
	{
		const entt::entity organisation = rules.identities.Find(
		    Kind::Organisation,
		    registry.get<const World::Broker>(rules.identities.Find(Kind::Broker, venue.venueId))
		        .operatorId);
		if (organisation != entt::null &&
		    !registry.get<const World::Organisation>(organisation).principalId.empty())
			continue;
		const entt::entity station = rules.fleet.StationOf(registry, venue.venueId);
		if (station == entt::null)
			continue;
		std::int64_t pads = 0;
		std::int64_t taken = 0;
		for (const entt::entity pad : registry.get<const Jettison::Dock::Platform>(station).berths)
			if (const auto &held = registry.get<const Jettison::Dock::Berth>(pad); !held.display)
			{
				++pads;
				taken += held.state != Jettison::Dock::BerthState::Free ? 1 : 0;
			}
		if (pads == 0)
			continue;
		const entt::entity market = rules.identities.Find(Kind::Market, venue.venueId);
		int &level = registry.get<World::Pricing>(market).levelBps;
		if (taken * World::kDefaultLevelBps > rule.raiseAboveBps * pads)
			level += rule.stepBps;
		else if (taken * World::kDefaultLevelBps < rule.lowerBelowBps * pads)
			level -= rule.stepBps;
		level = std::clamp(level, rule.minLevelBps, rule.maxLevelBps);
		World::Fees &fees = registry.get<World::Fees>(market);
		for (World::ClassFees &each : fees.classes)
		{
			const auto authored = std::ranges::find(
			    charges.padClasses, each.padClass, &Content::ClassChargesTuning::padClass);
			JETTISON_VERIFY(authored != charges.padClasses.end());
			each.docking = AtLevel(authored->dockingFee, level);
			each.berthPerHour = AtLevel(authored->berthFeePerHour, level);
		}
		fees.berthPerOccupantHour = AtLevel(charges.berthFeePerOccupantHour, level);
		fees.arrearsRateBps = charges.arrearsRateBps;
	}
}


// The first step pricing has to run in (Jettison::Sim::Quiet): the day's last.
std::uint64_t PriceNext(const SimWorld &world, const Rules &rules)
{
	const std::uint64_t day = rules.hourSteps * 24;
	const std::uint64_t now = world.Clock().Step();
	return now + (day - 1 - now % day);
}


// The hour's berth fee of every docked ship, in entity order, as the hour's last step ends
// (kBerthSystem), on the day's last its arrears' interest first.
void Berth(SimWorld &world, const Rules &rules)
{
	if ((world.Clock().Step() + 1) % rules.hourSteps != 0)
		return;
	entt::registry &registry = world.Registry();
	std::vector<entt::entity> ships;
	for (const auto [ship, record] : registry.view<const World::Ship>().each())
		if (!record.dockedAt.empty())
			ships.push_back(ship);
	std::ranges::sort(ships, {}, [](entt::entity ship) { return entt::to_integral(ship); });
	const bool dayEnds = (world.Clock().Step() + 1) % (rules.hourSteps * 24) == 0;
	for (const entt::entity ship : ships)
	{
		const World::Ship &record = registry.get<const World::Ship>(ship);
		const entt::entity market = rules.identities.Find(Kind::Market, record.dockedAt);
		JETTISON_VERIFY(market != entt::null);
		const World::Fees &fees = registry.get<const World::Fees>(market);
		const World::ClassFees *pClass =
		    World::Find(fees, Content::FeeClass(rules.content, record.hullId));
		JETTISON_VERIFY(pClass != nullptr);
		// The occupants' share while the venue's life support keeps them (D-047, D-100).
		const auto *pSupport = registry.try_get<const World::LifeSupport>(ship);
		const std::int64_t occupants = pSupport && pSupport->connectedTo == record.dockedAt
		                                   ? rules.hulls.at(record.hullId)->occupants.value_or(0)
		                                   : 0;
		const std::int64_t share =
		    occupants == 0 ? 0
		                   : std::min(fees.berthPerOccupantHour, kMaxCredits / occupants) * occupants;
		const std::int64_t fee =
		    pClass->berthPerHour + std::min(share, kMaxCredits - pClass->berthPerHour);
		const auto *pArrears = registry.try_get<const World::Arrears>(ship);
		World::Arrears arrears = pArrears ? *pArrears : World::Arrears{};
		if (dayEnds)
			Accrue(
			    arrears, std::min(fees.arrearsRateBps, rules.content.tuning.charges.maxArrearsRateBps));
		const std::int64_t owed = arrears.owed;
		const std::int64_t due = owed + std::min(fee, kMaxCredits - owed);
		World::Account &account =
		    registry.get<World::Account>(rules.identities.Find(Kind::CompanyAccount, record.companyId));
		World::Account &venue =
		    registry.get<World::Account>(rules.identities.Find(Kind::VenueAccount, record.dockedAt));
		const std::int64_t paid = std::min({due, account.balance, kMaxCredits - venue.balance});
		account.balance -= paid;
		venue.balance += paid;
		// The hour's fee is a holding charge of the lot a standing job has aboard (D-097).
		if (auto *pJob = registry.try_get<World::Job>(ship); pJob && pJob->totes > 0)
			pJob->charges += std::min(fee, kMaxCredits - pJob->charges);
		if (due > paid)
			registry.emplace_or_replace<World::Arrears>(
			    ship, due - paid, arrears.carry, pArrears ? arrears.since : world.Clock().Step());
		else
			registry.remove<World::Arrears>(ship);
	}
}


// The first step the berth fee has to run in (Jettison::Sim::Quiet): the hour's last, while a ship
// is docked; otherwise never, as no other step charges anything.
std::uint64_t BerthNext(const SimWorld &world, const Rules &rules)
{
	for (const auto [ship, record] : world.Registry().view<const World::Ship>().each())
		if (!record.dockedAt.empty())
		{
			const std::uint64_t now = world.Clock().Step();
			return now + (rules.hourSteps - 1 - now % rules.hourSteps);
		}
	return std::numeric_limits<std::uint64_t>::max();
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
		RegisterType<Found>("Manifest.Company.Found")
		    .Field<&Found::regionId>("regionId", kSaved)
		    .Field<&Found::companyId>("companyId", kSaved)
		    .Field<&Found::displayName>("displayName", kSaved);
		RegisterType<Move>("Manifest.Company.Move")
		    .Field<&Move::shipId>("shipId", kSaved)
		    .Field<&Move::venueId>("venueId", kSaved)
		    .Field<&Move::itemId>("itemId", kSaved)
		    .Field<&Move::totes>("totes", kSaved);
		RegisterType<Service>("Manifest.Company.Service").Field<&Service::shipId>("shipId", kSaved);
		RegisterType<Upgrade>("Manifest.Company.Upgrade")
		    .Field<&Upgrade::shipId>("shipId", kSaved)
		    .Field<&Upgrade::offerId>("offerId", kSaved)
		    .Field<&Upgrade::tier>("tier", kSaved);
		RegisterType<SetFees>("Manifest.Company.SetFees")
		    .Field<&SetFees::venueId>("venueId", kSaved)
		    .Field<&SetFees::fees>("fees", kSaved);
		RegisterType<SetRefuel>("Manifest.Company.SetRefuel")
		    .Field<&SetRefuel::shipId>("shipId", kSaved)
		    .Field<&SetRefuel::enabled>("enabled", kSaved);
		return true;
	}();
	(void)registered;
}


void RegisterCommands(
    SimWorld &world, Command::Requests &requests, World::Identities &identities, Travel::Fleet &fleet,
    const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	auto rules = std::make_shared<Rules>(identities, fleet, content, selected->second);
	rules->windowSteps = Trade::SalesWindowSteps(world, content);
	const double hourSteps = std::round(kSecondsPerHour / world.FixedStep());
	if (!(hourSteps >= 1.0) || std::abs(hourSteps * world.FixedStep() - kSecondsPerHour) > 1e-6)
		throw std::invalid_argument{"a simulation hour is not a whole number of the world's steps"};
	rules->hourSteps = static_cast<std::uint64_t>(hourSteps);
	for (const Content::ItemSelection &selection : selected->second.items)
	{
		const auto item =
		    std::ranges::find(content.gameMode.items, selection.itemId, &Core::ItemNode::itemId);
		JETTISON_VERIFY(item != content.gameMode.items.end());
		rules->items.emplace(selection.itemId, &*item);
	}
	for (const Core::ShipClassSpec &hull : content.hulls)
		rules->hulls.emplace(hull.hullId, &hull);
	for (const Core::SkillNode &skill : content.gameMode.skills)
		rules->skills.emplace(skill.skillId, &skill);
	for (const Content::OfferDefinition &offer : content.offers)
		if (rules->skills.contains(offer.skillId))
			rules->offers.emplace(offer.offerId, &offer);
	// Content refuses these too; a set built some other way is refused here.
	for (const Core::ShipClassSpec &hull : content.hulls)
		if (!Hold(*rules, {{}, hull.hullId, {}}, nullptr))
			throw std::invalid_argument{std::format(
			    "hull '{}' has a CargoVolume of {}, not a whole number of totes up to {}", hull.hullId,
			    hull.baseStats.at(Core::EffectStat::CargoVolume), Core::kMaxHoldTotes)};
	std::vector<std::string> padClasses;
	padClasses.reserve(content.tuning.charges.padClasses.size());
	for (const Content::ClassChargesTuning &charged : content.tuning.charges.padClasses)
		padClasses.push_back(charged.padClass);
	std::ranges::sort(padClasses);
	identities.SetCharges(std::move(padClasses), content.tuning.charges.maxArrearsRateBps);
	identities.SetHolds([rules](const World::Ship &ship, const World::Systems *pSystems)
	                    { return Hold(*rules, ship, pSystems); });
	identities.SetFuelTanks([rules](const World::Ship &ship, const World::Systems *pSystems)
	                        { return FuelTanks(*rules, ship, pSystems); });

	std::map<std::string, World::Installed, std::less<>> installed;
	for (const Core::ShipClassSpec &hull : content.hulls)
	{
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
	rules->reactionMassPerTote = perTote(content.tuning.fuel.reactionMassItemId);
	rules->pelletsPerTote = perTote(content.tuning.fuel.pelletItemId);
	rules->buy = Trade::MakeBuyer(world, identities, content);
	fleet.SetRefueller([rules](SimWorld &target, entt::entity ship, std::int64_t reserve)
	                   { Refuelled(target, *rules, ship, reserve); });

	requests.Register<Provision>(
	    std::string{kProvision}, kVersion, Command::ActorKind::Administrator,
	    [rules](SimWorld &target, const Actor & /*actor*/, const Provision &provision)
	    { return Provisioned(target, *rules, provision); });
	requests.Register<Found>(
	    std::string{kFound}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Found &found)
	    { return Founded(target, *rules, actor, found); });
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
	requests.Register<Upgrade>(
	    std::string{kUpgrade}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const Upgrade &upgrade)
	    { return Upgraded(target, *rules, actor, upgrade); });
	requests.Register<SetRefuel>(
	    std::string{kSetRefuel}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const SetRefuel &set)
	    { return RefuelSet(target, *rules, actor, set); });
	requests.Register<SetFees>(
	    std::string{kSetFees}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &target, const Actor &actor, const SetFees &set)
	    { return FeesSet(target, *rules, actor, set); });

	world.RegisterSystem(
	    {.name = std::string{kLifeSupportSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = Jettison::Ecs::Access::Of<World::LifeSupport, World::Ship, World::Job>()
	                   .Writing<World::LifeSupport, World::Job>(),
	     .after = {std::string{Travel::kSystem}},
	     .run = [rules](entt::registry &registry, double step) { Breathe(registry, *rules, step); }},
	    {.next = [rules](const SimWorld &target) { return BreatheNext(target, *rules); },
	     .skip = [rules](SimWorld &target, std::uint64_t passed, double)
	     { BreatheSkip(target.Registry(), *rules, target.FixedStep(), passed); }});
	world.RegisterSystem(
	    {.name = std::string{kPricingSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access =
	         Jettison::Ecs::Access::Of<
	             World::Broker, World::Organisation, Jettison::Dock::Platform, Jettison::Dock::Berth,
	             Jettison::Physics::OnRails, Jettison::Sim::InFrame, World::Fees, World::Pricing>()
	             .Writing<World::Fees, World::Pricing>(),
	     .after = {std::string{kLifeSupportSystem}},
	     .run = [rules, &world](entt::registry &, double) { Price(world, *rules); }},
	    {.next = [rules](const SimWorld &target) { return PriceNext(target, *rules); }});
	world.RegisterSystem(
	    {.name = std::string{kBerthSystem},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = Jettison::Ecs::Access::Of<
	                   World::Ship, World::Fees, World::Arrears, World::Account, World::LifeSupport>()
	                   .Writing<World::Account, World::Arrears, World::Job>(),
	     .after = {std::string{kPricingSystem}},
	     .run = [rules, &world](entt::registry &, double) { Berth(world, *rules); }},
	    {.next = [rules](const SimWorld &target) { return BerthNext(target, *rules); }});
}
} // namespace Manifest::Company
