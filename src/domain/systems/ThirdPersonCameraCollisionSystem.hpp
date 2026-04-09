#pragma once

#include "../../ecs/Registry.hpp"
#include "../../math/MathOps.hpp"
#include "../../math/Vec3.hpp"
#include "../../physics/CollisionFilter.hpp"
#include "../../physics/CollisionLayers.hpp"
#include "../../physics/Raycast.hpp"
#include "../../physics/SpatialGrid.hpp"
#include "../../systems/RaycastSystem.hpp"
#include "../../utils/TerrainHeightField.hpp"

#include "../../components/BoxColliderComponent.hpp"
#include "../../components/CapsuleColliderComponent.hpp"
#include "../../components/ColliderFilterComponent.hpp"
#include "../../components/RigidBodyComponent.hpp"
#include "../../components/SphereColliderComponent.hpp"
#include "../../components/TransformComponent.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

/// Pulls the third-person orbit camera toward the pivot when a ray hits static or dynamic colliders,
/// clamps height above terrain, and uses parallel side rays so the rig can slip past shallow obstructions.
class ThirdPersonCameraCollisionSystem {
public:
    struct Settings {
        /// Sphere around the eye: pull contact back so the near plane does not intersect geometry.
        float cameraCollisionRadius = 0.24f;
        /// Shortest allowed distance from pivot to eye after collision resolution.
        float minOrbitDistance = 0.42f;
        /// Eye Y must stay at least this far above sampled terrain.
        float groundClearance = 0.38f;
        /// Lateral offset (m) for side probe rays (0 disables extra probes).
        float sideProbeOffset = 0.1f;
        /// Added after `limitDist - margin` so the camera stays farther out (less aggressive pull-in).
        float obstructionRelaxMeters = 0.55f;
        /// Bits on collider `categoryBits` that block the camera (use `MaskAllButPlayer` to skip character capsules).
        uint32_t obstructionLayerMask = CollisionLayer::MaskAllButPlayer;
        /// If false, only static bodies (invMass == 0) block the camera (dynamic props pass through).
        bool blockDynamicColliders = true;
    };

    /// `desiredEye` is the unobstructed orbit position. Writes collision- and terrain-adjusted position to `outEye`.
    /// `ignoreEntity` is usually the followed character (player) so the rig does not hit its own capsule.
    static void resolve(
        Registry& registry,
        SpatialGrid& grid,
        TerrainHeightField* terrain,
        Entity ignoreEntity,
        const Vec3& pivot,
        const Vec3& desiredEye,
        const Settings& settings,
        Vec3& outEye)
    {
        outEye = desiredEye;

        Vec3 to{
            desiredEye.x - pivot.x,
            desiredEye.y - pivot.y,
            desiredEye.z - pivot.z};
        const float dist = length(to);
        if (dist < 1e-5f)
            return;

        Vec3 dir{to.x / dist, to.y / dist, to.z / dist};

        Vec3 right{0.f, 1.f, 0.f};
        Vec3 lateral = cross(right, dir);
        if (lengthSquared(lateral) < 1e-10f)
            lateral = {1.f, 0.f, 0.f};
        else
            lateral = normalize(lateral);

        const float off = settings.sideProbeOffset;
        const Vec3 probes[3] = {
            {0.f, 0.f, 0.f},
            {lateral.x * off, lateral.y * off, lateral.z * off},
            {-lateral.x * off, -lateral.y * off, -lateral.z * off},
        };

        float limitDist = dist;
        for (const Vec3& o : probes) {
            const Vec3 p0{pivot.x + o.x, pivot.y + o.y, pivot.z + o.z};
            const Vec3 p1{desiredEye.x + o.x, desiredEye.y + o.y, desiredEye.z + o.z};
            const float segLen = raycastSegmentMaxT(registry, grid, ignoreEntity, settings, p0, p1);
            limitDist = std::min(limitDist, segLen);
        }

        const float margin = settings.cameraCollisionRadius + 0.05f;
        if (limitDist < dist - 1e-4f) {
            float newDist = std::max(settings.minOrbitDistance, limitDist - margin + settings.obstructionRelaxMeters);
            newDist = std::min(newDist, dist);
            outEye = {
                pivot.x + dir.x * newDist,
                pivot.y + dir.y * newDist,
                pivot.z + dir.z * newDist};
        }

        if (terrain && !terrain->empty()) {
            float gh = 0.f;
            if (terrain->trySampleHeight(outEye.x, outEye.z, gh)) {
                const float minY = gh + settings.groundClearance;
                if (outEye.y < minY)
                    outEye.y = minY;
            }
        }
    }

private:
    /// Returns the distance along [p0→p1] to the first collider hit, or full segment length if none.
    static float raycastSegmentMaxT(
        Registry& registry,
        SpatialGrid& grid,
        Entity ignoreEntity,
        const Settings& settings,
        const Vec3& p0,
        const Vec3& p1)
    {
        Vec3 seg{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        const float maxLen = length(seg);
        if (maxLen < 1e-6f)
            return 0.f;
        const Vec3 d{seg.x / maxLen, seg.y / maxLen, seg.z / maxLen};

        std::vector<Entity> broad;
        grid.querySegment(p0, p1, broad);

        float bestT = maxLen + 1.f;
        for (Entity tgt : broad) {
            if (tgt == ignoreEntity || tgt == INVALID_ENTITY)
                continue;
            if (!registry.hasComponent<TransformComponent>(tgt))
                continue;
            if (registry.hasComponent<RigidBodyComponent>(tgt)) {
                const auto& rb = registry.getComponent<RigidBodyComponent>(tgt);
                if (!settings.blockDynamicColliders && rb.invMass > 0.f)
                    continue;
            }
            const uint32_t cat = raycast_detail::colliderCategory(registry, tgt);
            if ((cat & CollisionLayer::Player) != 0u)
                continue;
            if (!raycastLayerTest(cat, settings.obstructionLayerMask))
                continue;
            if (!registry.hasComponent<BoxColliderComponent>(tgt) && !registry.hasComponent<SphereColliderComponent>(tgt) &&
                !registry.hasComponent<CapsuleColliderComponent>(tgt))
                continue;

            RaycastHitData h{};
            if (!raycast_detail::testRayAgainstEntity(registry, tgt, p0, d, maxLen, h))
                continue;
            if (h.t >= 0.f && h.t < bestT)
                bestT = h.t;
        }

        if (bestT <= maxLen + 0.5f)
            return std::max(0.f, bestT);
        return maxLen;
    }
};
