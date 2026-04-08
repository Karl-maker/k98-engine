#pragma once

#include "../components/RigidBodyComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../math/MathOps.hpp"
#include "../math/Vec3.hpp"
#include <algorithm>
#include <cmath>

/// Axis-aligned boxes in world space. `normal` points from the first box (a) toward the second (b).
/// Uses SAT: separation along the axis with the smallest overlap; ties prefer Y, then X, then Z for stability.
inline bool aabbVsAABB(
    const Vec3& aMin,
    const Vec3& aMax,
    const Vec3& bMin,
    const Vec3& bMax,
    Vec3& normal,
    float& penetration)
{
    if (aMax.x <= bMin.x || aMin.x >= bMax.x || aMax.y <= bMin.y || aMin.y >= bMax.y || aMax.z <= bMin.z || aMin.z >= bMax.z)
        return false;

    const float ox = std::min(aMax.x, bMax.x) - std::max(aMin.x, bMin.x);
    const float oy = std::min(aMax.y, bMax.y) - std::max(aMin.y, bMin.y);
    const float oz = std::min(aMax.z, bMax.z) - std::max(aMin.z, bMin.z);

    const float aMidX = (aMin.x + aMax.x) * 0.5f;
    const float aMidY = (aMin.y + aMax.y) * 0.5f;
    const float aMidZ = (aMin.z + aMax.z) * 0.5f;
    const float bMidX = (bMin.x + bMax.x) * 0.5f;
    const float bMidY = (bMin.y + bMax.y) * 0.5f;
    const float bMidZ = (bMin.z + bMax.z) * 0.5f;

    const float m = std::min(ox, std::min(oy, oz));
    penetration = m;
    constexpr float te = 1e-5f;
    int axis;
    if (std::fabs(oy - m) <= te)
        axis = 1;
    else if (std::fabs(ox - m) <= te)
        axis = 0;
    else
        axis = 2;

    switch (axis) {
    case 0:
        normal = (aMidX < bMidX) ? Vec3{1.f, 0.f, 0.f} : Vec3{-1.f, 0.f, 0.f};
        break;
    case 1:
        normal = (aMidY < bMidY) ? Vec3{0.f, 1.f, 0.f} : Vec3{0.f, -1.f, 0.f};
        break;
    default:
        normal = (aMidZ < bMidZ) ? Vec3{0.f, 0.f, 1.f} : Vec3{0.f, 0.f, -1.f};
        break;
    }
    return true;
}

inline bool sphereVsSphere(
    const Vec3& aPos,
    float aR,
    const Vec3& bPos,
    float bR,
    Vec3& normal,
    float& penetration)
{
    const Vec3 diff{bPos.x - aPos.x, bPos.y - aPos.y, bPos.z - aPos.z};
    const float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    const float r = aR + bR;
    if (distSq >= r * r || distSq < 1e-12f)
        return false;
    const float dist = std::sqrt(distSq);
    const float invDist = 1.f / dist;
    normal = {diff.x * invDist, diff.y * invDist, diff.z * invDist};
    penetration = r - dist;
    return true;
}

inline void resolveCollision(
    RigidBodyComponent& a,
    RigidBodyComponent& b,
    TransformComponent& ta,
    TransformComponent& tb,
    const Vec3& normal,
    float penetration)
{
    if (penetration <= 0.f)
        return;
    const float totalInvMass = a.invMass + b.invMass;
    if (totalInvMass <= 1e-12f)
        return;

    const float s = penetration / totalInvMass;
    const Vec3 correction{normal.x * s, normal.y * s, normal.z * s};

    ta.position.x -= correction.x * a.invMass;
    ta.position.y -= correction.y * a.invMass;
    ta.position.z -= correction.z * a.invMass;
    tb.position.x += correction.x * b.invMass;
    tb.position.y += correction.y * b.invMass;
    tb.position.z += correction.z * b.invMass;

    Vec3 relativeVel{b.velocity.x - a.velocity.x, b.velocity.y - a.velocity.y, b.velocity.z - a.velocity.z};
    const float velAlongNormal = dot(relativeVel, normal);
    if (velAlongNormal > 0.f)
        return;

    // Near-resting contacts: no restitution to avoid jitter when equal-mass pairs settle or stack.
    constexpr float restitutionFull = 0.05f;
    constexpr float restVelocityThreshold = 2.f;
    const float restitution =
        (std::fabs(velAlongNormal) < restVelocityThreshold) ? 0.f : restitutionFull;
    float j = -(1.f + restitution) * velAlongNormal;
    j /= totalInvMass;
    const Vec3 impulse{normal.x * j, normal.y * j, normal.z * j};

    a.velocity.x -= impulse.x * a.invMass;
    a.velocity.y -= impulse.y * a.invMass;
    a.velocity.z -= impulse.z * a.invMass;
    b.velocity.x += impulse.x * b.invMass;
    b.velocity.y += impulse.y * b.invMass;
    b.velocity.z += impulse.z * b.invMass;

    const float mu = std::min(a.friction, b.friction);
    if (mu > 1e-6f) {
        const float k = std::clamp(mu * 0.52f, 0.f, 0.92f);
        auto dampTangential = [&](RigidBodyComponent& rb) {
            if (rb.invMass <= 0.f)
                return;
            const float vn = dot(rb.velocity, normal);
            Vec3 vt{
                rb.velocity.x - normal.x * vn,
                rb.velocity.y - normal.y * vn,
                rb.velocity.z - normal.z * vn};
            rb.velocity.x = normal.x * vn + vt.x * (1.f - k);
            rb.velocity.y = normal.y * vn + vt.y * (1.f - k);
            rb.velocity.z = normal.z * vn + vt.z * (1.f - k);
        };
        dampTangential(a);
        dampTangential(b);
    }
}

inline float distPointToAABBSq(const Vec3& p, const Vec3& mn, const Vec3& mx, Vec3& closestOnBox)
{
    closestOnBox.x = std::clamp(p.x, mn.x, mx.x);
    closestOnBox.y = std::clamp(p.y, mn.y, mx.y);
    closestOnBox.z = std::clamp(p.z, mn.z, mx.z);
    const float dx = p.x - closestOnBox.x;
    const float dy = p.y - closestOnBox.y;
    const float dz = p.z - closestOnBox.z;
    return dx * dx + dy * dy + dz * dz;
}

/// Minimum squared distance between segment [a,b] and axis-aligned box; closest points on segment and box.
inline void closestPointsSegmentAABB(
    const Vec3& a,
    const Vec3& b,
    const Vec3& mn,
    const Vec3& mx,
    Vec3& closestSeg,
    Vec3& closestBox,
    float& distSq)
{
    float lo = 0.f;
    float hi = 1.f;
    for (int i = 0; i < 40; ++i) {
        const float m1 = lo + (hi - lo) * (1.f / 3.f);
        const float m2 = lo + (hi - lo) * (2.f / 3.f);
        Vec3 p1 = vec3Lerp(a, b, m1);
        Vec3 p2 = vec3Lerp(a, b, m2);
        Vec3 q1, q2;
        const float d1 = distPointToAABBSq(p1, mn, mx, q1);
        const float d2 = distPointToAABBSq(p2, mn, mx, q2);
        if (d1 < d2)
            hi = m2;
        else
            lo = m1;
    }
    const float t = (lo + hi) * 0.5f;
    closestSeg = vec3Lerp(a, b, t);
    distSq = distPointToAABBSq(closestSeg, mn, mx, closestBox);
}

/// Y-up capsule vs axis-aligned box. On hit, `normal` points from capsule toward box (first body toward second).
inline bool capsuleVsAABB(
    const Vec3& capCenter,
    float radius,
    float halfHeight,
    const Vec3& bMin,
    const Vec3& bMax,
    Vec3& normal,
    float& penetration)
{
    const Vec3 p0{capCenter.x, capCenter.y - halfHeight, capCenter.z};
    const Vec3 p1{capCenter.x, capCenter.y + halfHeight, capCenter.z};
    Vec3 closestSeg;
    Vec3 closestBox;
    float distSq = 0.f;
    closestPointsSegmentAABB(p0, p1, bMin, bMax, closestSeg, closestBox, distSq);
    const float dist = std::sqrt(distSq);
    if (dist >= radius)
        return false;
    penetration = radius - dist;
    if (dist > 1e-7f) {
        normal = normalize(
            {closestBox.x - closestSeg.x, closestBox.y - closestSeg.y, closestBox.z - closestSeg.z});
    } else {
        const Vec3 boxCenter{
            (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f, (bMin.z + bMax.z) * 0.5f};
        normal = normalize(
            {boxCenter.x - capCenter.x, boxCenter.y - capCenter.y, boxCenter.z - capCenter.z});
        if (lengthSquared(normal) < 1e-12f)
            normal = {0.f, 1.f, 0.f};
    }
    return true;
}
