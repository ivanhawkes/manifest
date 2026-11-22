#pragma once

#include <manifest/space/StarSystem.h>

namespace Jettison::Physics
{
class PhysicsWorld;
}

namespace Manifest::Space
{
// Defines a star system as the physics frame of its name (Jettison::Sim::FrameId,
// Jettison::Physics::FrameModel): its bodies are what rails and local frames name
// (Jettison::Physics::OnRails, Jettison::Physics::LocalFrame), each where its orbit has it and turned
// as it spins, or as a jump point faces (Space::BodyOrientation); and its gravity is each body with a
// gravitational parameter pulling as a point mass, and as one whose mass is all within the point's
// distance from its centre, inside its radius. The system has to be resolved (Space::Resolve), as
// reading one does. Throws std::invalid_argument for one that is not, or has no name or bodies, or
// whose frame is already defined.
void DefineStarSystem(Jettison::Physics::PhysicsWorld &physics, Space::StarSystem system);
} // namespace Manifest::Space
