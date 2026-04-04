#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"

struct RayComponent
{
    Vec3   origin{};
    Vec3   direction{}; // normalized
    float  maxDistance{100.0f};
    Entity ignoreEntity{INVALID_ENTITY};
};
