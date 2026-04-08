#pragma once

#include "../ecs/Registry.hpp"
#include "../components/LocomotorControlComponent.hpp"
#include "../components/MovementComponent.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"

#include <cmath>

// -----------------------------------------------------------------------------
// Maps LocomotorControlComponent (-1..1 axes) + camera facing into
// MovementComponent::desiredWorldVelocityXZ at walkSpeed.
// -----------------------------------------------------------------------------

class LocomotorIntentSystem {
public:
    static void update(Registry& registry, Entity player, Entity camera)
    {
        if (!registry.hasComponent<LocomotorControlComponent>(player) ||
            !registry.hasComponent<MovementComponent>(player))
            return;

        auto& ctrl = registry.getComponent<LocomotorControlComponent>(player);
        auto& mv   = registry.getComponent<MovementComponent>(player);

        float fx = 0.0f;
        float fz = -1.0f;
        float rx = 1.0f;
        float rz = 0.0f;

        if (registry.hasComponent<TransformComponent>(camera) &&
            registry.hasComponent<CameraComponent>(camera)) {
            auto& camComp = registry.getComponent<CameraComponent>(camera);
            auto& camTf   = registry.getComponent<TransformComponent>(camera);

            Entity lookTarget = player;
            if (camComp.enableLookAt && camComp.lookAtTarget != INVALID_ENTITY)
                lookTarget = camComp.lookAtTarget;

            if (lookTarget != INVALID_ENTITY && registry.hasComponent<TransformComponent>(lookTarget)) {
                const auto& tgtTf = registry.getComponent<TransformComponent>(lookTarget);
                const float dx =
                    (tgtTf.position.x + camComp.lookAtOffset.x) - camTf.position.x;
                const float dz =
                    (tgtTf.position.z + camComp.lookAtOffset.z) - camTf.position.z;
                const float hDist = std::sqrt(dx * dx + dz * dz);
                if (hDist > 1.0e-5f) {
                    fx = dx / hDist;
                    fz = dz / hDist;
                } else {
                    const float yaw = camComp.currentYaw;
                    fx = -std::sin(yaw);
                    fz = -std::cos(yaw);
                }
                rx = fz;
                rz = -fx;
            }
        }

        float mx = ctrl.right;
        float mz = -ctrl.forward;
        const float mLen = std::sqrt(mx * mx + mz * mz);
        if (mLen > 1.0e-5f) {
            mx /= mLen;
            mz /= mLen;
        }

        const float fwdIn = -mz;
        const float vx    = (mx * rx + fwdIn * fx) * mv.walkSpeed;
        const float vz    = (mx * rz + fwdIn * fz) * mv.walkSpeed;

        mv.desiredWorldVelocityXZ = {vx, 0.0f, vz};
    }
};
