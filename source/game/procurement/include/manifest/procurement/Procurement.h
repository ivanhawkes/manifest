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
// needs of the item, beyond what it holds and its open tenders call for, a budget in whole credits,
// moved from the region's public account to the tender's own, and whole days of bid and delivery
// windows within procurement's bounds, at a venue serving the region. Refused, writing nothing, for a
// region, project or input the world does not have (UnknownTarget); a region no one governs, one the
// AI runs or another principal directs (NotAuthorised); a venue not serving it (WrongVenue); no totes,
// more than are still needed, a project past tendering or a window out of bounds (InvalidQuantity); a
// budget below a credit (InvalidPrice); a region with procurement.maxOpenTenders not yet closed
// (LimitReached); and a public account short of the budget (InsufficientFunds).
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
// region's public account, by the principal Publish is sent by. Refused for a tender the world does
// not have (UnknownTarget), another's (NotAuthorised), and one closed or run at or past its bid close
// (Closed).
struct Cancel
{
	std::string tenderId;
};

inline constexpr std::string_view kPublish = "Manifest.Procurement.Publish";
inline constexpr std::string_view kCancel = "Manifest.Procurement.Cancel";
inline constexpr std::uint32_t kVersion = 1;

// The tenders' system, by the name the step's table gives it (docs/stage-4-trading-plan.md, "Tender
// contract (W11)"): after manifest.expiry and before kProjects. Each open tender whose bid close it
// is, in ID order, closes with no award, its budget returned, and each region keeps its last
// procurement.keptClosed closed tenders, the oldest closed dropped. Then, as the world starts, on each
// economic turn's last step and in a step a tender closed, the AI issuer: for each region whose
// governor the AI runs, in the scenario's order, at the first venue serving it, each input its
// projects still need, in the project's order, with no tender open for it, is tendered, the totes
// still needed at Trade::TenderPrice times one and the project's priorityPremium (Trade::Budget), with
// procurement's AI windows; and on a turn's last step each relief Trade::Reliefs calls for of such a
// region with no relief tender open for its need. One the region cannot fund or bound is not.
inline constexpr std::string_view kTenders = "manifest.tenders";

// The projects' system, after kTenders and before manifest.economy, each region's World::Project in
// entity order. A Tendering one holding every input, its region knowing the project's skills and with
// an operational workshop, has its inputs consumed, all at once, and works from that step. One that
// is Working or Paused works the step while its region has an operational workshop, and is Paused for
// a step it has none. The step that makes its work's last it is Done, its recipe unlocked in its
// region's World::Knowledge once, which no facility's loss takes back.
inline constexpr std::string_view kProjects = "manifest.projects";

// The steps of whole days at the fixed step, rounded to the nearest, at least one.
std::uint64_t Steps(std::uint32_t days, double fixedStep);

// The ID of a tender: its region, its project or need, its item and the count Identities issued it.
std::string TenderId(
    std::string_view regionId, std::string_view purpose, std::string_view itemId, std::uint64_t count);

void RegisterTypes();

// Registers Publish and Cancel, adds kTenders and kProjects, after Trade::Register has added the
// systems they run among, and gives the identities what each project needs
// (World::Identities::SetProjects). The content must outlive the world.
void Register(
    Jettison::Sim::SimWorld &world, Command::Requests &requests, World::Identities &identities,
    const Content::ContentSet &content);
} // namespace Manifest::Procurement
