#pragma once

#include <cstdint>
#include <string_view>

#include <jettison/sim/SimWorld.h>

#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>

namespace Manifest::Procurement
{
// The system, by the name the step's table gives it (docs/stage-4-trading-plan.md, "Tender contract
// (W11)"): after manifest.expiry and before manifest.economy, each region's World::Project in entity
// order. A Tendering one holding every input, its region knowing the project's skills and with an
// operational workshop, has its inputs consumed, all at once, and works from that step. One that is
// Working or Paused works the step while its region has an operational workshop, and is Paused for
// a step it has none. The step that makes its work's last it is Done, its recipe unlocked in its
// region's World::Knowledge once, which no facility's loss takes back.
inline constexpr std::string_view kProjects = "manifest.projects";

// The steps of a project's work: its days at the fixed step, rounded to the nearest, at least one.
std::uint64_t WorkSteps(std::uint32_t workDays, double fixedStep);

// Adds kProjects, after Trade::Register has added the systems it runs among, and gives the identities
// what each project needs (World::Identities::SetProjects).
void Register(
    Jettison::Sim::SimWorld &world, World::Identities &identities, const Content::ContentSet &content);
} // namespace Manifest::Procurement
