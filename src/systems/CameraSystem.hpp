#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include <cmath>

class CameraSystem {
public:
    void update(Registry& registry, float dt)
    {
        auto entities = registry.getEntitiesWith<CameraComponent, TransformComponent>();

        for (auto e : entities)
        {
            auto& cam = registry.getComponent<CameraComponent>(e);
            if (!cam.active) continue;

            auto& transform = registry.getComponent<TransformComponent>(e);

            // =========================
            // FOLLOW (SMOOTH)
            // =========================
            if (cam.enableFollow &&
                registry.hasComponent<WorldTransformComponent>(e))
            {
                auto& world = registry.getComponent<WorldTransformComponent>(e);

                Vec3 targetPos{
                    world.world.m[12],
                    world.world.m[13],
                    world.world.m[14]
                };

                float t = 1.0f - std::exp(-cam.followLerp * dt);

                transform.position.x += (targetPos.x - transform.position.x) * t;
                transform.position.y += (targetPos.y - transform.position.y) * t;
                transform.position.z += (targetPos.z - transform.position.z) * t;
            }

            // =========================
            // LOOK AT (YAW ONLY)
            // =========================
            if (cam.enableLookAt &&
                cam.lookAtTarget != INVALID_ENTITY &&
                registry.hasComponent<TransformComponent>(cam.lookAtTarget))
            {
                auto& target = registry.getComponent<TransformComponent>(cam.lookAtTarget);

                float dx = (target.position.x + cam.lookAtOffset.x) - transform.position.x;
                float dz = (target.position.z + cam.lookAtOffset.z) - transform.position.z;

                float yaw = std::atan2(dx, dz);

                // 🔥 MANUAL QUATERNION (Y-AXIS ROTATION)
                float halfYaw = yaw * 0.5f;

                transform.rotation.x = 0.0f;
                transform.rotation.y = std::sin(halfYaw);
                transform.rotation.z = 0.0f;
                transform.rotation.w = std::cos(halfYaw);
            }

            // =========================
            // VIEW MATRIX
            // =========================
            cam.viewMatrix = Mat4::FromTR(transform.position, transform.rotation);
        }
    }
};