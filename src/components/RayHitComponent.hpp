#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// RaycastHitComponent — last raycast result for an entity with RayComponent.
// Filled by RaycastSystem each update (cleared then written).
//
// Register: registry.registerComponent<RaycastHitComponent>();
// -----------------------------------------------------------------------------

struct RaycastHitComponent
{
    bool  hit{false};
    Entity entity{INVALID_ENTITY};

    Vec3  point{};
    float distance{0.0f};
};
