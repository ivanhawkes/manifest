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
		if (holding.available > Core::kMaxWarehouseIbcs ||
		    holding.reserved > Core::kMaxWarehouseIbcs - holding.available)
			return std::format(
			    "the holding of '{}' at '{}' is over a warehouse's {} IBCs", holding.itemId,
			    holding.venueId, Core::kMaxWarehouseIbcs);
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
		if (lot.itemId.empty() || lot.containers == 0)
			return "a lot names no item or holds no IBC";
		if (lot.containers > Core::kMaxHoldIbcs - carried)
			return std::format("the lots are over a hold's {} IBCs", Core::kMaxHoldIbcs);
		carried += lot.containers;
		if (at > 0 && cargo.lots[at - 1].itemId >= lot.itemId)
			return "the lots are out of order or held twice";
	}
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
		    .Value<Kind::Ship>("Ship");
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
		    .Field<&Region::playerHome>("playerHome", kSaved)
		    .Field<&Region::population>("population", kSaved)
		    .Field<&Region::health>("health", kSaved)
		    .Field<&Region::happiness>("happiness", kSaved);
		RegisterType<Account>("Manifest.World.Account").Field<&Account::balance>("balance", kSaved);
		RegisterType<Ship>("Manifest.World.Ship")
		    .Field<&Ship::companyId>("companyId", kSaved)
		    .Field<&Ship::hullId>("hullId", kSaved)
		    .Field<&Ship::dockedAt>("dockedAt", kSaved);
		RegisterType<Holding>("Manifest.World.Holding")
		    .Field<&Holding::venueId>("venueId", kSaved)
		    .Field<&Holding::itemId>("itemId", kSaved)
		    .Field<&Holding::available>("available", kSaved)
		    .Field<&Holding::reserved>("reserved", kSaved);
		RegisterType<Stock>("Manifest.World.Stock").Field<&Stock::holdings>("holdings", kSaved);
		RegisterType<Lot>("Manifest.World.Lot")
		    .Field<&Lot::itemId>("itemId", kSaved)
		    .Field<&Lot::containers>("containers", kSaved);
		RegisterType<Cargo>("Manifest.World.Cargo").Field<&Cargo::lots>("lots", kSaved);
		RegisterType<IdentityCounters>("Manifest.World.IdentityCounters")
		    .Field<&IdentityCounters::issued>("issued", kSaved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::World
