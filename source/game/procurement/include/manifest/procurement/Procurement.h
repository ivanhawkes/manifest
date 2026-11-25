#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <jettison/sim/SimWorld.h>

#include <manifest/command/Requests.h>
#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>

namespace Manifest::Procurement
{
// The players' command that tenders for a project's input (D-101), sent by the principal directing
// the region's governor (World::Organisation's principalId): the totes, at most what the project still
// needs of the item, beyond what it holds and its open and awarded tenders call for, a budget in whole
// credits, moved from the region's public account to the tender's own, and whole days of bid and
// delivery windows within procurement's bounds, at a venue serving the region. Refused, writing
// nothing, for a region, project or input the world does not have (UnknownTarget); a region no one
// governs, one the AI runs or another principal directs (NotAuthorised); a venue not serving it
// (WrongVenue); no totes, more than are still needed, a project past tendering or a window out of
// bounds (InvalidQuantity); a budget below a credit (InvalidPrice); a region with
// procurement.maxOpenTenders not yet closed (LimitReached); and a public account short of the budget
// (InsufficientFunds).
struct Publish
{
	std::string regionId;
	std::string projectId;
	std::string itemId;
	std::string venueId;
	std::uint64_t totes{0};
	std::int64_t budget{0};
	std::uint32_t bidDays{0};
	std::uint32_t deliveryDays{0};
};

// The players' command that cancels an open tender before its bid close, its budget returned to the
// region's public account and its bids dropped, by the principal Publish is sent by. Refused for a
// tender the world does not have (UnknownTarget), another's (NotAuthorised), and one closed or run at
// or past its bid close (Closed).
struct Cancel
{
	std::string tenderId;
};

// The players' command that places or replaces their company's sealed bid on an open tender (D-101):
// its total in whole credits for all the tender's totes by its delivery close, from 1 to the budget,
// with no goods or money reserved. A new bid gives revision 0, a replacement the bid's current
// revision, and each is accepted at its step and request order. Refused, writing nothing, for a tender
// or company the world does not have (UnknownTarget), another's company (NotAuthorised), a tender
// closed or run at or past its bid close (Closed), a total outside 1 to the budget (InvalidPrice), a
// revision not the bid's (StaleRevision), and a new bid on a tender with procurement.maxBids
// (LimitReached).
struct Bid
{
	std::string tenderId;
	std::string companyId;
	std::int64_t total{0};
	std::uint64_t revision{0};
};

// The players' command that withdraws their company's bid before the bid close. Refused as Bid is,
// and UnknownTarget for a company with no bid on the tender.
struct Withdraw
{
	std::string tenderId;
	std::string companyId;
};

// The players' command that delivers to their company's contract (D-101) from its ship docked at the
// tender's venue: the tender's item aboard, at most the totes still owed and the room its recipient's
// warehouse there has, unloaded straight into the project's Stock, or a relief tender's region's
// households', the rest staying aboard. Each delivery is paid what it adds to Owed from the tender's
// account; the last fulfils the contract, its bond returned. Refused, writing nothing, for a tender or
// ship the world does not have (UnknownTarget), another's ship (NotAuthorised), a tender not awarded
// or run at or past its delivery close (Closed), a ship not of the contract's company
// (NotAuthorised), not docked (NotDocked) or docked elsewhere (WrongVenue), none of the item aboard
// (InsufficientStock), and no room for a tote (CapacityExceeded).
struct Deliver
{
	std::string tenderId;
	std::string shipId;
};

// The players' command that abandons their company's contract, in default at once: accepted goods
// and payments stand, the tender's account's rest returns to the public account, the bond's share of
// the totes undelivered, rounded down, is forfeit to it and the rest returned. Refused for a tender
// the world does not have (UnknownTarget), one not awarded or whose contract has ended, or run at or
// past its delivery close (Closed), and another company's (NotAuthorised).
struct Abandon
{
	std::string tenderId;
};

// The players' command that appends a tuning project to a region's queue (D-110), sent by the principal
// directing its governor: behind every project queued there, its head at once when the region has
// none, so Tendering. A Cancelled one is taken up again, at the back, with what it was delivered.
// Refused, writing nothing, for a region or project the world or the tuning does not have
// (UnknownTarget), a region no one governs, one the AI runs or another directs (NotAuthorised), and
// a project queued there, under way or done (AlreadyQueued).
struct Queue
{
	std::string regionId;
	std::string projectId;
};

// The players' command that takes a Queued or Tendering project out of its region's queue (D-110),
// by the principal Queue is sent by: its open tenders cancelled, their bids dropped and budgets back
// in the public account, its awarded contracts running on into its Stock, and the project Cancelled,
// holding what was delivered; the region's next queued project is then its head. Refused, writing
// nothing, for a project the region does not have (UnknownTarget), another's (NotAuthorised), and one
// Working, Paused, Done or Cancelled (Closed).
struct CancelProject
{
	std::string regionId;
	std::string projectId;
};

// The players' command that sets a region's standing development policy (World::Development, D-110),
// by the principal Queue is sent by, its wait cleared: with tender on, the AI issuer tenders the
// region's head project's inputs as for an AI-run region, priced the same, while the budgets of its
// live tenders together stay within the limit, in whole credits; one that would pass it, or that the
// public account cannot fund, waits, the reason kept until an input is tendered. Off, the default, the
// player publishes by command. Refused for a region the world does not have (UnknownTarget), one the
// sender does not direct (NotAuthorised), and a limit below 0 (InvalidQuantity).
struct SetDevelopment
{
	std::string regionId;
	bool tender{false};
	std::int64_t limit{0};
};

inline constexpr std::string_view kQueue = "Manifest.Procurement.Queue";
inline constexpr std::string_view kSetDevelopment = "Manifest.Procurement.SetDevelopment";
inline constexpr std::string_view kCancelProject = "Manifest.Procurement.CancelProject";
inline constexpr std::string_view kPublish = "Manifest.Procurement.Publish";
inline constexpr std::string_view kCancel = "Manifest.Procurement.Cancel";
inline constexpr std::string_view kBid = "Manifest.Procurement.Bid";
inline constexpr std::string_view kWithdraw = "Manifest.Procurement.Withdraw";
inline constexpr std::string_view kDeliver = "Manifest.Procurement.Deliver";
inline constexpr std::string_view kAbandon = "Manifest.Procurement.Abandon";
inline constexpr std::uint32_t kVersion = 1;

// The tenders' system, by the name the step's table gives it (docs/governance.md, "Where it runs in the
// step"): after manifest.expiry and before kProjects. Each awarded tender whose delivery close it is,
// in ID order, ends in default, as Abandon ends it. Then each open tender whose bid close it is, in ID
// order, is awarded to its lowest bid, then the earliest accepted, then its company's ID, whose company
// can fund the bond, procurement's rate of the total rounded up, moved into the tender's BondAccount,
// the next bid's otherwise, so an earlier award's bond spends what a later one would need; the budget
// beyond the total returns to the public account and the delivery window runs from the step. With none
// it closes unawarded, its budget returned. Its bids are dropped either way, and each region keeps its
// last procurement.keptClosed closed tenders, the oldest closed dropped. Then, as the world starts, on
// each economic turn's last step, in a step a tender's bidding closed and in one a tender closed or its
// contract ended, the AI issuer: for each region whose governor the AI runs, or whose player's
// development policy tenders within its limit (SetDevelopment), in the scenario's order, at the first
// venue serving it, each input its head project still needs, in the project's order, with no tender
// open for it, is tendered, the totes still needed at Trade::TenderPrice times one and the project's
// priorityPremium (Trade::Budget), with procurement's AI windows; and on a turn's last step each relief
// Trade::Reliefs calls for of such a region with no relief tender open for its need. One the region
// cannot fund or bound is not.
inline constexpr std::string_view kTenders = "manifest.tenders";

// The projects' system, after kTenders and before manifest.economy, each region's World::Project in
// entity order. A Tendering one holding every input, its region knowing the project's skills and with
// an operational workshop, has its inputs consumed, all at once, and works from that step. One that
// is Working or Paused works the step while its region has an operational workshop, and is Paused for
// a step it has none. The step that makes its work's last it is Done, its recipe unlocked or its skill
// taught in its region's World::Knowledge once, which no facility's loss takes back, and its region's
// next Queued project, the lowest queued, is its head, Tendering. Queued and Cancelled ones wait.
inline constexpr std::string_view kProjects = "manifest.projects";

// The steps of whole days at the fixed step, rounded to the nearest, at least one.
std::uint64_t Steps(std::uint32_t days, double fixedStep);

// The ID of a tender: its region, its project or need, its item and the count Identities issued it.
std::string TenderId(
    std::string_view regionId, std::string_view purpose, std::string_view itemId, std::uint64_t count);

void RegisterTypes();

// Registers Queue, CancelProject, SetDevelopment, Publish, Cancel, Bid, Withdraw, Deliver and Abandon,
// adds kTenders and kProjects, after Trade::Register has added the systems they run among, and gives
// the identities what each project needs (World::Identities::SetProjects). The content must outlive the
// world.
void Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);
} // namespace Manifest::Procurement
