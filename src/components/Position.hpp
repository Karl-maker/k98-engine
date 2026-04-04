#pragma once

#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// Position — gameplay / physics translation (often integrated from Velocity).
// Sync to TransformComponent via PositionToTransformSystem when transforms are
// authoritative for rendering and attachments.
//
// Register: registry.registerComponent<Position>();
// -----------------------------------------------------------------------------

struct Position : Vec3
{
    using Vec3::Vec3;
};
