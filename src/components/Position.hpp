#pragma once

#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// Position — gameplay / physics translation (often integrated from Velocity).
// `previous` is the position at the **start** of the current fixed tick (copy taken
// before integration). Use it for safe reads when another system may write `*this`
// later in the same tick, or for render interpolation with the last tick’s end.
//
// Sync to TransformComponent via PositionToTransformSystem when transforms are
// authoritative for rendering and attachments.
//
// Register: registry.registerComponent<Position>();
// -----------------------------------------------------------------------------

struct Position : Vec3
{
    using Vec3::Vec3;

    Vec3 previous{};

    /// Copy current x,y,z into previous — call once per tick **before** physics/integration.
    void syncPreviousFromCurrent() { previous = Vec3{x, y, z}; }
};
