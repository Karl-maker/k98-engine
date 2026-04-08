#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include <cstdint>

/// Optional per-entity ray query (e.g. bone attachment for origin). Filled each frame by `RaycastSystem`.
struct RaycastComponent
{
    static constexpr int kMaxConeDebugRays = 48;

    /// Primary axis in local space (bone + localOffset); normalized by the system. Cone opens around this axis.
    Vec3 localDirection{0.f, 0.f, -1.f};
    float maxDistance = 30.f;
    /// Hit colliders with `(categoryBits & layerMask) != 0`.
    uint32_t layerMask = 0xFFFFFFFFu;
    /// Skip hits against this entity (e.g. player root when casting from head).
    Entity ignoreEntity = INVALID_ENTITY;
    bool debugDraw = true;

    /// If false, only **one** ray is cast along `localDirection` (ignore cone fields). If true, use the cone below.
    bool useCone = false;
    /// Half-angle (degrees) of the cone around `localDirection` (only if `useCone`). Ignored when `useCone` is false.
    float coneHalfAngleDeg = 14.f;
    /// Rings from axis toward the cone edge. Total directions ≈ `1 + coneRings * coneSegments` (capped by `kMaxConeDebugRays`).
    int coneRings = 2;
    /// Samples per ring around the axis.
    int coneSegments = 10;

    bool hasHit = false;
    float hitDistance = 0.f;
    Vec3 hitPoint{};
    Vec3 hitNormal{};
    Entity hitEntity = INVALID_ENTITY;

    /// Central axis in world space after attachment (cone center direction).
    Vec3 lastWorldOrigin{};
    Vec3 lastWorldDirection{};
    /// Closest hit along the cone, or central ray miss endpoint.
    Vec3 lastRayEnd{};

    /// Debug: one segment per sub-ray (origin → end along max distance or hit for that sample).
    int debugRayCount = 0;
    Vec3 debugRayEnd[kMaxConeDebugRays]{};
};
