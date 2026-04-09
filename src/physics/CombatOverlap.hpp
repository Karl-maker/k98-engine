#pragma once

#include "../components/TransformComponent.hpp"
#include "../math/MathOps.hpp"
#include "../math/Vec3.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

/// Axis-aligned combat volumes (world space, no rotation). Used by hitboxes, hurtboxes, and trigger volumes.

enum class CombatShape : std::uint8_t {
    Box = 0,
    Sphere = 1,
    Capsule = 2,
};

inline void combatVolumeToAABB(
    const TransformComponent& t,
    const Vec3& offset,
    CombatShape shape,
    const Vec3& halfExtents,
    float radius,
    float capsuleHalfHeight,
    Vec3& outMin,
    Vec3& outMax)
{
    const Vec3 c{t.position.x + offset.x, t.position.y + offset.y, t.position.z + offset.z};
    switch (shape) {
    case CombatShape::Box:
        outMin = {c.x - halfExtents.x, c.y - halfExtents.y, c.z - halfExtents.z};
        outMax = {c.x + halfExtents.x, c.y + halfExtents.y, c.z + halfExtents.z};
        return;
    case CombatShape::Sphere:
        outMin = {c.x - radius, c.y - radius, c.z - radius};
        outMax = {c.x + radius, c.y + radius, c.z + radius};
        return;
    case CombatShape::Capsule:
    default: {
        const float hh = capsuleHalfHeight;
        const float r = radius;
        outMin = {c.x - r, c.y - hh - r, c.z - r};
        outMax = {c.x + r, c.y + hh + r, c.z + r};
        return;
    }
    }
}

inline bool aabbOverlapsAABB(const Vec3& aMin, const Vec3& aMax, const Vec3& bMin, const Vec3& bMax)
{
    return aMax.x > bMin.x && aMin.x < bMax.x && aMax.y > bMin.y && aMin.y < bMax.y && aMax.z > bMin.z && aMin.z < bMax.z;
}

inline bool sphereOverlapsAABB(const Vec3& center, float radius, const Vec3& bMin, const Vec3& bMax)
{
    const float cx = std::clamp(center.x, bMin.x, bMax.x);
    const float cy = std::clamp(center.y, bMin.y, bMax.y);
    const float cz = std::clamp(center.z, bMin.z, bMax.z);
    const float dx = center.x - cx;
    const float dy = center.y - cy;
    const float dz = center.z - cz;
    return dx * dx + dy * dy + dz * dz <= radius * radius;
}

inline bool spheresOverlap(const Vec3& a, float ra, const Vec3& b, float rb)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    const float rr = ra + rb;
    return dx * dx + dy * dy + dz * dz <= rr * rr;
}
