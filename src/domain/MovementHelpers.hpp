#pragma once

#include "../math/MathOps.hpp"
#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"
#include <cmath>

/// Y-up facing: +X / +Z planar direction → yaw about world +Y (matches `atan2(x, z)` forward on XZ).
inline Quat lookRotationYUp(const Vec3& direction)
{
    const Vec3 f{direction.x, 0.f, direction.z};
    if (lengthSquared(f) < 1e-12f)
        return {0.f, 0.f, 0.f, 1.f};
    const Vec3 n = normalize(f);
    const float yaw = std::atan2(n.x, n.z);
    return quatAxisAngle({0.f, 1.f, 0.f}, yaw);
}
