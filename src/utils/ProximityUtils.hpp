#pragma once

#include <cmath>

#include "../math/Vec3.hpp"

namespace ProximityUtils {

inline float distanceSquared(const Vec3& a, const Vec3& b)
{
    Vec3 d = a - b;
    return lengthSquared(d);
}

inline float distance(const Vec3& a, const Vec3& b)
{
    return std::sqrt(distanceSquared(a, b));
}

inline bool isWithinRadius(const Vec3& a, const Vec3& b, float radius)
{
    const float r2 = radius * radius;
    return distanceSquared(a, b) <= r2;
}

} // namespace ProximityUtils
