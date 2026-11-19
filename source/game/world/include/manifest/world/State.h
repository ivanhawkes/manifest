#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <manifest/content/Definitions.h>

namespace Manifest::World
{
// What an entity of the hosted world is. The kind is part of its identity, so a company and a
// region that share a textual ID are two entities, and so are a region's public and household
// accounts, which both carry the region's ID.
enum class Kind
{
	Principal,
	Company,
	Organisation,
	Region,
	CompanyAccount,
	PublicAccount,
	HouseholdAccount,
	Ship
};

// Every hosted entity has one, given when it is created and never changed (Identities::Create).
struct Identity
{
	Kind kind{Kind::Principal};
	std::string id;
};

// A player's control identity in the world. Credentials and connections are not world state.
struct Principal
{
	std::string displayName;
};

struct Company
{
	std::string principalId;
	std::string displayName;
};

struct Organisation
{
	Content::OrganisationKind kind{Content::OrganisationKind::Polity};
	std::string displayName;
	// The player who directs it; empty for one the AI runs.
	std::string principalId;
};

// The one owner of a population. Its money is in its PublicAccount and HouseholdAccount.
struct Region
{
	std::string planetId;
	std::string civilisationId;
	// An Organisation; empty only for a player's home not yet allocated.
	std::string governorId;
	bool playerHome{false};
	double population{0.0};
	double health{0.0};
	double happiness{0.0};
};

// The owner is the account entity's Identity: its kind and the owner's ID.
struct Account
{
	// Whole credits.
	std::int64_t balance{0};
};

// A company's ship: its hull, a Manifest.Hull by ID, and the venue it is docked at, empty when it
// is not. Stage 3 seeds the dock at provisioning; no command sets it.
struct Ship
{
	std::string companyId;
	std::string hullId;
	std::string dockedAt;

	bool operator==(const Ship &) const = default;
};

// An owner's goods at one venue's warehouse, in IBCs (D-078), their total at most kMaxWarehouseIbcs.
// Reserved IBCs back an order and cannot move; the total held is the sum.
struct Holding
{
	std::string venueId;
	std::string itemId;
	std::uint64_t available{0};
	std::uint64_t reserved{0};

	bool operator==(const Holding &) const = default;
};

// Every holding of the owner whose entity carries it, by (venue, item), none of them empty. The
// owner is the entity's Identity, so equal IDs of two kinds hold apart.
struct Stock
{
	std::vector<Holding> holdings;

	bool operator==(const Stock &) const = default;
};

// The IBCs of one item in a ship's hold, at most kMaxHoldIbcs.
struct Lot
{
	std::string itemId;
	std::uint64_t containers{0};

	bool operator==(const Lot &) const = default;
};

// What a ship carries, by item, each lot above 0. Its owner's, at no venue.
struct Cargo
{
	std::vector<Lot> lots;

	bool operator==(const Cargo &) const = default;
};

// Why a Stock or a Cargo breaks the rules above, for a load to refuse: none when it keeps them.
std::optional<std::string> Broken(const Stock &stock);

std::optional<std::string> Broken(const Cargo &cargo);

// The components and their service's record, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::World
