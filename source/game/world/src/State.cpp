#include <manifest/world/State.h>

#include <jettison/reflect/Reflect.h>

#include <manifest/world/Identities.h>

namespace Manifest::World
{
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
		    .Value<Kind::HouseholdAccount>("HouseholdAccount");
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
		RegisterType<IdentityCounters>("Manifest.World.IdentityCounters")
		    .Field<&IdentityCounters::issued>("issued", kSaved);
		return true;
	}();
	(void)registered;
}
} // namespace Manifest::World
