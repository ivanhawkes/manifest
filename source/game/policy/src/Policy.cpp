#include <manifest/policy/Policy.h>

#include <jettison/core/Assert.h>
#include <jettison/reflect/Reflect.h>

#include <manifest/world/State.h>

namespace Manifest::Policy
{
namespace
{
using Command::Actor;
using Command::Result;
using Jettison::Sim::SimWorld;
using World::Kind;


Result Sets(SimWorld &world, const World::Identities &identities, const Actor &actor, const Set &set)
{
	entt::registry &registry = world.Registry();
	const entt::entity ship = identities.Find(Kind::Ship, set.shipId);
	if (ship == entt::null)
		return Result::UnknownTarget;
	const entt::entity company =
	    identities.Find(Kind::Company, registry.get<const World::Ship>(ship).companyId);
	JETTISON_VERIFY(company != entt::null);
	if (registry.get<const World::Company>(company).principalId != actor.id)
		return Result::NotAuthorised;
	if (set.targetProfit < 0 || set.lossAllowance < 0)
		return Result::InvalidQuantity;
	registry.emplace_or_replace<World::Policy>(ship, set.enabled, set.targetProfit, set.lossAllowance);
	if (!registry.all_of<World::Job>(ship))
		registry.emplace<World::Job>(ship);
	return Result::Succeeded;
}
} // namespace


void RegisterTypes()
{
	static const bool registered = []
	{
		World::RegisterTypes();
		using namespace Jettison::Reflect;
		constexpr Attribute kSaved = Attribute::Saved;
		RegisterType<Set>("Manifest.Policy.Set")
		    .Field<&Set::shipId>("shipId", kSaved)
		    .Field<&Set::enabled>("enabled", kSaved)
		    .Field<&Set::targetProfit>("targetProfit", kSaved)
		    .Field<&Set::lossAllowance>("lossAllowance", kSaved);
		return true;
	}();
	(void)registered;
}


void Register(
    SimWorld & /*world*/, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet & /*content*/)
{
	requests.Register<Set>(
	    std::string{kSet}, kVersion, Command::ActorKind::Principal,
	    [&identities](SimWorld &on, const Actor &actor, const Set &set)
	    { return Sets(on, identities, actor, set); });
}
} // namespace Manifest::Policy
