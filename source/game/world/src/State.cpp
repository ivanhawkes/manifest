#include <manifest/world/State.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <tuple>

#include <jettison/reflect/Reflect.h>

#include <manifest/world/Identities.h>

namespace Manifest::World
{

Kind AccountOf(Kind owner)
{
	switch (owner)
	{
		case Kind::Broker: return Kind::BrokerAccount;
		case Kind::Employer: return Kind::EmployerAccount;
		case Kind::Household: return Kind::HouseholdAccount;
		case Kind::Company:
		case Kind::Principal:
		case Kind::Organisation:
		case Kind::Region:
		case Kind::CompanyAccount:
		case Kind::PublicAccount:
		case Kind::HouseholdAccount:
		case Kind::Ship:
		case Kind::BrokerAccount:
		case Kind::VenueAccount:
		case Kind::Order:
		case Kind::EmployerAccount:
		case Kind::Market: break;
	}
	return Kind::CompanyAccount;
}


std::optional<std::string> Broken(const Stock &stock)
{
	for (std::size_t at = 0; at < stock.holdings.size(); ++at)
	{
		const Holding &holding = stock.holdings[at];
		if (holding.venueId.empty() || holding.itemId.empty())
			return "a holding names no venue or item";
		if (holding.available == 0 && holding.reserved == 0)
			return std::format("the holding of '{}' at '{}' is empty", holding.itemId, holding.venueId);
		if (holding.available > Core::kMaxWarehouseTotes ||
		    holding.reserved > Core::kMaxWarehouseTotes - holding.available)
			return std::format(
			    "the holding of '{}' at '{}' is over a warehouse's {} totes", holding.itemId,
			    holding.venueId, Core::kMaxWarehouseTotes);
		if (at > 0 && std::tie(stock.holdings[at - 1].venueId, stock.holdings[at - 1].itemId) >=
		                  std::tie(holding.venueId, holding.itemId))
			return "the holdings are out of order or held twice";
	}
	return std::nullopt;
}


std::optional<std::string> Broken(const Cargo &cargo)
{
	std::uint64_t carried = 0;
	for (std::size_t at = 0; at < cargo.lots.size(); ++at)
	{
		const Lot &lot = cargo.lots[at];
		if (lot.itemId.empty() || lot.totes == 0)
			return "a lot names no item or holds no tote";
		if (lot.totes > Core::kMaxHoldTotes - carried)
			return std::format("the lots are over a hold's {} totes", Core::kMaxHoldTotes);
		carried += lot.totes;
		if (at > 0 && cargo.lots[at - 1].itemId >= lot.itemId)
			return "the lots are out of order or held twice";
	}
	for (std::size_t at = 0; at < cargo.opened.size(); ++at)
	{
		const Opened &opened = cargo.opened[at];
		if (opened.itemId.empty() || !std::isfinite(opened.quantity) || opened.quantity <= 0.0)
			return "an opened tote names no item or holds nothing";
		if (carried == Core::kMaxHoldTotes)
			return std::format(
			    "the lots and opened totes are over a hold's {} totes", Core::kMaxHoldTotes);
		++carried;
		if (at > 0 && cargo.opened[at - 1].itemId >= opened.itemId)
			return "the opened totes are out of order or opened twice";
	}
	return std::nullopt;
}


std::optional<std::string> Broken(const Order &order)
{
	if ((order.ownerKind != Kind::Company && order.ownerKind != Kind::Broker &&
	     order.ownerKind != Kind::Employer && order.ownerKind != Kind::Household) ||
	    order.ownerId.empty() || order.venueId.empty() || order.itemId.empty())
		return "an order names no company or broker, venue or item";
	if (order.limit <= 0 || order.feeBps < 0 || order.feeBps > 10'000 || order.sequence == 0)
		return std::format("order {} has no price, fee or place in the book", order.sequence);
	if (order.remaining == 0 || order.remaining > order.totes || order.totes > Core::kMaxWarehouseTotes)
		return std::format("order {} has nothing left or more than it was placed for", order.sequence);
	if (order.escrow < 0 || (order.side == Side::Sell && order.escrow != 0))
		return std::format("order {} holds escrow it cannot", order.sequence);
	return std::nullopt;
}


std::optional<std::string> Broken(const Production &production)
{
	if (production.taxCarry < 0)
		return "a production's tax carry is below 0";
	for (std::size_t at = 0; at < production.batches.size(); ++at)
	{
		const Batch &batch = production.batches[at];
		if (batch.venueId.empty() || batch.itemId.empty() || batch.totes == 0 ||
		    batch.totes > Core::kMaxWarehouseTotes)
			return "a batch names no venue or item, or makes no tote or more than a warehouse holds";
		if (!std::isfinite(batch.duration) || batch.duration <= 0.0 || !std::isfinite(batch.progress) ||
		    batch.progress < 0.0 || batch.progress > batch.duration)
			return std::format(
			    "the batch of '{}' at '{}' has no duration or progress past it", batch.itemId,
			    batch.venueId);
		if (batch.wages < 0 || batch.paid < 0 || batch.paid > batch.wages)
			return std::format(
			    "the batch of '{}' at '{}' has paid more than its wages", batch.itemId, batch.venueId);
		if (at > 0 && std::tie(production.batches[at - 1].venueId, production.batches[at - 1].itemId) >=
		                  std::tie(batch.venueId, batch.itemId))
			return "the batches are out of order or under way twice";
	}
	return std::nullopt;
}


std::optional<std::string> Broken(const Pantry &pantry)
{
	for (const Opened &opened : pantry.opened)
		if (opened.itemId.empty() || !std::isfinite(opened.quantity) || opened.quantity <= 0.0)
			return "an opened tote names no item or holds nothing";
	for (const Unmet &unmet : pantry.unmet)
		if (unmet.needId.empty() || !std::isfinite(unmet.quantity) || unmet.quantity < 0.0)
			return "an unmet need names no need or is below 0";
	if (!std::isfinite(pantry.severity) || pantry.severity < 0.0 || pantry.severity > 1.0)
		return "a pantry's severity is not in [0, 1]";
	return std::nullopt;
}


std::optional<std::string> Broken(const Systems &systems)
{
	for (std::size_t at = 0; at < systems.fitted.size(); ++at)
	{
		const Fitting &fitting = systems.fitted[at];
		if (fitting.skillId.empty())
			return "a system names no skill";
		if (fitting.tier < 1)
			return std::format("the system '{}' is fitted at tier {}", fitting.skillId, fitting.tier);
		if (at > 0 && systems.fitted[at - 1].skillId >= fitting.skillId)
			return "the systems are out of order or fitted twice";
	}
	return std::nullopt;
}


std::optional<std::string> Broken(const LifeSupport &lifeSupport)
{
	for (const std::vector<double> *const reserve : {&lifeSupport.tanks, &lifeSupport.cartridges})
		for (const double left : *reserve)
			if (!std::isfinite(left) || left < 0.0)
				return "a tank or a cartridge holds less than nothing, or no number";
	return std::nullopt;
}


std::optional<std::string> Broken(const Sales &sales)
{
	for (std::size_t at = 0; at < sales.recent.size(); ++at)
	{
		const Sold &sold = sales.recent[at];
		if (sold.itemId.empty() || sold.totes == 0)
			return "a sale names no item or sold no tote";
		if (at > 0 && std::tie(sales.recent[at - 1].step, sales.recent[at - 1].itemId) >=
		                  std::tie(sold.step, sold.itemId))
			return "the recent sales are out of order or listed twice";
	}
	for (std::size_t at = 0; at < sales.last.size(); ++at)
	{
		const LastSale &last = sales.last[at];
		if (last.itemId.empty() || last.price <= 0)
			return "a last sale names no item or price";
		if (at > 0 && sales.last[at - 1].itemId >= last.itemId)
			return "the last sales are out of order or listed twice";
	}
	return std::nullopt;
}


std::optional<std::string> Broken(const Reports &reports)
{
	for (std::size_t at = 0; at < reports.reports.size(); ++at)
	{
		const Report &report = reports.reports[at];
		if (report.venueId.empty() || report.since > report.observed)
			return "a market report names no venue, or its window starts after it was observed";
		if (at > 0 && reports.reports[at - 1].venueId >= report.venueId)
			return "the market reports are out of order or held twice";
		for (const Line &line : report.lines)
			if (line.itemId.empty() || line.bid < 0 || line.ask < 0 || line.lastPrice < 0 ||
			    (line.bid == 0) != (line.bidTotes == 0) || (line.ask == 0) != (line.askTotes == 0) ||
			    line.lastStep > report.observed)
				return std::format("the report of '{}' has a good of no item or price", report.venueId);
	}
	return std::nullopt;
}


std::optional<std::string> Broken(const Fees &fees)
{
	if (fees.berthPerOccupantHour < 0 || fees.arrearsRateBps < 0)
		return "a venue's fee is below 0";
	for (auto each = fees.classes.begin(); each != fees.classes.end(); ++each)
	{
		if (each->padClass.empty() || each->docking < 0 || each->berthPerHour < 0)
			return "a venue's fee is of no pad class or below 0";
		if (each != fees.classes.begin() && std::prev(each)->padClass >= each->padClass)
			return "a venue's fees are not each of one pad class, in name order";
	}
	return std::nullopt;
}


const ClassFees *Find(const Fees &fees, std::string_view padClass)
{
	const auto found = std::ranges::find(fees.classes, padClass, &ClassFees::padClass);
	return found == fees.classes.end() ? nullptr : &*found;
}


std::optional<std::string> Broken(const Arrears &arrears)
{
	if (arrears.owed <= 0)
		return "a ship's arrears are not above 0";
	if (arrears.carry < 0 || arrears.carry >= kRateYear)
		return "a ship's arrears carry a part of a credit outside [0, 1)";
	return std::nullopt;
}


std::optional<std::string> Broken(const Policy &policy)
{
	if (policy.targetProfit < 0 || policy.lossAllowance < 0)
		return "a policy's target or allowance is below 0";
	return std::nullopt;
}


std::optional<std::string> Broken(const Job &job)
{
	if (job.cost < 0 || job.charges < 0 || job.allowanceSpent < 0)
		return "a job's lot costs, is charged or has spent less than nothing";
	if (job.totes == 0 && (job.cost != 0 || job.charges != 0 || job.allowanceSpent != 0))
		return "a job with no lot has its cost, charges or allowance";
	if ((job.phase == Phase::Blocked) != (job.blockage != Blockage::None))
		return "a job is blocked with no reason, or has one and is not blocked";
	const bool buying = job.phase == Phase::ToSource || job.phase == Phase::Buying;
	const bool selling = job.phase == Phase::ToDestination || job.phase == Phase::Selling;
	if ((buying || selling) && (job.itemId.empty() || job.destinationId.empty()))
		return "a job under way names no item or destination";
	if (buying && (job.sourceId.empty() || job.totes != 0))
		return "a job buying names no source or already has its lot";
	if (selling && job.totes == 0)
		return "a job selling has no lot";
	if (job.totes > 0 && job.itemId.empty())
		return "a job's lot names no item";
	return std::nullopt;
}


void RegisterTypes()
{
	static const bool registered = []
	{
		Content::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;

		RegisterEnum<Kind>("Manifest.World.Kind")
		    .Value<Kind::Principal>("Principal")
		    .Value<Kind::Company>("Company")
		    .Value<Kind::Organisation>("Organisation")
		    .Value<Kind::Region>("Region")
		    .Value<Kind::CompanyAccount>("CompanyAccount")
		    .Value<Kind::PublicAccount>("PublicAccount")
		    .Value<Kind::HouseholdAccount>("HouseholdAccount")
		    .Value<Kind::Ship>("Ship")
		    .Value<Kind::Broker>("Broker")
		    .Value<Kind::BrokerAccount>("BrokerAccount")
		    .Value<Kind::VenueAccount>("VenueAccount")
		    .Value<Kind::Order>("Order")
		    .Value<Kind::Employer>("Employer")
		    .Value<Kind::EmployerAccount>("EmployerAccount")
		    .Value<Kind::Household>("Household")
		    .Value<Kind::Market>("Market");
		RegisterType<Identity>("Manifest.World.Identity")
		    .Field<&Identity::kind>("kind", kSaved)
		    .Field<&Identity::id>("id", kSaved);
		RegisterType<Principal>("Manifest.World.Principal")
		    .Field<&Principal::displayName>("displayName", kSaved)
		    .Field<&Principal::npc>("npc", kSaved);
		RegisterType<Company>("Manifest.World.Company")
		    .Field<&Company::principalId>("principalId", kSaved)
		    .Field<&Company::displayName>("displayName", kSaved);
		RegisterType<Organisation>("Manifest.World.Organisation")
		    .Field<&Organisation::kind>("kind", kSaved)
		    .Field<&Organisation::displayName>("displayName", kSaved)
		    .Field<&Organisation::principalId>("principalId", kSaved);
		RegisterType<Region>("Manifest.World.Region")
		    .Field<&Region::planetId>("planetId", kSaved)
		    .Field<&Region::civilisationId>("civilisationId", kSaved)
		    .Field<&Region::governorId>("governorId", kSaved)
		    .Field<&Region::population>("population", kSaved)
		    .Field<&Region::health>("health", kSaved)
		    .Field<&Region::happiness>("happiness", kSaved);
		RegisterType<Account>("Manifest.World.Account").Field<&Account::balance>("balance", kSaved);
		RegisterType<Ship>("Manifest.World.Ship")
		    .Field<&Ship::companyId>("companyId", kSaved)
		    .Field<&Ship::hullId>("hullId", kSaved)
		    .Field<&Ship::dockedAt>("dockedAt", kSaved);
		RegisterEnum<Leg>("Manifest.World.Leg")
		    .Value<Leg::Departing>("Departing")
		    .Value<Leg::Clearing>("Clearing")
		    .Value<Leg::Cruising>("Cruising")
		    .Value<Leg::Arriving>("Arriving")
		    .Value<Leg::Arrived>("Arrived")
		    .Value<Leg::Failed>("Failed");
		RegisterEnum<Failure>("Manifest.World.Failure")
		    .Value<Failure::None>("None")
		    .Value<Failure::DepartureRefused>("DepartureRefused")
		    .Value<Failure::CruiseRefused>("CruiseRefused")
		    .Value<Failure::PadRefused>("PadRefused")
		    .Value<Failure::PadLost>("PadLost");
		RegisterType<Journey>("Manifest.World.Journey")
		    .Field<&Journey::venueId>("venueId", kSaved)
		    .Field<&Journey::leg>("leg", kSaved)
		    .Field<&Journey::failure>("failure", kSaved)
		    .Field<&Journey::started>("started", kSaved);
		RegisterType<Holding>("Manifest.World.Holding")
		    .Field<&Holding::venueId>("venueId", kSaved)
		    .Field<&Holding::itemId>("itemId", kSaved)
		    .Field<&Holding::available>("available", kSaved)
		    .Field<&Holding::reserved>("reserved", kSaved);
		RegisterType<Stock>("Manifest.World.Stock").Field<&Stock::holdings>("holdings", kSaved);
		RegisterType<Lot>("Manifest.World.Lot")
		    .Field<&Lot::itemId>("itemId", kSaved)
		    .Field<&Lot::totes>("totes", kSaved);
		RegisterType<Opened>("Manifest.World.Opened")
		    .Field<&Opened::itemId>("itemId", kSaved)
		    .Field<&Opened::quantity>("quantity", kSaved);
		RegisterType<Cargo>("Manifest.World.Cargo")
		    .Field<&Cargo::lots>("lots", kSaved)
		    .Field<&Cargo::opened>("opened", kSaved);
		RegisterType<Fitting>("Manifest.World.Fitting")
		    .Field<&Fitting::skillId>("skillId", kSaved)
		    .Field<&Fitting::tier>("tier", kSaved);
		RegisterType<Systems>("Manifest.World.Systems").Field<&Systems::fitted>("fitted", kSaved);
		RegisterType<LifeSupport>("Manifest.World.LifeSupport")
		    .Field<&LifeSupport::tanks>("tanks", kSaved)
		    .Field<&LifeSupport::cartridges>("cartridges", kSaved)
		    .Field<&LifeSupport::connectedTo>("connectedTo", kSaved);
		RegisterType<Broker>("Manifest.World.Broker").Field<&Broker::operatorId>("operatorId", kSaved);
		RegisterEnum<Side>("Manifest.World.Side").Value<Side::Buy>("Buy").Value<Side::Sell>("Sell");
		RegisterType<Order>("Manifest.World.Order")
		    .Field<&Order::ownerKind>("ownerKind", kSaved)
		    .Field<&Order::ownerId>("ownerId", kSaved)
		    .Field<&Order::venueId>("venueId", kSaved)
		    .Field<&Order::itemId>("itemId", kSaved)
		    .Field<&Order::side>("side", kSaved)
		    .Field<&Order::limit>("limit", kSaved)
		    .Field<&Order::totes>("totes", kSaved)
		    .Field<&Order::remaining>("remaining", kSaved)
		    .Field<&Order::escrow>("escrow", kSaved)
		    .Field<&Order::feeBps>("feeBps", kSaved)
		    .Field<&Order::sequence>("sequence", kSaved)
		    .Field<&Order::expiresStep>("expiresStep", kSaved)
		    .Field<&Order::request>("request", kSaved);
		RegisterType<Batch>("Manifest.World.Batch")
		    .Field<&Batch::venueId>("venueId", kSaved)
		    .Field<&Batch::itemId>("itemId", kSaved)
		    .Field<&Batch::totes>("totes", kSaved)
		    .Field<&Batch::duration>("duration", kSaved)
		    .Field<&Batch::progress>("progress", kSaved)
		    .Field<&Batch::wages>("wages", kSaved)
		    .Field<&Batch::paid>("paid", kSaved);
		RegisterType<Production>("Manifest.World.Production")
		    .Field<&Production::batches>("batches", kSaved)
		    .Field<&Production::taxCarry>("taxCarry", kSaved);
		RegisterType<Unmet>("Manifest.World.Unmet")
		    .Field<&Unmet::needId>("needId", kSaved)
		    .Field<&Unmet::quantity>("quantity", kSaved);
		RegisterType<Pantry>("Manifest.World.Pantry")
		    .Field<&Pantry::opened>("opened", kSaved)
		    .Field<&Pantry::unmet>("unmet", kSaved)
		    .Field<&Pantry::severity>("severity", kSaved)
		    .Field<&Pantry::shortIntervals>("shortIntervals", kSaved);
		RegisterType<Sold>("Manifest.World.Sold")
		    .Field<&Sold::step>("step", kSaved)
		    .Field<&Sold::itemId>("itemId", kSaved)
		    .Field<&Sold::totes>("totes", kSaved);
		RegisterType<LastSale>("Manifest.World.LastSale")
		    .Field<&LastSale::itemId>("itemId", kSaved)
		    .Field<&LastSale::step>("step", kSaved)
		    .Field<&LastSale::price>("price", kSaved);
		RegisterType<Sales>("Manifest.World.Sales")
		    .Field<&Sales::recent>("recent", kSaved)
		    .Field<&Sales::last>("last", kSaved);
		RegisterEnum<Source>("Manifest.World.Source")
		    .Value<Source::Docked>("Docked")
		    .Value<Source::Starting>("Starting");
		RegisterType<Line>("Manifest.World.Line")
		    .Field<&Line::itemId>("itemId", kSaved)
		    .Field<&Line::bid>("bid", kSaved)
		    .Field<&Line::bidTotes>("bidTotes", kSaved)
		    .Field<&Line::ask>("ask", kSaved)
		    .Field<&Line::askTotes>("askTotes", kSaved)
		    .Field<&Line::sold>("sold", kSaved)
		    .Field<&Line::lastStep>("lastStep", kSaved)
		    .Field<&Line::lastPrice>("lastPrice", kSaved);
		RegisterType<Report>("Manifest.World.Report")
		    .Field<&Report::venueId>("venueId", kSaved)
		    .Field<&Report::source>("source", kSaved)
		    .Field<&Report::observed>("observed", kSaved)
		    .Field<&Report::since>("since", kSaved)
		    .Field<&Report::lines>("lines", kSaved);
		RegisterType<Reports>("Manifest.World.Reports").Field<&Reports::reports>("reports", kSaved);
		RegisterType<ClassFees>("Manifest.World.ClassFees")
		    .Field<&ClassFees::padClass>("padClass", kSaved)
		    .Field<&ClassFees::docking>("docking", kSaved)
		    .Field<&ClassFees::berthPerHour>("berthPerHour", kSaved);
		RegisterType<Fees>("Manifest.World.Fees")
		    .Field<&Fees::classes>("classes", kSaved)
		    .Field<&Fees::berthPerOccupantHour>("berthPerOccupantHour", kSaved)
		    .Field<&Fees::arrearsRateBps>("arrearsRateBps", kSaved);
		RegisterType<Pricing>("Manifest.World.Pricing").Field<&Pricing::levelBps>("levelBps", kSaved);
		RegisterType<Arrears>("Manifest.World.Arrears")
		    .Field<&Arrears::owed>("owed", kSaved)
		    .Field<&Arrears::carry>("carry", kSaved);
		RegisterType<Policy>("Manifest.World.Policy")
		    .Field<&Policy::enabled>("enabled", kSaved)
		    .Field<&Policy::targetProfit>("targetProfit", kSaved)
		    .Field<&Policy::lossAllowance>("lossAllowance", kSaved);
		RegisterEnum<Phase>("Manifest.World.Phase")
		    .Value<Phase::Choosing>("Choosing")
		    .Value<Phase::ToSource>("ToSource")
		    .Value<Phase::Buying>("Buying")
		    .Value<Phase::ToDestination>("ToDestination")
		    .Value<Phase::Selling>("Selling")
		    .Value<Phase::Blocked>("Blocked");
		RegisterEnum<Blockage>("Manifest.World.Blockage")
		    .Value<Blockage::None>("None")
		    .Value<Blockage::NoTrade>("NoTrade")
		    .Value<Blockage::JourneyFailed>("JourneyFailed")
		    .Value<Blockage::Refused>("Refused");
		RegisterType<Job>("Manifest.World.Job")
		    .Field<&Job::phase>("phase", kSaved)
		    .Field<&Job::itemId>("itemId", kSaved)
		    .Field<&Job::sourceId>("sourceId", kSaved)
		    .Field<&Job::destinationId>("destinationId", kSaved)
		    .Field<&Job::totes>("totes", kSaved)
		    .Field<&Job::cost>("cost", kSaved)
		    .Field<&Job::charges>("charges", kSaved)
		    .Field<&Job::allowanceSpent>("allowanceSpent", kSaved)
		    .Field<&Job::estimate>("estimate", kSaved)
		    .Field<&Job::blockage>("blockage", kSaved)
		    .Field<&Job::blockedSince>("blockedSince", kSaved)
		    .Field<&Job::retry>("retry", kSaved);
		RegisterType<IdentityCounters>("Manifest.World.IdentityCounters")
		    .Field<&IdentityCounters::issued>("issued", kSaved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::World
