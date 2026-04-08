#pragma once
#include "../math/Vec3.hpp"

struct BoxColliderComponent
{
    Vec3 halfExtents;   // size from center
    Vec3 offset;        // local offset from entity origin
    bool isTrigger = false;
};