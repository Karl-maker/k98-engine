#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"

struct RaycastHitComponent
{
    bool  hit{false};
    Entity entity{INVALID_ENTITY};

    Vec3  point{};
    float distance{0.0f};
};
