#pragma once

#include "../ecs/Entity.hpp"
#include "../math/MathOps.hpp"
#include "../math/Vec3.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

struct RaycastHitData
{
    Entity entity = INVALID_ENTITY;
    float t = 0.f;
    Vec3 point{};
    Vec3 normal{};
};

/// Axis-aligned box in world space. `d` should be unit length; `maxT` is max ray length.
inline bool rayVsAABB(
    const Vec3& o,
    const Vec3& d,
    float maxT,
    const Vec3& bMin,
    const Vec3& bMax,
    float& tHit,
    Vec3& normalOut)
{
    constexpr float eps = 1e-8f;
    float tNear = 0.f;
    float tFar = maxT;

    auto slab = [&](float oa, float da, float mn, float mx) -> bool {
        if (std::fabs(da) < eps) {
            if (oa < mn || oa > mx)
                return false;
            return true;
        }
        const float inv = 1.f / da;
        float t0 = (mn - oa) * inv;
        float t1 = (mx - oa) * inv;
        if (t0 > t1)
            std::swap(t0, t1);
        if (t0 > tNear)
            tNear = t0;
        if (t1 < tFar)
            tFar = t1;
        return tNear <= tFar;
    };

    if (!slab(o.x, d.x, bMin.x, bMax.x))
        return false;
    if (!slab(o.y, d.y, bMin.y, bMax.y))
        return false;
    if (!slab(o.z, d.z, bMin.z, bMax.z))
        return false;

    float t = tNear;
    if (t < 0.f) {
        if (tFar < 0.f || tFar > maxT)
            return false;
        t = tFar;
    } else if (t > maxT)
        return false;

    tHit = t;
    const Vec3 p{o.x + d.x * t, o.y + d.y * t, o.z + d.z * t};
    constexpr float fe = 1e-3f;
    float best = 1e30f;
    normalOut = {0.f, 1.f, 0.f};
    auto face = [&](float dist, const Vec3& n) {
        if (dist < best) {
            best = dist;
            normalOut = n;
        }
    };
    face(std::fabs(p.x - bMin.x), {-1.f, 0.f, 0.f});
    face(std::fabs(p.x - bMax.x), {1.f, 0.f, 0.f});
    face(std::fabs(p.y - bMin.y), {0.f, -1.f, 0.f});
    face(std::fabs(p.y - bMax.y), {0.f, 1.f, 0.f});
    face(std::fabs(p.z - bMin.z), {0.f, 0.f, -1.f});
    face(std::fabs(p.z - bMax.z), {0.f, 0.f, 1.f});
    return true;
}

inline bool rayVsSphere(
    const Vec3& o,
    const Vec3& d,
    float maxT,
    const Vec3& center,
    float radius,
    float& tHit,
    Vec3& normalOut)
{
    const Vec3 oc{o.x - center.x, o.y - center.y, o.z - center.z};
    const float b = dot(d, oc);
    const float c = dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.f)
        return false;
    const float s = std::sqrt(disc);
    float t0 = -b - s;
    float t1 = -b + s;
    float t = (t0 > 1e-6f) ? t0 : t1;
    if (t < 1e-6f || t > maxT)
        return false;
    tHit = t;
    const Vec3 p{o.x + d.x * t, o.y + d.y * t, o.z + d.z * t};
    normalOut = normalize({p.x - center.x, p.y - center.y, p.z - center.z});
    return true;
}

/// Y-up capsule: segment from (cx, cy-h, cz) to (cx, cy+h, cz), radius `radius`.
inline bool rayVsCapsuleY(
    const Vec3& o,
    const Vec3& d,
    float maxT,
    const Vec3& capCenter,
    float radius,
    float halfHeight,
    float& tHit,
    Vec3& normalOut)
{
    const Vec3 p0{capCenter.x, capCenter.y - halfHeight, capCenter.z};
    const Vec3 p1{capCenter.x, capCenter.y + halfHeight, capCenter.z};

    float bestT = maxT + 1.f;
    Vec3 bestN{0.f, 1.f, 0.f};
    bool any = false;

    auto tryT = [&](float t, const Vec3& n) {
        if (t > 1e-6f && t < bestT && t <= maxT) {
            bestT = t;
            bestN = n;
            any = true;
        }
    };

    // Spherical caps (full spheres — conservative for rays outside)
    float ts;
    Vec3 ns;
    if (rayVsSphere(o, d, maxT, p0, radius, ts, ns))
        tryT(ts, ns);
    if (rayVsSphere(o, d, maxT, p1, radius, ts, ns))
        tryT(ts, ns);

    // Infinite Y-aligned cylinder through capCenter xz
    const float ox = o.x - capCenter.x;
    const float oz = o.z - capCenter.z;
    const float dx = d.x;
    const float dz = d.z;
    const float a = dx * dx + dz * dz;
    if (a > 1e-10f) {
        const float b = 2.f * (ox * dx + oz * dz);
        const float c = ox * ox + oz * oz - radius * radius;
        const float disc = b * b - 4.f * a * c;
        if (disc >= 0.f) {
            const float s = std::sqrt(disc);
            const float inv2a = 0.5f / a;
            float tA = (-b - s) * inv2a;
            float tB = (-b + s) * inv2a;
            for (float t : {tA, tB}) {
                if (t > 1e-6f && t <= maxT) {
                    const float y = o.y + t * d.y;
                    if (y >= p0.y && y <= p1.y) {
                        const float px = o.x + t * d.x;
                        const float pz = o.z + t * d.z;
                        Vec3 n{px - capCenter.x, 0.f, pz - capCenter.z};
                        n = normalize(n);
                        tryT(t, n);
                    }
                }
            }
        }
    }

    if (!any)
        return false;
    tHit = bestT;
    normalOut = bestN;
    return true;
}
