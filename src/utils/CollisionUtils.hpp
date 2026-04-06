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

/// Minimum translation to apply to A so it no longer overlaps B (B treated as fixed). Zero if separated.
inline Vec3 mtvAabbSeparateAFromB(
    const Vec3& minA, const Vec3& maxA,
    const Vec3& minB, const Vec3& maxB)
{
    const Vec3 cA = (minA + maxA) * 0.5f;
    const Vec3 cB = (minB + maxB) * 0.5f;

    const float overlapX = std::min(maxA.x, maxB.x) - std::max(minA.x, minB.x);
    const float overlapY = std::min(maxA.y, maxB.y) - std::max(minA.y, minB.y);
    const float overlapZ = std::min(maxA.z, maxB.z) - std::max(minA.z, minB.z);
    if (overlapX <= 0.f || overlapY <= 0.f || overlapZ <= 0.f)
        return Vec3{0.f, 0.f, 0.f};

    if (overlapX < overlapY && overlapX < overlapZ) {
        const float sx = (cA.x < cB.x) ? -1.f : 1.f;
        return Vec3{sx * overlapX, 0.f, 0.f};
    }
    if (overlapY < overlapZ) {
        const float sy = (cA.y < cB.y) ? -1.f : 1.f;
        return Vec3{0.f, sy * overlapY, 0.f};
    }
    const float sz = (cA.z < cB.z) ? -1.f : 1.f;
    return Vec3{0.f, 0.f, sz * overlapZ};
}

/// Push sphere center out of AABB B. Returns zero if no penetration.
inline Vec3 mtvSphereSeparateFromAabb(const Vec3& center, float radius, const Vec3& minB, const Vec3& maxB)
{
    const float cx = std::max(minB.x, std::min(maxB.x, center.x));
    const float cy = std::max(minB.y, std::min(maxB.y, center.y));
    const float cz = std::max(minB.z, std::min(maxB.z, center.z));
    const Vec3  closest{cx, cy, cz};
    Vec3        delta = center - closest;
    const float d2    = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    const float r2    = radius * radius;
    if (d2 >= r2)
        return Vec3{0.f, 0.f, 0.f};
    const float d = std::sqrt(std::max(d2, 1e-12f));
    delta.x /= d;
    delta.y /= d;
    delta.z /= d;
    const float pen = radius - d;
    return Vec3{delta.x * pen, delta.y * pen, delta.z * pen};
}

} // namespace CollisionUtils
