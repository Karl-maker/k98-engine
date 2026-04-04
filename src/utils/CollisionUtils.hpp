#pragma once

#include "../math/Vec3.hpp"
#include "ProximityUtils.hpp"

namespace CollisionUtils {

inline bool sphereSphere(
    const Vec3& aPos, float aRadius,
    const Vec3& bPos, float bRadius)
{
    const float r = aRadius + bRadius;
    return ProximityUtils::distanceSquared(aPos, bPos) <= (r * r);
}

inline bool pointInSphere(
    const Vec3& point,
    const Vec3& center,
    float radius)
{
    return ProximityUtils::isWithinRadius(point, center, radius);
}

inline bool aabbAabb(
    const Vec3& minA, const Vec3& maxA,
    const Vec3& minB, const Vec3& maxB)
{
    return (
        minA.x <= maxB.x && maxA.x >= minB.x &&
        minA.y <= maxB.y && maxA.y >= minB.y &&
        minA.z <= maxB.z && maxA.z >= minB.z
    );
}

inline bool pointInAabb(
    const Vec3& p,
    const Vec3& min,
    const Vec3& max)
{
    return (
        p.x >= min.x && p.x <= max.x &&
        p.y >= min.y && p.y <= max.y &&
        p.z >= min.z && p.z <= max.z
    );
}

} // namespace CollisionUtils
