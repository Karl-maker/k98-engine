#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// RayComponent — world-space ray or thick sweep. `direction` must be normalized
// before RaycastSystem. Written each frame by gameplay or FacingRaySystem.
//
// Register: registry.registerComponent<RayComponent>();
// Pair: RaycastHitComponent on the same entity; run RaycastSystem after origins are final.
// -----------------------------------------------------------------------------

struct RayComponent
{
    Vec3   origin{};
    Vec3   direction{}; // normalized
    float  maxDistance{100.0f};
    Entity ignoreEntity{INVALID_ENTITY};
    uint32_t layerMask{0xFFFFFFFF}; // what layers this ray can hit
    float radius{0.0f};             // 0 = ray, >0 = sphere cast
};