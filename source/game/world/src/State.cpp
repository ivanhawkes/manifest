#include <manifest/world/State.h>

#include <format>
#include <tuple>

#include <jettison/reflect/Reflect.h>

#include <manifest/world/Identities.h>

namespace Manifest::World
{

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
	return std::nullopt;
}


std::optional<std::string> Broken(const Order &order)
{
	if ((order.ownerKind != Kind::Company && order.ownerKind != Kind::Broker) ||
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
		    .Value<Kind::Order>("Order");
		RegisterType<Identity>("Manifest.World.Identity")
		    .Field<&Identity::kind>("kind", kSaved)
		    .Field<&Identity::id>("id", kSaved);
		RegisterType<Principal>("Manifest.World.Principal")
		    .Field<&Principal::displayName>("displayName", kSaved);
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
		RegisterType<Cargo>("Manifest.World.Cargo").Field<&Cargo::lots>("lots", kSaved);
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
		RegisterType<IdentityCounters>("Manifest.World.IdentityCounters")
		    .Field<&IdentityCounters::issued>("issued", kSaved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::World
