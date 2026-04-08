#pragma once

#include "math/Vec3.hpp"

struct CapsuleColliderComponent
{
    float radius;
    float height;
    Vec3 offset;
    bool isTrigger = false;
};