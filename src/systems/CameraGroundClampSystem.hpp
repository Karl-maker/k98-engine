#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../systems/GravitySystem.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/TerrainHeightField.hpp"

// -----------------------------------------------------------------------------
// After CameraSystem: raises camera Transform Y so the eye stays above terrain +
// static solid tops at the camera’s xz (same grid + heightfield sampling as
// GravitySystem::sampleGroundHeightAtXZ).
// -----------------------------------------------------------------------------

class CameraGroundClampSystem {
public:
    void update(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain,
        float fallbackGroundY,
        uint32_t solidLayerMask) const
    {
        for (Entity e : registry.getEntitiesWith<CameraComponent, TransformComponent>()) {
            auto& cam = registry.getComponent<CameraComponent>(e);
            if (!cam.active || !cam.enableGroundHeightClamp)
                continue;

            auto& tf = registry.getComponent<TransformComponent>(e);

            const float groundY = GravitySystem::sampleGroundHeightAtXZ(
                registry, grid, terrain, tf.position.x, tf.position.z, fallbackGroundY, solidLayerMask);
            const float minEyeY = groundY + cam.groundClearance;
            if (tf.position.y < minEyeY)
                tf.position.y = minEyeY;
        }
    }
};
