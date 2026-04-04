#pragma once

#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// Velocity — linear velocity (units/sec), usually integrated into Position each
// tick in gameplay code: pos += velocity * dt; then PositionToTransformSystem.
//
// Register: registry.registerComponent<Velocity>();
// Typical pair: Position + Velocity on the same entity.
// -----------------------------------------------------------------------------

struct Velocity : Vec3
{
    using Vec3::Vec3;
};
