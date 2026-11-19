#pragma once

#include <cstdint>
#include <string>

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
	HouseholdAccount
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

// The components and their service's record, with Reflect. Safe to call again.
void RegisterTypes();
} // namespace Manifest::World
