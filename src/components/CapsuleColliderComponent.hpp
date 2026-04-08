#pragma once
#include "../math/Vec3.hpp"

/// Y-axis aligned capsule: segment from (center - halfHeight * up) to (center + halfHeight * up), radius `radius`.
struct CapsuleColliderComponent
{
    float radius{};
    float halfHeight{}; // distance from capsule center to each sphere center along +Y
    Vec3 offset{};
    bool isTrigger = false;
};
