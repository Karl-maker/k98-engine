#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "../math/Vec3.hpp"

namespace RaycastUtils {
    
    inline bool sphereAabb(
        const Vec3& center,
        float radius,
        const Vec3& min,
        const Vec3& max)
    {
        float dx = std::max(min.x - center.x, 0.0f);
        dx = std::max(dx, center.x - max.x);

        float dy = std::max(min.y - center.y, 0.0f);
        dy = std::max(dy, center.y - max.y);

        float dz = std::max(min.z - center.z, 0.0f);
        dz = std::max(dz, center.z - max.z);

        float distSq = dx*dx + dy*dy + dz*dz;

        return distSq <= (radius * radius);
    }
        
    // Slab method: ray vs axis-aligned box. dir should be non-zero (caller normalizes).
    inline bool rayAabb(
        const Vec3& origin,
        const Vec3& dir,
        const Vec3& minB,
        const Vec3& maxB,
        float& tHit)
    {
        float tmin = 0.0f;
        float tmax = FLT_MAX;

        const float eps = 1.0e-8f;

        auto axis = [&](float o, float d, float mn, float mx)
        {
            if (std::fabs(d) < eps)
            {
                if (o < mn || o > mx)
                {
                    tmin = 1.0f;
                    tmax = -1.0f;
                }
                return;
            }
            float invD = 1.0f / d;
            float t0 = (mn - o) * invD;
            float t1 = (mx - o) * invD;
            if (t0 > t1)
            {
                std::swap(t0, t1);
            }
            tmin = std::max(tmin, t0);
            tmax = std::min(tmax, t1);
        };

        axis(origin.x, dir.x, minB.x, maxB.x);
        axis(origin.y, dir.y, minB.y, maxB.y);
        axis(origin.z, dir.z, minB.z, maxB.z);

        if (tmax < tmin || tmax < 0.0f)
        {
            return false;
        }

        tHit = (tmin >= 0.0f) ? tmin : tmax;
        return true;
    }

} // namespace RaycastUtils
