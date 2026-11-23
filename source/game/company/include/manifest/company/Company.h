#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/travel/Travel.h>
#include <manifest/world/Identities.h>
#include <manifest/world/State.h>

namespace Manifest::Company
{
// The administrative command that provisions a player (D-089): their Principal, with their starting
// market reports (Trade::Seed), a company with the configured starting credits, and one ship of the
// scenario's starter hull, parked on a pad of its starter venue (Travel::Fleet::Park) with an empty
// hold, every oxygen tank full and every cartridge fresh, once, its reaction mass tanks and pellet
// magazine full, once (D-107, D-108), and connected to the venue's life support (D-047); no region or
// polity. Rejected AlreadyProvisioned for a principal the world has, NoPadFree with no free pad there
// the starter fits, NoServiceFree with no life-support connection free there, and MalformedPayload for
// an empty principal ID.
struct Provision
{
	std::string principalId;
	std::string displayName;
	std::string companyName;
};

// The players' commands that move goods between their company's stock at the venue a ship is docked
// at and the ship's hold, the one way and the other, neither changing their owner. The venue is the
// one the sender believes the ship is at. Each checks everything before it writes anything: the
// ship (UnknownTarget), that the sender's company owns it (NotAuthorised), that it is docked
// (NotDocked) at the venue (WrongVenue), the scenario's item (UnknownTarget), a count above 0
// (InvalidQuantity), enough available to move, reserved goods not counted (InsufficientStock), and
// room for them: the ship's hold, its opened totes taking a tote each, on loading, the warehouse's
// kMaxWarehouseTotes on unloading, less what the company's resting buy orders there may still fill
// (CapacityExceeded).
struct Move
{
	std::string shipId;
	std::string venueId;
	std::string itemId;
	std::uint64_t totes{0};
};

// The players' commands that resupply their company's ship's life support from its own hold, wherever
// it is (D-094). Refill fills its oxygen tanks, in order, from the tuning's oxygen item: an opened
// tote's first, then whole totes opened one at a time, what a tote has left kept opened in the hold.
// Replace takes one cartridge the same way and installs it fresh in place of the installed one with
// the least left, which is thrown away with what it had. Each checks before it writes: the ship
// (UnknownTarget), that the sender's company owns it (NotAuthorised), a hull with tanks to fill, not
// full, or cartridges to replace (CapacityExceeded), and the goods aboard (InsufficientStock).
struct Service
{
	std::string shipId;
};

// The players' command that buys a tier of an offer (Content::OfferDefinition) for their company's
// ship with the company's credits, paid to no one, while the ship is docked (D-096). The ship's
// Systems take the offer's skill at the tier, and its hold is then its hull's CargoVolume with every
// system's effect at its tier, as the effects engine computes a hull's stats. The tier is the one
// bought, one above the ship's, so a request repeated for a tier the ship has pays nothing and is
// refused AlreadyFitted. It checks before it writes: the ship (UnknownTarget), that the sender's
// company owns it (NotAuthorised), the offer, of a skill of the game mode (UnknownTarget), that the
// ship is docked (NotDocked), a tier above 0 (InvalidQuantity), not fitted already (AlreadyFitted),
// the next tier, at most the offer's, on a hull with a slot for the skill's effect and every
// prerequisite fitted to the ship at its tier (NotFittable), a hold of a whole number of totes up to
// kMaxHoldTotes (CapacityExceeded), and the cost in the company's account (InsufficientFunds).
struct Upgrade
{
	std::string shipId;
	std::string offerId;
	std::uint32_t tier{0};
};

// The command by which the principal directing a venue's operator organisation (World::Broker's
// operatorId, Organisation::principalId) posts its venue's whole World::Fees (D-100), charged from the
// next charge: a docking fee paid and an hour charged stay as they were. It checks before it writes:
// the venue (UnknownTarget), a sender who directs its operator, which no one does for an AI-run one
// (NotAuthorised), each pad class of the venue's posted fees named once in their order and every fee
// and rate at least 0 (InvalidQuantity), and an arrears rate at most the tuning's usury limit
// (RateTooHigh).
struct SetFees
{
	std::string venueId;
	World::Fees fees;
};

// The players' command that sets whether a journey from a venue first refuels their company's ship
// (World::Refuel), on until set off. Refuelling buys, at the venue's asks through its book
// (Trade::MakeBuyer) and within the company's account after what the journey still pays, the whole
// totes of the fuel tuning's reaction mass its tanks have room for, straight into them; where its hold
// has no pellets, a tote of them into its hold, as Load would, the hold's room allowing; and then fills
// the magazine from the pellets in its hold, opening a tote at a time (D-108). It checks the ship
// (UnknownTarget) and that the sender's company owns it (NotAuthorised).
struct SetRefuel
{
	std::string shipId;
	bool enabled{true};
};

// The players' command by which their company founds their homeworld (D-110): in a region no one
// governs, the company pays company.foundingPrice into the region's public account, and a new polity
// of the display name, its ID issued, directed by the sender, governs the region from this step. The
// region's population, accounts, condition, knowledge, facilities and projects carry on as they were.
// No command takes a governed region's governor away. It checks before it writes: the region and the
// company (UnknownTarget), that the sender's principal owns the company (NotAuthorised), a display
// name (MalformedPayload), a region with no governor (Governed), a sender who directs no region's
// governor yet (AlreadyFounded), the price in the company's account (InsufficientFunds) and room for
// it in the public account (CapacityExceeded).
struct Found
{
	std::string regionId;
	std::string companyId;
	std::string displayName;
};

inline constexpr std::string_view kProvision = "Manifest.Company.Provision";
inline constexpr std::string_view kFound = "Manifest.Company.Found";
inline constexpr std::string_view kLoad = "Manifest.Company.Load";
inline constexpr std::string_view kUnload = "Manifest.Company.Unload";
inline constexpr std::string_view kRefill = "Manifest.Company.Refill";
inline constexpr std::string_view kReplace = "Manifest.Company.Replace";
inline constexpr std::string_view kUpgrade = "Manifest.Company.Upgrade";
inline constexpr std::string_view kSetFees = "Manifest.Company.SetFees";
inline constexpr std::string_view kSetRefuel = "Manifest.Company.SetRefuel";

// The system, by the name the step's table gives it: after manifest.travel, it lets go of the
// life-support connection of each ship no longer docked at its venue, connects each docked ship while
// the venue has a connection free (Content::VenueDefinition::lifeSupportCapacity), in entity order,
// and draws each unconnected ship's step of use from its own tanks and cartridges, in order: the
// tuning's rates times its hull's occupants, never more than is left (D-046, D-094).
inline constexpr std::string_view kLifeSupportSystem = "manifest.lifeSupport";
// The system after it that prices each venue whose operator no principal directs (D-100), as each
// simulation day's last step ends: its World::Pricing level, in basis points of the tuning's defaults,
// rises by charges.pricing's stepBps when more of its station's pads (display pads aside) are taken
// than raiseAboveBps of them and falls by it when fewer than lowerBelowBps are, within minLevelBps to
// maxLevelBps, and each of its World::Fees is then the default at that level, to the nearest credit,
// the arrears rate the default's.
inline constexpr std::string_view kPricingSystem = "manifest.pricing";
// The system after it that charges the berth fee (D-097, D-100): as each simulation hour's last step
// ends, each ship docked then, in entity order, pays its World::Arrears and its venue's World::Fees,
// the berthPerHour of its fee class (Content::FeeClass) and berthPerOccupantHour for each of its hull's
// occupants while it is connected to the venue's life support, from its company's account to its
// venue's, as far as the one holds and the other has room, what is left owed carried as its arrears
// from the step it began,
// the fee added to the charges of the lot its World::Job has aboard. As each day's last step ends,
// first, the arrears grow by a day's interest at the venue's arrearsRateBps, at most the tuning's
// usury limit.
inline constexpr std::string_view kBerthSystem = "manifest.berths";
inline constexpr std::uint32_t kVersion = 1;

// The payloads, with Reflect, and the command types. Safe to call again.
void RegisterTypes();

// Registers the commands with the requests, the life-support, pricing and berth systems after
// manifest.travel, the ships' holds with the identities (World::Identities::SetHolds), and their
// refuelling with the fleet (Travel::Fleet::SetRefueller, SetRefuel). The
// identities, the fleet and the content, whose scenario they run on, must outlive the world. Throws
// std::invalid_argument for a content set with no scenario selected.
void RegisterCommands(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    Travel::Fleet &fleet, const Content::ContentSet &content);
} // namespace Manifest::Company
