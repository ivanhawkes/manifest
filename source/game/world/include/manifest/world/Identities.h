#pragma once

#include <entt/entity/registry.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <jettison/sim/SimWorld.h>

#include <manifest/world/State.h>

namespace Manifest::World
{
// The saved part of Identities: how many IDs each issued kind has handed out.
struct IdentityCounters
{
	std::map<Kind, std::uint64_t> issued;
};

// The hosted world's names: the lookup from a kind and ID to its entity, and the counters new IDs
// come from. The counters are saved; the lookup is derived from the entities' Identity components
// and rebuilt on every load. An entity with an Identity is created through Create and no other way,
// or the lookup misses it.
class Identities final : public Jettison::Sim::WorldService
{
  public:
	// A new entity in the registry with its Identity. Null, creating nothing, for an empty ID or one
	// the kind already has.
	entt::entity Create(entt::registry &registry, Kind kind, std::string id);

	// Destroys an entity Create made, and its name with it.
	void Destroy(entt::registry &registry, entt::entity entity);

	// Null when the world has no such entity.
	entt::entity Find(Kind kind, const std::string &id) const;

	// An ID for a new Principal, Company, Organisation, Ship or Order, "<Kind>n" with n counting from
	// 1, skipping any the kind already has, and never handed out twice, across saves. The number is
	// spent here, so a caller checks everything that could reject the action first. None once the
	// kind's counter is exhausted. A kind that is not issued (a region, an account) is a programming
	// error.
	std::optional<std::string> Issue(Kind kind);

	// The n of the kind's last ID Issue gave, 0 before the first.
	std::uint64_t LastIssued(Kind kind) const
	{
		const auto issued = m_counters.issued.find(kind);
		return issued == m_counters.issued.end() ? 0 : issued->second;
	}

	// The totes each hull's hold carries, by hull ID, which Load holds each ship's cargo to; a hull it
	// does not name carries none. Unset, as in a world with no company commands, a load checks each
	// hold against kMaxHoldTotes alone.
	void SetHolds(std::map<std::string, std::uint64_t, std::less<>> holds)
	{
		m_holds = std::move(holds);
	}

	void Save(Jettison::Serialise::BinaryWriter &writer) const override;

	// Fails the save for two entities of one kind and ID, an empty ID, a Stock, Cargo, Order,
	// Production or Pantry that breaks its rules (Broken), a holding's reserved totes that are not its
	// owner's sell orders' there, or, once SetHolds has run, a ship carrying more than its hull's hold.
	std::function<void()> Load(
	    Jettison::Serialise::BinaryReader &reader, const entt::registry &loaded,
	    std::span<const entt::meta_any> queued) override;

  private:
	std::map<std::pair<Kind, std::string>, entt::entity> m_index;
	IdentityCounters m_counters;
	std::optional<std::map<std::string, std::uint64_t, std::less<>>> m_holds;
};
} // namespace Manifest::World
