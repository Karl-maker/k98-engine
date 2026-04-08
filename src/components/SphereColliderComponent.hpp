#pragma once

#include "math/Vec3.hpp"

struct SphereColliderComponent
{
    float radius;
    Vec3 offset;
    bool isTrigger = false;
};