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

// What a project needs, by its ID: its inputs' totes, by item, and the steps of its work.
struct ProjectNeeds
{
	std::map<std::string, std::uint64_t, std::less<>> inputs;
	std::uint64_t workSteps{0};
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

	// The most of each fuel a ship holds, its tanks' reaction mass and its magazine's pellets, by its
	// hull and the systems fitted to it; none for systems it cannot carry.
	using FuelTanksOf = std::function<std::optional<Fuel>(const Ship &, const Systems *)>;

	// The rule FuelTanks answers by and Load holds each ship's Fuel to. Unset, a load checks only that
	// each ship's fuel is a number of at least 0.
	void SetFuelTanks(FuelTanksOf tanks)
	{
		m_fuelTanks = std::move(tanks);
	}

	// The ship's tanks by the rule SetFuelTanks gave; none before it has run.
	std::optional<Fuel> FuelTanks(const Ship &ship, const Systems *pSystems) const
	{
		return m_fuelTanks ? m_fuelTanks(ship, pSystems) : std::nullopt;
	}

	// What each hull's life support holds, by hull ID, and a fresh cartridge's occupant-days, which
	// Load holds each ship's LifeSupport to; a hull it does not name holds none. Unset, a load checks
	// only that each reserve is a number of at least 0.
	void SetLifeSupport(std::map<std::string, Installed, std::less<>> installed, double freshCartridge)
	{
		m_installed = std::move(installed);
		m_freshCartridge = freshCartridge;
	}

	// What each project needs, by project ID, which Load holds each Project and its Stock to. Unset, a
	// load checks only each Project's own rules (Broken).
	void SetProjects(std::map<std::string, ProjectNeeds, std::less<>> projects)
	{
		m_projects = std::move(projects);
	}

	// The pad classes every venue's Fees name, each once and in name order, and the highest arrears
	// rate one may post (D-100), which Load holds each Fees to. Unset, a load checks each Fees' own
	// rules alone.
	void SetCharges(std::vector<std::string> padClasses, int maxArrearsRateBps)
	{
		m_padClasses = std::move(padClasses);
		m_maxArrearsRateBps = maxArrearsRateBps;
	}

	// What one tote of each traded item holds in its unit, by item ID, which Load holds each opened
	// tote in a hold or a pantry to, and the gross a withheld credit is for, 0 for no tax, which each
	// production's tax carry is below (0 with no tax). Unset, a load checks only that an opened tote
	// holds more than nothing and a carry is not below 0.
	void SetTotes(std::map<std::string, double, std::less<>> unitsPerTote, std::int64_t perTaxCredit)
	{
		m_unitsPerTote = std::move(unitsPerTote);
		m_perTaxCredit = perTaxCredit;
	}

	void Save(Jettison::Serialise::BinaryWriter &writer) const override;

	// Fails the save for two entities of one kind and ID, an empty ID, a Stock, Cargo, Order,
	// Production, Pantry, Sales, Reports, Fees, Arrears, Policy or Job that breaks its rules (Broken),
	// arrears on anything but a docked ship, a market report
	// of a venue the world does not hold, a holding's reserved totes that are not its owner's sell
	// orders' there, a LifeSupport of no number or less than nothing, or connected to no venue, Systems
	// that break their rules, or, once SetHolds has run, a ship with no hold or carrying more than it
	// holds, lots and opened totes together, and once SetLifeSupport has, more in a tank or a cartridge
	// than its hull's holds; a Fuel of no number or less than nothing, or once SetFuelTanks has run,
	// more of either than its ship holds; Knowledge, Facilities or a Project that break their rules, a
	// Project with no Stock or of no region with its Knowledge and Facilities, and once SetProjects has
	// run, a project of no ID it gave, worked past its steps or Done short of them, or holding what it
	// does not need: an item it does not take, more than it takes, or anything once its inputs are
	// consumed; a Tender that breaks its rules, of a region or project the world does not hold, or open
	// without its budget in its account, awarded without its total less what it paid (Owed) there or
	// its bond in its BondAccount or to no company, or closed with either; a TenderAccount or
	// BondAccount of no tender; and a Bid that breaks its rules, of no company, or on no open tender or
	// past its budget, or not by its ID; once SetCharges has run, a market with no Fees or Fees of
	// other pad classes or past the highest arrears rate, and once SetTotes has, an opened tote holding
	// more than a tote of its item, or a tax carry not below the gross a withheld credit is for.
	std::function<void()> Load(
	    Jettison::Serialise::BinaryReader &reader, const entt::registry &loaded,
	    std::span<const entt::meta_any> queued) override;

  private:
	void CheckProject(
	    Jettison::Serialise::BinaryReader &reader, const Project &project, const Stock &stock) const;

	std::map<std::pair<Kind, std::string>, entt::entity> m_index;
	IdentityCounters m_counters;
	HoldOf m_holds;
	FuelTanksOf m_fuelTanks;
	std::optional<std::map<std::string, Installed, std::less<>>> m_installed;
	double m_freshCartridge{0.0};
	std::optional<std::map<std::string, ProjectNeeds, std::less<>>> m_projects;
	std::optional<std::vector<std::string>> m_padClasses;
	int m_maxArrearsRateBps{0};
	std::optional<std::map<std::string, double, std::less<>>> m_unitsPerTote;
	std::int64_t m_perTaxCredit{0};
};
} // namespace Manifest::World
