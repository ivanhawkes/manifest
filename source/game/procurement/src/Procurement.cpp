#include <manifest/procurement/Procurement.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <jettison/core/Assert.h>
#include <jettison/profile/Profile.h>
#include <jettison/reflect/Reflect.h>

#include <manifest/config/SimulationConfig.h>
#include <manifest/trade/Economy.h>
#include <manifest/trade/Trade.h>

namespace Manifest::Procurement
{
namespace
{
using Command::Actor;
using Command::Result;
using Jettison::Sim::SimWorld;
using World::Kind;
using World::ProjectPhase;
using World::TenderState;

constexpr double kSecondsPerDay = 86'400.0;
constexpr std::int64_t kWholeBps = 10'000;

// A project as its definition gives it, by project ID: its inputs in its order.
struct Plan
{
	World::ProjectNeeds needs;
	std::vector<Content::ItemTotes> inputs;
	std::vector<std::string> skills;
	std::string recipeId;
	double priorityPremium{0.0};
};

// A region as the scenario gives it: the venues serving it, in the scenario's order, and its projects.
struct Area
{
	std::string regionId;
	std::vector<std::string> venues;
	std::vector<std::string> projects;
};

struct Rules
{
	World::Identities *pIdentities{nullptr};
	const Command::Requests *pRequests{nullptr};
	// The bond's rate, in basis points of a winning total.
	std::int64_t bondBps{0};
	Trade::Pricer pricer;
	Content::ProcurementTuning tuning;
	std::map<std::string, Plan, std::less<>> plans;
	// In the scenario's order.
	std::vector<Area> areas;
	std::map<std::string, double, std::less<>> unitsPerTote;
	std::uint64_t stepsPerDay{0};
	std::uint64_t turnSteps{0};

	const Area &AreaOf(std::string_view regionId) const
	{
		const auto area = std::ranges::find(areas, regionId, &Area::regionId);
		JETTISON_VERIFY(area != areas.end());
		return *area;
	}
};


entt::entity Required(const Rules &rules, Kind kind, const std::string &id)
{
	const entt::entity entity = rules.pIdentities->Find(kind, id);
	JETTISON_VERIFY(entity != entt::null);
	return entity;
}


// ─── Projects ───

bool Operational(const World::Facilities &facilities)
{
	// shortcut: every project is metallurgy's, worked at a workshop (Content.cpp's CheckProjects);
	// a project names its facility when a second needs another.
	return std::ranges::any_of(
	    facilities.facilities, [](const World::Facility &facility)
	    { return facility.kind == Content::FacilityKind::Workshop && facility.operational; });
}


std::uint64_t Held(const World::Stock &stock, std::string_view itemId)
{
	std::uint64_t held = 0;
	for (const World::Holding &holding : stock.holdings)
		if (holding.itemId == itemId)
			held += holding.available;
	return held;
}


bool Startable(const Plan &plan, const World::Stock &stock, const World::Knowledge &knowledge)
{
	for (const std::string &skill : plan.skills)
		if (!std::ranges::binary_search(knowledge.skills, skill))
			return false;
	return std::ranges::all_of(
	    plan.needs.inputs, [&](const auto &input) { return Held(stock, input.first) >= input.second; });
}


// What a project's region is, for its rules.
struct Region
{
	entt::entity entity{entt::null};
	bool operational{false};
};


Region RegionOf(const entt::registry &registry, const Rules &rules, const World::Project &project)
{
	const entt::entity region = Required(rules, Kind::Region, project.regionId);
	return {region, Operational(registry.get<const World::Facilities>(region))};
}


void Work(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Procurement.Work");
	entt::registry &registry = world.Registry();
	for (const auto [entity, project, stock] : registry.view<World::Project, World::Stock>().each())
	{
		if (project.phase == ProjectPhase::Done)
			continue;
		const Plan &plan = rules.plans.at(project.projectId);
		const Region region = RegionOf(registry, rules, project);
		if (project.phase == ProjectPhase::Tendering)
		{
			if (!region.operational ||
			    !Startable(plan, stock, registry.get<const World::Knowledge>(region.entity)))
				continue;
			// A project holds no more than its inputs (Identities::Load, and delivery owes no more).
			stock.holdings.clear();
		}
		if (!region.operational)
		{
			project.phase = ProjectPhase::Paused;
			continue;
		}
		project.phase = ProjectPhase::Working;
		if (++project.worked < plan.needs.workSteps)
			continue;
		project.phase = ProjectPhase::Done;
		std::vector<std::string> &recipes = registry.get<World::Knowledge>(region.entity).recipes;
		if (const auto at = std::ranges::lower_bound(recipes, plan.recipeId);
		    at == recipes.end() || *at != plan.recipeId)
			recipes.insert(at, plan.recipeId);
	}
}


// The first step the projects have to run in (Jettison::Sim::Quiet): now for one that starts, pauses
// or resumes, the last of a Working one's work, and none while the rest wait.
std::uint64_t WorkNext(const SimWorld &world, const Rules &rules)
{
	const entt::registry &registry = world.Registry();
	const std::uint64_t now = world.Clock().Step();
	std::uint64_t next = std::numeric_limits<std::uint64_t>::max();
	for (const auto [entity, project, stock] :
	     registry.view<const World::Project, const World::Stock>().each())
	{
		const Region region = RegionOf(registry, rules, project);
		switch (project.phase)
		{
			case ProjectPhase::Tendering:
				if (region.operational && Startable(
				                              rules.plans.at(project.projectId), stock,
				                              registry.get<const World::Knowledge>(region.entity)))
					return now;
				break;
			case ProjectPhase::Paused:
				if (region.operational)
					return now;
				break;
			case ProjectPhase::Working:
				if (!region.operational)
					return now;
				// Run at now, the step's work is worked + 1; the last is workSteps.
				next = std::min(
				    next,
				    now + (rules.plans.at(project.projectId).needs.workSteps - project.worked - 1));
				break;
			case ProjectPhase::Done: break;
		}
	}
	return next;
}


// The work of the steps passed over, one a step, each Working project's, none of them its last.
void WorkSkip(SimWorld &world, std::uint64_t passed)
{
	for (const auto [entity, project] : world.Registry().view<World::Project>().each())
		if (project.phase == ProjectPhase::Working)
			project.worked += passed;
}


// ─── Tenders ───

// Open or awarded: calling for its goods.
bool Live(const World::Tender &tender)
{
	return tender.state == TenderState::Open || tender.state == TenderState::Awarded;
}


template <typename Match> bool AnyLive(const entt::registry &registry, Match match)
{
	for (const auto [entity, tender] : registry.view<const World::Tender>().each())
		if (Live(tender) && match(tender))
			return true;
	return false;
}


// A tender's terms as its issuer gives them.
struct Terms
{
	std::string regionId;
	std::string projectId;
	std::string needId;
	std::string itemId;
	std::string venueId;
	std::uint64_t totes{0};
	std::int64_t budget{0};
	std::uint32_t bidDays{0};
	std::uint32_t deliveryDays{0};
};


// The totes of the item the region's project still needs: its input, less what it holds and what its
// live tenders call for; none for a project past tendering.
std::uint64_t StillNeeded(
    const entt::registry &registry, const Rules &rules, const std::string &regionId,
    const std::string &projectId, const std::string &itemId)
{
	const entt::entity entity = Required(rules, Kind::Project, World::ProjectId(regionId, projectId));
	if (registry.get<const World::Project>(entity).phase != ProjectPhase::Tendering)
		return 0;
	const auto input = rules.plans.at(projectId).needs.inputs.find(itemId);
	if (input == rules.plans.at(projectId).needs.inputs.end())
		return 0;
	std::uint64_t called = Held(registry.get<const World::Stock>(entity), itemId);
	// What a contract has delivered is held already.
	for (const auto [other, tender] : registry.view<const World::Tender>().each())
		if (Live(tender) && tender.regionId == regionId && tender.projectId == projectId &&
		    tender.itemId == itemId)
			called += tender.totes - tender.delivered;
	return input->second > called ? input->second - called : 0;
}


// What a tender's issuer may not publish, whoever it is: Succeeded for one it may.
Result Check(const entt::registry &registry, const Rules &rules, const Terms &terms)
{
	if (terms.totes == 0 ||
	    (!terms.projectId.empty() &&
	     terms.totes > StillNeeded(registry, rules, terms.regionId, terms.projectId, terms.itemId)))
		return Result::InvalidQuantity;
	if (terms.budget < 1)
		return Result::InvalidPrice;
	std::uint32_t open = 0;
	for (const auto [entity, tender] : registry.view<const World::Tender>().each())
		open += Live(tender) && tender.regionId == terms.regionId ? 1 : 0;
	if (open >= rules.tuning.maxOpenTenders)
		return Result::LimitReached;
	if (registry.get<const World::Account>(Required(rules, Kind::PublicAccount, terms.regionId))
	        .balance < terms.budget)
		return Result::InsufficientFunds;
	return Result::Succeeded;
}


// Publishes terms Check passed: the tender, its account and its budget moved into it.
void Published(SimWorld &world, const Rules &rules, const Terms &terms)
{
	entt::registry &registry = world.Registry();
	World::Identities &identities = *rules.pIdentities;
	JETTISON_VERIFY(identities.Issue(Kind::Tender).has_value());
	const std::string id = TenderId(
	    terms.regionId, terms.projectId.empty() ? terms.needId : terms.projectId, terms.itemId,
	    identities.LastIssued(Kind::Tender));
	const std::uint64_t now = world.Clock().Step();
	const entt::entity tender = identities.Create(registry, Kind::Tender, id);
	JETTISON_VERIFY(tender != entt::null);
	registry.emplace<World::Tender>(
	    tender, terms.regionId, terms.projectId, terms.needId, terms.itemId, terms.venueId, terms.totes,
	    terms.budget, now, now + terms.bidDays * rules.stepsPerDay, terms.deliveryDays,
	    TenderState::Open, std::uint64_t{0});
	const entt::entity account = identities.Create(registry, Kind::TenderAccount, id);
	JETTISON_VERIFY(account != entt::null);
	registry.emplace<World::Account>(account, terms.budget);
	registry.get<World::Account>(Required(rules, Kind::PublicAccount, terms.regionId)).balance -=
	    terms.budget;
}


// Closes an open tender, what its account holds returned to its region's public account.
void Close(SimWorld &world, const Rules &rules, entt::entity entity, TenderState state)
{
	entt::registry &registry = world.Registry();
	World::Tender &tender = registry.get<World::Tender>(entity);
	const std::string &id = registry.get<const World::Identity>(entity).id;
	const entt::entity account = Required(rules, Kind::TenderAccount, id);
	registry.get<World::Account>(Required(rules, Kind::PublicAccount, tender.regionId)).balance +=
	    registry.get<const World::Account>(account).balance;
	rules.pIdentities->Destroy(registry, account);
	tender.state = state;
	tender.closed = world.Clock().Step();
}


// The region's closed tenders beyond procurement.keptClosed, oldest closed first, dropped.
void Prune(SimWorld &world, const Rules &rules, const std::string &regionId)
{
	entt::registry &registry = world.Registry();
	std::vector<std::tuple<std::uint64_t, std::string, entt::entity>> closed;
	for (const auto [entity, identity, tender] :
	     registry.view<const World::Identity, const World::Tender>().each())
		if (!Live(tender) && tender.regionId == regionId)
			closed.emplace_back(tender.closed, identity.id, entity);
	if (closed.size() <= rules.tuning.keptClosed)
		return;
	std::ranges::sort(closed);
	closed.resize(closed.size() - rules.tuning.keptClosed);
	for (const auto &[step, id, entity] : closed)
		rules.pIdentities->Destroy(registry, entity);
}


// The bond on a winning total: the rate's share of it, rounded up.
std::int64_t BondOf(std::int64_t total, std::int64_t bps)
{
	return total / kWholeBps * bps + (total % kWholeBps * bps + kWholeBps - 1) / kWholeBps;
}


// An open tender's bidding closed: to its lowest bid, then its earliest accepted, then its company's
// ID, whose company can fund the bond, moved into the tender's BondAccount, the budget beyond the
// total returned; unawarded with none. Its bids are dropped either way.
void AwardAt(SimWorld &world, const Rules &rules, entt::entity entity)
{
	entt::registry &registry = world.Registry();
	const std::string id = registry.get<const World::Identity>(entity).id;
	std::vector<std::tuple<std::int64_t, std::uint64_t, std::uint64_t, std::string, entt::entity>> bids;
	for (const auto [bid, placed] : registry.view<const World::Bid>().each())
		if (placed.tenderId == id)
			bids.emplace_back(placed.total, placed.step, placed.order, placed.companyId, bid);
	std::ranges::sort(bids);
	World::Award award = bids.empty() ? World::Award::NoBids : World::Award::Unbonded;
	std::optional<std::size_t> won;
	for (std::size_t at = 0; at < bids.size() && !won; ++at)
	{
		const auto &[total, step, order, companyId, bid] = bids[at];
		if (registry.get<const World::Account>(Required(rules, Kind::CompanyAccount, companyId))
		        .balance >= BondOf(total, rules.bondBps))
		{
			won = at;
			award = at == 0 ? World::Award::Lowest : World::Award::PastUnbonded;
		}
	}
	std::int64_t total = 0;
	std::string companyId;
	if (won)
	{
		total = std::get<0>(bids[*won]);
		companyId = std::get<3>(bids[*won]);
	}
	for (const auto &each : bids)
		rules.pIdentities->Destroy(registry, std::get<4>(each));
	World::Tender &tender = registry.get<World::Tender>(entity);
	tender.award = award;
	if (!won)
	{
		Close(world, rules, entity, TenderState::Unawarded);
		return;
	}
	const std::int64_t bond = BondOf(total, rules.bondBps);
	registry.get<World::Account>(Required(rules, Kind::CompanyAccount, companyId)).balance -= bond;
	const entt::entity held = rules.pIdentities->Create(registry, Kind::BondAccount, id);
	JETTISON_VERIFY(held != entt::null);
	registry.emplace<World::Account>(held, bond);
	World::Account &reserved = registry.get<World::Account>(Required(rules, Kind::TenderAccount, id));
	registry.get<World::Account>(Required(rules, Kind::PublicAccount, tender.regionId)).balance +=
	    reserved.balance - total;
	reserved.balance = total;
	tender.state = TenderState::Awarded;
	tender.companyId = companyId;
	tender.price = total;
	tender.bond = bond;
	tender.deliveryClose = world.Clock().Step() + tender.deliveryDays * rules.stepsPerDay;
}


// Ends an awarded tender's contract: what its account holds returned to the public account, the
// bond's share of the totes undelivered, rounded down, forfeit to it and the rest returned to the
// supplier, all of it once every tote is delivered. Fulfilled then, in default otherwise.
void Settle(SimWorld &world, const Rules &rules, entt::entity entity)
{
	entt::registry &registry = world.Registry();
	const World::Tender &tender = registry.get<const World::Tender>(entity);
	const entt::entity bond =
	    Required(rules, Kind::BondAccount, registry.get<const World::Identity>(entity).id);
	const std::int64_t held = registry.get<const World::Account>(bond).balance;
	const auto whole = static_cast<std::int64_t>(tender.totes);
	const auto missing = static_cast<std::int64_t>(tender.totes - tender.delivered);
	// Exactly, as World::Owed: the bond's remainder times the missing totes is below Q squared.
	const std::int64_t forfeit = held / whole * missing + held % whole * missing / whole;
	registry.get<World::Account>(Required(rules, Kind::PublicAccount, tender.regionId)).balance +=
	    forfeit;
	registry.get<World::Account>(Required(rules, Kind::CompanyAccount, tender.companyId)).balance +=
	    held - forfeit;
	rules.pIdentities->Destroy(registry, bond);
	Close(world, rules, entity, missing == 0 ? TenderState::Fulfilled : TenderState::Defaulted);
}


// The company the actor is principal of, for its bids; none for a company the world does not have,
// Result's UnknownTarget, or another's, NotAuthorised.
Result Bidder(
    const entt::registry &registry, const Rules &rules, const Actor &actor,
    const std::string &companyId)
{
	const entt::entity company = rules.pIdentities->Find(Kind::Company, companyId);
	if (company == entt::null)
		return Result::UnknownTarget;
	return registry.get<const World::Company>(company).principalId == actor.id ? Result::Succeeded
	                                                                           : Result::NotAuthorised;
}


Result Bids(SimWorld &world, const Rules &rules, const Actor &actor, const Bid &bid)
{
	entt::registry &registry = world.Registry();
	const entt::entity entity = rules.pIdentities->Find(Kind::Tender, bid.tenderId);
	if (entity == entt::null)
		return Result::UnknownTarget;
	if (const Result bidder = Bidder(registry, rules, actor, bid.companyId);
	    bidder != Result::Succeeded)
		return bidder;
	const World::Tender &tender = registry.get<const World::Tender>(entity);
	const std::uint64_t now = world.Clock().Step();
	if (tender.state != TenderState::Open || now >= tender.bidClose)
		return Result::Closed;
	if (bid.total < 1 || bid.total > tender.budget)
		return Result::InvalidPrice;
	const std::string id = World::BidId(bid.tenderId, bid.companyId);
	const std::uint64_t order = rules.pRequests->RunningOrder();
	if (const entt::entity placed = rules.pIdentities->Find(Kind::Bid, id); placed != entt::null)
	{
		World::Bid &held = registry.get<World::Bid>(placed);
		if (bid.revision != held.revision)
			return Result::StaleRevision;
		held = {bid.tenderId, bid.companyId, bid.total, held.revision + 1, now, order};
		return Result::Succeeded;
	}
	if (bid.revision != 0)
		return Result::StaleRevision;
	std::uint32_t placed = 0;
	for (const auto [other, each] : registry.view<const World::Bid>().each())
		placed += each.tenderId == bid.tenderId ? 1 : 0;
	if (placed >= rules.tuning.maxBids)
		return Result::LimitReached;
	const entt::entity created = rules.pIdentities->Create(registry, Kind::Bid, id);
	JETTISON_VERIFY(created != entt::null);
	registry.emplace<World::Bid>(
	    created, bid.tenderId, bid.companyId, bid.total, std::uint64_t{1}, now, order);
	return Result::Succeeded;
}


Result Withdraws(SimWorld &world, const Rules &rules, const Actor &actor, const Withdraw &withdraw)
{
	entt::registry &registry = world.Registry();
	const entt::entity entity = rules.pIdentities->Find(Kind::Tender, withdraw.tenderId);
	if (entity == entt::null)
		return Result::UnknownTarget;
	if (const Result bidder = Bidder(registry, rules, actor, withdraw.companyId);
	    bidder != Result::Succeeded)
		return bidder;
	const World::Tender &tender = registry.get<const World::Tender>(entity);
	if (tender.state != TenderState::Open || world.Clock().Step() >= tender.bidClose)
		return Result::Closed;
	const entt::entity placed =
	    rules.pIdentities->Find(Kind::Bid, World::BidId(withdraw.tenderId, withdraw.companyId));
	if (placed == entt::null)
		return Result::UnknownTarget;
	rules.pIdentities->Destroy(registry, placed);
	return Result::Succeeded;
}


// The stock a tender's goods are delivered into: its project's, or its region's households' for a
// relief tender.
entt::entity Recipient(const Rules &rules, const World::Tender &tender)
{
	return tender.projectId.empty()
	           ? Required(rules, Kind::Household, tender.regionId)
	           : Required(rules, Kind::Project, World::ProjectId(tender.regionId, tender.projectId));
}


Result Delivers(SimWorld &world, const Rules &rules, const Actor &actor, const Deliver &deliver)
{
	entt::registry &registry = world.Registry();
	const entt::entity entity = rules.pIdentities->Find(Kind::Tender, deliver.tenderId);
	const entt::entity shipEntity = rules.pIdentities->Find(Kind::Ship, deliver.shipId);
	if (entity == entt::null || shipEntity == entt::null)
		return Result::UnknownTarget;
	const World::Ship &ship = registry.get<const World::Ship>(shipEntity);
	if (const Result bidder = Bidder(registry, rules, actor, ship.companyId);
	    bidder != Result::Succeeded)
		return bidder;
	World::Tender &tender = registry.get<World::Tender>(entity);
	if (tender.state != TenderState::Awarded || world.Clock().Step() >= tender.deliveryClose)
		return Result::Closed;
	if (ship.companyId != tender.companyId)
		return Result::NotAuthorised;
	if (ship.dockedAt.empty())
		return Result::NotDocked;
	if (ship.dockedAt != tender.venueId)
		return Result::WrongVenue;
	World::Cargo &cargo = registry.get<World::Cargo>(shipEntity);
	const auto lot = std::ranges::find(cargo.lots, tender.itemId, &World::Lot::itemId);
	if (lot == cargo.lots.end())
		return Result::InsufficientStock;
	World::Stock &stock = registry.get<World::Stock>(Recipient(rules, tender));
	const auto key = std::tie(tender.venueId, tender.itemId);
	const auto holding = std::ranges::lower_bound(
	    stock.holdings, key, std::less<>{},
	    [](const World::Holding &held) { return std::tie(held.venueId, held.itemId); });
	const bool held =
	    holding != stock.holdings.end() && std::tie(holding->venueId, holding->itemId) == key;
	const std::uint64_t warehoused = held ? holding->available + holding->reserved : 0;
	const std::uint64_t room =
	    Core::kMaxWarehouseTotes > warehoused ? Core::kMaxWarehouseTotes - warehoused : 0;
	const std::uint64_t accepted = std::min({lot->totes, tender.totes - tender.delivered, room});
	if (accepted == 0)
		return Result::CapacityExceeded;

	lot->totes -= accepted;
	if (lot->totes == 0)
		cargo.lots.erase(lot);
	if (held)
		holding->available += accepted;
	else
		stock.holdings.insert(holding, {tender.venueId, tender.itemId, accepted, 0});
	const std::int64_t before = World::Owed(tender);
	tender.delivered += accepted;
	const std::int64_t paid = World::Owed(tender) - before;
	const std::string &id = registry.get<const World::Identity>(entity).id;
	registry.get<World::Account>(Required(rules, Kind::TenderAccount, id)).balance -= paid;
	registry.get<World::Account>(Required(rules, Kind::CompanyAccount, tender.companyId)).balance +=
	    paid;
	if (tender.delivered == tender.totes)
	{
		const std::string regionId = tender.regionId;
		Settle(world, rules, entity);
		Prune(world, rules, regionId);
	}
	return Result::Succeeded;
}


Result Abandons(SimWorld &world, const Rules &rules, const Actor &actor, const Abandon &abandon)
{
	entt::registry &registry = world.Registry();
	const entt::entity entity = rules.pIdentities->Find(Kind::Tender, abandon.tenderId);
	if (entity == entt::null)
		return Result::UnknownTarget;
	const World::Tender &tender = registry.get<const World::Tender>(entity);
	if (tender.companyId.empty())
		return Result::Closed;
	if (const Result bidder = Bidder(registry, rules, actor, tender.companyId);
	    bidder != Result::Succeeded)
		return bidder;
	if (tender.state != TenderState::Awarded || world.Clock().Step() >= tender.deliveryClose)
		return Result::Closed;
	const std::string regionId = tender.regionId;
	Settle(world, rules, entity);
	Prune(world, rules, regionId);
	return Result::Succeeded;
}


// Whether the actor directs the region's governor, so may publish and cancel its tenders.
bool Directs(
    const entt::registry &registry, const Rules &rules, const Actor &actor, const std::string &regionId)
{
	const std::string &governorId =
	    registry.get<const World::Region>(Required(rules, Kind::Region, regionId)).governorId;
	if (governorId.empty())
		return false;
	const std::string &principalId =
	    registry.get<const World::Organisation>(Required(rules, Kind::Organisation, governorId))
	        .principalId;
	return !principalId.empty() && principalId == actor.id;
}


Result Publishes(SimWorld &world, const Rules &rules, const Actor &actor, const Publish &publish)
{
	const entt::registry &registry = world.Registry();
	if (rules.pIdentities->Find(Kind::Region, publish.regionId) == entt::null ||
	    rules.pIdentities->Find(Kind::Project, World::ProjectId(publish.regionId, publish.projectId)) ==
	        entt::null ||
	    !rules.plans.at(publish.projectId).needs.inputs.contains(publish.itemId))
		return Result::UnknownTarget;
	if (!Directs(registry, rules, actor, publish.regionId))
		return Result::NotAuthorised;
	if (const std::vector<std::string> &venues = rules.AreaOf(publish.regionId).venues;
	    std::ranges::find(venues, publish.venueId) == venues.end())
		return Result::WrongVenue;
	const Content::ProcurementTuning &tuning = rules.tuning;
	if (publish.bidDays < tuning.minBidDays || publish.bidDays > tuning.maxBidDays ||
	    publish.deliveryDays < tuning.minDeliveryDays || publish.deliveryDays > tuning.maxDeliveryDays)
		return Result::InvalidQuantity;
	const Terms terms{publish.regionId, publish.projectId, "",
	                  publish.itemId,   publish.venueId,   publish.totes,
	                  publish.budget,   publish.bidDays,   publish.deliveryDays};
	if (const Result checked = Check(registry, rules, terms); checked != Result::Succeeded)
		return checked;
	Published(world, rules, terms);
	return Result::Succeeded;
}


Result Cancels(SimWorld &world, const Rules &rules, const Actor &actor, const Cancel &cancel)
{
	const entt::entity entity = rules.pIdentities->Find(Kind::Tender, cancel.tenderId);
	if (entity == entt::null)
		return Result::UnknownTarget;
	const World::Tender &tender = world.Registry().get<const World::Tender>(entity);
	if (!Directs(world.Registry(), rules, actor, tender.regionId))
		return Result::NotAuthorised;
	if (tender.state != TenderState::Open || world.Clock().Step() >= tender.bidClose)
		return Result::Closed;
	const std::string regionId = tender.regionId;
	for (const auto [bid, placed] : world.Registry().view<const World::Bid>().each())
		if (placed.tenderId == cancel.tenderId)
			rules.pIdentities->Destroy(world.Registry(), bid);
	Close(world, rules, entity, TenderState::Cancelled);
	Prune(world, rules, regionId);
	return Result::Succeeded;
}


bool AiRun(const entt::registry &registry, const Rules &rules, const std::string &regionId)
{
	const std::string &governorId =
	    registry.get<const World::Region>(Required(rules, Kind::Region, regionId)).governorId;
	return !governorId.empty() &&
	       registry.get<const World::Organisation>(Required(rules, Kind::Organisation, governorId))
	           .principalId.empty();
}


// The AI issuer's tenders, as kTenders gives them; reliefs only on a turn's last step.
void Issue(SimWorld &world, const Rules &rules, bool reliefs)
{
	JETTISON_PROFILE_ZONE("Manifest.Procurement.Issue");
	const entt::registry &registry = world.Registry();
	const Content::ProcurementTuning &tuning = rules.tuning;
	for (const Area &area : rules.areas)
	{
		if (area.venues.empty() || !AiRun(registry, rules, area.regionId))
			continue;
		for (const std::string &projectId : area.projects)
		{
			const Plan &plan = rules.plans.at(projectId);
			for (const Content::ItemTotes &input : plan.inputs)
			{
				const std::uint64_t totes =
				    StillNeeded(registry, rules, area.regionId, projectId, input.itemId);
				const bool open = AnyLive(
				    registry,
				    [&](const World::Tender &tender)
				    {
					    return tender.regionId == area.regionId && tender.projectId == projectId &&
					           tender.itemId == input.itemId;
				    });
				if (totes == 0 || open)
					continue;
				const std::optional<std::int64_t> price =
				    rules.pricer.tenderPrice(world, area.venues.front(), input.itemId);
				const std::optional<std::int64_t> budget =
				    price
				        ? Trade::Budget(
				              *price, static_cast<double>(totes) * rules.unitsPerTote.at(input.itemId),
				              plan.priorityPremium)
				        : std::nullopt;
				const Terms terms{area.regionId,      projectId,           "",
				                  input.itemId,       area.venues.front(), totes,
				                  budget.value_or(0), tuning.aiBidDays,    tuning.aiDeliveryDays};
				if (budget && Check(registry, rules, terms) == Result::Succeeded)
					Published(world, rules, terms);
			}
		}
	}
	if (!reliefs)
		return;
	for (const Trade::Relief &relief : rules.pricer.reliefs(world))
	{
		if (!AiRun(registry, rules, relief.regionId))
			continue;
		const bool open = AnyLive(
		    registry, [&](const World::Tender &tender)
		    { return tender.regionId == relief.regionId && tender.needId == relief.needId; });
		const Terms terms{relief.regionId,      "",           relief.needId, relief.itemId,
		                  relief.venueId,       relief.totes, relief.budget, tuning.aiBidDays,
		                  tuning.aiDeliveryDays};
		if (!open && Check(registry, rules, terms) == Result::Succeeded)
			Published(world, rules, terms);
	}
}


// Whether a tender closed, or its contract ended, at the step.
bool AnyClosedAt(const entt::registry &registry, std::uint64_t step)
{
	for (const auto [entity, tender] : registry.view<const World::Tender>().each())
		if (!Live(tender) && tender.closed == step)
			return true;
	return false;
}


bool TurnEnds(std::uint64_t step, const Rules &rules)
{
	return (step + 1) % rules.turnSteps == 0;
}


void Tenders(SimWorld &world, const Rules &rules)
{
	JETTISON_PROFILE_ZONE("Manifest.Procurement.Tenders");
	entt::registry &registry = world.Registry();
	const std::uint64_t now = world.Clock().Step();
	std::vector<std::pair<std::string, entt::entity>> lapsing;
	std::vector<std::pair<std::string, entt::entity>> closing;
	for (const auto [entity, identity, tender] :
	     registry.view<const World::Identity, const World::Tender>().each())
	{
		if (tender.state == TenderState::Awarded && tender.deliveryClose <= now)
			lapsing.emplace_back(identity.id, entity);
		if (tender.state == TenderState::Open && tender.bidClose <= now)
			closing.emplace_back(identity.id, entity);
	}
	std::ranges::sort(lapsing);
	std::ranges::sort(closing);
	std::set<std::string, std::less<>> regions;
	// Defaults first, so a bond returned can fund an award closing in the same step.
	for (const auto &[id, entity] : lapsing)
	{
		regions.insert(registry.get<const World::Tender>(entity).regionId);
		Settle(world, rules, entity);
	}
	for (const auto &[id, entity] : closing)
	{
		regions.insert(registry.get<const World::Tender>(entity).regionId);
		AwardAt(world, rules, entity);
	}
	for (const std::string &regionId : regions)
		Prune(world, rules, regionId);
	// Ended now, by this system or by a command before it: a default's totes are tendered again.
	const bool ended = AnyClosedAt(registry, now);
	const bool turnEnds = TurnEnds(now, rules);
	if (now == 0 || !closing.empty() || ended || turnEnds)
		Issue(world, rules, turnEnds);
}


// The first step the tenders have to run in: the world's first, one a tender closed in, the next bid
// or delivery close or the turn's last step.
std::uint64_t TendersNext(const SimWorld &world, const Rules &rules)
{
	const std::uint64_t now = world.Clock().Step();
	if (now == 0 || AnyClosedAt(world.Registry(), now))
		return now;
	std::uint64_t next = now + (rules.turnSteps - 1 - now % rules.turnSteps);
	for (const auto [entity, tender] : world.Registry().view<const World::Tender>().each())
	{
		if (tender.state == TenderState::Open)
			next = std::min(next, std::max(now, tender.bidClose));
		if (tender.state == TenderState::Awarded)
			next = std::min(next, std::max(now, tender.deliveryClose));
	}
	return next;
}


std::shared_ptr<Rules> MakeRules(
    SimWorld &world, const Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content)
{
	const auto selected = content.scenarios.find(content.scenario);
	if (selected == content.scenarios.end())
		throw std::invalid_argument{"the content set has no scenario selected"};
	const Content::ScenarioDefinition &scenario = selected->second;
	auto rules = std::make_shared<Rules>();
	rules->pIdentities = &identities;
	rules->pRequests = &requests;
	rules->pricer = Trade::MakePricer(world, identities, content);
	rules->tuning = content.tuning.procurement;
	rules->bondBps = std::llround(content.tuning.procurement.performanceSecurityRate * kWholeBps);
	rules->stepsPerDay = Steps(1, world.FixedStep());
	rules->turnSteps = std::max<std::uint64_t>(
	    1, static_cast<std::uint64_t>(
	           std::llround(content.tuning.economy.intervalSeconds / world.FixedStep())));
	for (const Content::ProjectDefinition &project : content.tuning.projects)
	{
		Plan plan;
		plan.inputs = project.inputs;
		for (const Content::ItemTotes &input : project.inputs)
			plan.needs.inputs.emplace(input.itemId, input.totes);
		plan.needs.workSteps = Steps(project.workDays, world.FixedStep());
		for (const Content::SkillSelection &skill : project.requiredSkills)
			plan.skills.push_back(skill.skillId);
		plan.recipeId = project.unlocksRecipe;
		plan.priorityPremium = project.priorityPremium;
		rules->plans.emplace(project.projectId, std::move(plan));
	}
	for (const Content::RegionDefinition &region : scenario.regions)
	{
		Area &area = rules->areas.emplace_back();
		area.regionId = region.regionId;
		for (const Content::VenueDefinition &venue : scenario.venues)
			if (std::ranges::find(
			        venue.regions, region.regionId, &Content::RegionSelection::regionId) !=
			    venue.regions.end())
				area.venues.push_back(venue.venueId);
		for (const Content::ProjectSelection &project : region.projects)
			area.projects.push_back(project.projectId);
	}
	for (const Core::ItemNode &item : content.gameMode.items)
		rules->unitsPerTote.emplace(
		    item.itemId, item.unitsPerCubicMetre / static_cast<double>(Core::kTotesPerIbc));
	return rules;
}
} // namespace


std::uint64_t Steps(std::uint32_t days, double fixedStep)
{
	return std::max<std::uint64_t>(
	    1, static_cast<std::uint64_t>(std::llround(days * kSecondsPerDay / fixedStep)));
}


std::string TenderId(
    std::string_view regionId, std::string_view purpose, std::string_view itemId, std::uint64_t count)
{
	return std::format("{}:{}:{}:{}", regionId, purpose, itemId, count);
}


void RegisterTypes()
{
	static const bool registered = []
	{
		Command::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<Publish>("Manifest.Procurement.Publish")
		    .Field<&Publish::regionId>("regionId", kSaved)
		    .Field<&Publish::projectId>("projectId", kSaved)
		    .Field<&Publish::itemId>("itemId", kSaved)
		    .Field<&Publish::venueId>("venueId", kSaved)
		    .Field<&Publish::totes>("totes", kSaved)
		    .Field<&Publish::budget>("budget", kSaved)
		    .Field<&Publish::bidDays>("bidDays", kSaved)
		    .Field<&Publish::deliveryDays>("deliveryDays", kSaved);
		RegisterType<Cancel>("Manifest.Procurement.Cancel")
		    .Field<&Cancel::tenderId>("tenderId", kSaved);
		RegisterType<Bid>("Manifest.Procurement.Bid")
		    .Field<&Bid::tenderId>("tenderId", kSaved)
		    .Field<&Bid::companyId>("companyId", kSaved)
		    .Field<&Bid::total>("total", kSaved)
		    .Field<&Bid::revision>("revision", kSaved);
		RegisterType<Withdraw>("Manifest.Procurement.Withdraw")
		    .Field<&Withdraw::tenderId>("tenderId", kSaved)
		    .Field<&Withdraw::companyId>("companyId", kSaved);
		RegisterType<Deliver>("Manifest.Procurement.Deliver")
		    .Field<&Deliver::tenderId>("tenderId", kSaved)
		    .Field<&Deliver::shipId>("shipId", kSaved);
		RegisterType<Abandon>("Manifest.Procurement.Abandon")
		    .Field<&Abandon::tenderId>("tenderId", kSaved);
		return true;
	}();
	(void)registered;
}


void Register(
    SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content)
{
	std::shared_ptr<const Rules> rules = MakeRules(world, requests, identities, content);
	std::map<std::string, World::ProjectNeeds, std::less<>> needs;
	for (const auto &[projectId, plan] : rules->plans)
		needs.emplace(projectId, plan.needs);
	identities.SetProjects(std::move(needs));

	requests.Register<Publish>(
	    std::string{kPublish}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Publish &publish)
	    { return Publishes(on, *rules, actor, publish); });
	requests.Register<Cancel>(
	    std::string{kCancel}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Cancel &cancel)
	    { return Cancels(on, *rules, actor, cancel); });
	requests.Register<Bid>(
	    std::string{kBid}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Bid &bid)
	    { return Bids(on, *rules, actor, bid); });
	requests.Register<Deliver>(
	    std::string{kDeliver}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Deliver &deliver)
	    { return Delivers(on, *rules, actor, deliver); });
	requests.Register<Abandon>(
	    std::string{kAbandon}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Abandon &abandon)
	    { return Abandons(on, *rules, actor, abandon); });
	requests.Register<Withdraw>(
	    std::string{kWithdraw}, kVersion, Command::ActorKind::Principal,
	    [rules](SimWorld &on, const Actor &actor, const Withdraw &withdraw)
	    { return Withdraws(on, *rules, actor, withdraw); });

	using Jettison::Ecs::Access;
	world.RegisterSystem(
	    {.name = std::string{kTenders},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = Access::Of<
	                   World::Region, World::Organisation, World::Project, World::Stock, World::Pantry,
	                   World::Order, World::Broker, World::Company>()
	                   .Writing<World::Tender, World::Account, World::Identity, World::Bid>(),
	     .after = {std::string{Trade::kExpiry}},
	     .run = [rules, &world](entt::registry &, double) { Tenders(world, *rules); },
	     .before = {std::string{Trade::kEconomy}}},
	    {.next = [rules](const SimWorld &target) { return TendersNext(target, *rules); }});
	world.RegisterSystem(
	    {.name = std::string{kProjects},
	     .phase = Jettison::Ecs::TickPhase::Simulation,
	     .access = Access::Of<World::Facilities, World::Identity>()
	                   .Writing<World::Project, World::Stock, World::Knowledge>(),
	     .after = {std::string{kTenders}},
	     .run = [rules, &world](entt::registry &, double) { Work(world, *rules); },
	     .before = {std::string{Trade::kEconomy}}},
	    {.next = [rules](const SimWorld &target) { return WorkNext(target, *rules); },
	     .skip = [](SimWorld &target, std::uint64_t passed, double) { WorkSkip(target, passed); }});
}
} // namespace Manifest::Procurement
