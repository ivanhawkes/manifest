#pragma once

#include <jettison/sim/SimWorld.h>

#include <manifest/content/Content.h>
#include <manifest/world/Identities.h>

namespace Manifest::World
{
// Registers the hosted components, in the order that is the save's, and adds the Identities
// service, which the world owns and the returned reference reaches for the world's lifetime.
// RegisterTypes must have run.
Identities &RegisterWorld(Jettison::Sim::SimWorld &world);

// The selected scenario's organisations, then its regions, each region followed by its public and
// household accounts, its households' goods, and its employer with its account, then its venues, each a
// broker holding the venue's stock, the broker's account with the authored treasury and the venue's
// empty account, in the scenario's order, so the same content creates the same entities. Throws
// std::invalid_argument for a content set with no scenario selected, or a world that already holds any
// of them.
void CreateInitialState(
    Jettison::Sim::SimWorld &world, Identities &identities, const Content::ContentSet &content);
} // namespace Manifest::World
