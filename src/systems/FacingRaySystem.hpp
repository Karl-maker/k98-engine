#pragma once

// =============================================================================
// FacingRaySystem
// -----------------------------------------------------------------------------
// For entities that have FacingRayDriverComponent + RayComponent + TransformComponent:
//   - Copies rotation from `rotationAlignEntity` (e.g. player) onto the ray entity.
//   - Sets ray origin from the ray entity’s transform position (populate transform via
//     AttachmentSystem + socket on parent first).
//   - Sets horizontal aim direction from `cameraEntity` toward the active look/lock
//     target (reads CameraComponent + transforms), then normalizes.
//   - Writes RayComponent fields from the driver (maxDistance, layerMask, radius, ignore).
//
// Registration:
//   registry.registerComponent<FacingRayDriverComponent>();
//   registry.registerComponent<RayComponent>();
//   registry.registerComponent<TransformComponent>();
//   registry.registerComponent<CameraComponent>(); // on camera entity
//
// Order (same frame):
//   SocketSystem → AttachmentSystem → FacingRaySystem → RaycastSystem
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/FacingRayDriverComponent.hpp"
#include "../components/RayComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../math/Vec3.hpp"

#include <cmath>

class FacingRaySystem {
public:
    void update(Registry& registry) const
    {
        auto entities = registry.getEntitiesWith<FacingRayDriverComponent, RayComponent, TransformComponent>();

        for (Entity e : entities)
        {
            auto& driver = registry.getComponent<FacingRayDriverComponent>(e);
            auto& ray    = registry.getComponent<RayComponent>(e);
            auto& rayTf  = registry.getComponent<TransformComponent>(e);

            if (driver.rotationAlignEntity != INVALID_ENTITY &&
                registry.hasComponent<TransformComponent>(driver.rotationAlignEntity))
            {
                rayTf.rotation =
                    registry.getComponent<TransformComponent>(driver.rotationAlignEntity).rotation;
            }

            if (driver.cameraEntity == INVALID_ENTITY ||
                !registry.hasComponent<CameraComponent>(driver.cameraEntity) ||
                !registry.hasComponent<TransformComponent>(driver.cameraEntity))
            {
                ray.origin    = rayTf.position;
                ray.direction = {0.0f, 0.0f, -1.0f};
                ray.maxDistance = driver.maxDistance;
                ray.layerMask   = driver.layerMask;
                ray.radius      = driver.sweepRadius;
                ray.ignoreEntity = driver.ignoreEntity;
                continue;
            }

            auto&       camComp = registry.getComponent<CameraComponent>(driver.cameraEntity);
            const auto& camTf = registry.getComponent<TransformComponent>(driver.cameraEntity);

            Entity lookTarget = INVALID_ENTITY;
            if (camComp.enableLockOn && camComp.lockOnTarget != INVALID_ENTITY)
                lookTarget = camComp.lockOnTarget;
            else if (camComp.enableLookAt && camComp.lookAtTarget != INVALID_ENTITY)
                lookTarget = camComp.lookAtTarget;

            Vec3 dir{0.0f, 0.0f, -1.0f};
            if (lookTarget != INVALID_ENTITY && registry.hasComponent<TransformComponent>(lookTarget))
            {
                const auto& targetTf = registry.getComponent<TransformComponent>(lookTarget);
                const float dx =
                    (targetTf.position.x + camComp.lookAtOffset.x) - camTf.position.x;
                const float dz =
                    (targetTf.position.z + camComp.lookAtOffset.z) - camTf.position.z;
                const float hLen = std::sqrt(dx * dx + dz * dz);
                if (hLen > 1.0e-5f)
                {
                    dir.x = dx / hLen;
                    dir.y = 0.0f;
                    dir.z = dz / hLen;
                }
            }

            ray.origin       = rayTf.position;
            ray.direction    = normalize(dir);
            ray.maxDistance  = driver.maxDistance;
            ray.ignoreEntity = driver.ignoreEntity;
            ray.layerMask    = driver.layerMask;
            ray.radius       = driver.sweepRadius;
        }
    }
};
