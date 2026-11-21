#pragma once

#include <entt/entity/registry.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <jettison/sim/SimWorld.h>

#include <manifest/world/State.h>

namespace Manifest::World
{
// What a hull's life support holds (D-050): each oxygen tank's capacity in kilograms, and how many
// scrubber cartridges are installed.
struct Installed
{
	std::vector<double> tanks;
	std::size_t cartridges{0};
};

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

	// The totes a ship's hold carries, by its hull and the systems fitted to it, none for a ship with
	// none; none at all for systems that give it no whole number of totes up to kMaxHoldTotes.
	using HoldOf = std::function<std::optional<std::uint64_t>(const Ship &, const Systems *)>;

	// The rule Hold answers by and Load holds each ship's cargo to. Unset, as in a world with no
	// company commands, a load checks each hold against kMaxHoldTotes alone.
	void SetHolds(HoldOf holds)
	{
		m_holds = std::move(holds);
	}

	// The ship's hold by the rule SetHolds gave; none before it has run.
	std::optional<std::uint64_t> Hold(const Ship &ship, const Systems *pSystems) const
	{
		return m_holds ? m_holds(ship, pSystems) : std::nullopt;
	}

	// What each hull's life support holds, by hull ID, and a fresh cartridge's occupant-days, which
	// Load holds each ship's LifeSupport to; a hull it does not name holds none. Unset, a load checks
	// only that each reserve is a number of at least 0.
	void SetLifeSupport(std::map<std::string, Installed, std::less<>> installed, double freshCartridge)
	{
		m_installed = std::move(installed);
		m_freshCartridge = freshCartridge;
	}

	void Save(Jettison::Serialise::BinaryWriter &writer) const override;

	// Fails the save for two entities of one kind and ID, an empty ID, a Stock, Cargo, Order,
	// Production, Pantry, Sales, Reports, Policy or Job that breaks its rules (Broken), a market report
	// of a venue the world does not hold, a holding's reserved totes that are not its owner's sell
	// orders' there, a LifeSupport of no number or less than nothing, or connected to no venue, Systems
	// that break their rules, or, once SetHolds has run, a ship with no hold or carrying more than it
	// holds, lots and opened totes together, and once SetLifeSupport has, more in a tank or a cartridge
	// than its hull's holds.
	std::function<void()> Load(
	    Jettison::Serialise::BinaryReader &reader, const entt::registry &loaded,
	    std::span<const entt::meta_any> queued) override;

  private:
	std::map<std::pair<Kind, std::string>, entt::entity> m_index;
	IdentityCounters m_counters;
	HoldOf m_holds;
	std::optional<std::map<std::string, Installed, std::less<>>> m_installed;
	double m_freshCartridge{0.0};
};
} // namespace Manifest::World
