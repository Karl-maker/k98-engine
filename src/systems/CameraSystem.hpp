#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
            // DETERMINE TARGET ENTITY
            // =========================
            Entity targetEntity = INVALID_ENTITY;

            if (cam.enableLockOn && cam.lockOnTarget != INVALID_ENTITY)
                targetEntity = cam.lockOnTarget;
            else if (cam.enableLookAt)
                targetEntity = cam.lookAtTarget;

            // =========================
            // STEP 1: COMPUTE TARGET POSITION
            // =========================
            Vec3 targetPos = transform.position; // fallback

            // ---- ORBIT MODE ----
            if (cam.enableOrbit &&
                targetEntity != INVALID_ENTITY &&
                registry.hasComponent<TransformComponent>(targetEntity))
            {
                auto& target = registry.getComponent<TransformComponent>(targetEntity);

                // =========================
                // UPDATE RAW INPUT (NO dt)
                // =========================
                // =========================
                // INPUT SMOOTHING (NEW)
                // =========================
                float inputAccel = 8.0f;

                // smooth input (this is the key)
                cam.inputVelocityX += (cam.inputDeltaX - cam.inputVelocityX) * std::clamp(inputAccel * dt, 0.0f, 1.0f);

                // apply smoothed input
                cam.orbitYaw += cam.inputVelocityX * cam.orbitSensitivity;
                cam.orbitPitch += cam.inputDeltaY * cam.orbitSensitivity;

                // clamp pitch
                if (cam.orbitPitch < cam.minPitch) cam.orbitPitch = cam.minPitch;
                if (cam.orbitPitch > cam.maxPitch) cam.orbitPitch = cam.maxPitch;

                // =========================
                // SMOOTH ROTATION (NEW)
                // =========================
                float tRot = std::clamp(cam.followLerp * dt, 0.0f, 1.0f);

                // initialize on first frame (prevents snapping)
                if (cam.currentYaw == 0.0f && cam.currentPitch == 0.0f)
                {
                    cam.currentYaw = cam.orbitYaw;
                    cam.currentPitch = cam.orbitPitch;
                }

                cam.currentYaw   += (cam.orbitYaw   - cam.currentYaw)   * tRot;
                cam.currentPitch += (cam.orbitPitch - cam.currentPitch) * tRot;

                // =========================
                // USE SMOOTHED VALUES
                // =========================
                float cosPitch = std::cos(cam.currentPitch);
                float sinPitch = std::sin(cam.currentPitch);
                float cosYaw   = std::cos(cam.currentYaw);
                float sinYaw   = std::sin(cam.currentYaw);

                Vec3 offset{
                    cam.orbitDistance * cosPitch * sinYaw,
                    cam.orbitDistance * sinPitch,
                    cam.orbitDistance * cosPitch * cosYaw
                };

                targetPos = {
                    target.position.x + offset.x,
                    target.position.y + offset.y,
                    target.position.z + offset.z
                };

                // consume input
                cam.inputDeltaX = 0.0f;
                cam.inputDeltaY = 0.0f;
            }
            // ---- FOLLOW (SOCKET/ATTACHMENT) ----
            else if (cam.enableFollow &&
                     registry.hasComponent<WorldTransformComponent>(e))
            {
                auto& world = registry.getComponent<WorldTransformComponent>(e);

                targetPos = {
                    world.world.m[12],
                    world.world.m[13],
                    world.world.m[14]
                };
            }

            // =========================
            // STEP 2: APPLY POSITION FOLLOW (lerp or spring)
            // =========================
            if (cam.enableFollow || cam.enableOrbit)
            {
                if (cam.followPositionSpring)
                {
                    // Avoid spring blow-up on long frames / breakpoints
                    const float springDt = std::min(dt, 1.0f / 30.0f);

                    const float omega0 =
                        static_cast<float>(2.0 * M_PI) * std::max(0.01f, cam.followSpringFrequency);
                    const float zeta = std::max(0.01f, cam.followSpringDampingRatio);
                    const float twoZetaOmega = 2.0f * zeta * omega0;
                    const float omegaSq = omega0 * omega0;

                    float ex = targetPos.x - transform.position.x;
                    float ey = targetPos.y - transform.position.y;
                    float ez = targetPos.z - transform.position.z;

                    float ax = omegaSq * ex - twoZetaOmega * cam.followPositionVelocity.x;
                    float ay = omegaSq * ey - twoZetaOmega * cam.followPositionVelocity.y;
                    float az = omegaSq * ez - twoZetaOmega * cam.followPositionVelocity.z;

                    cam.followPositionVelocity.x += ax * springDt;
                    cam.followPositionVelocity.y += ay * springDt;
                    cam.followPositionVelocity.z += az * springDt;

                    transform.position.x += cam.followPositionVelocity.x * springDt;
                    transform.position.y += cam.followPositionVelocity.y * springDt;
                    transform.position.z += cam.followPositionVelocity.z * springDt;
                }
                else
                {
                    float t = std::clamp(cam.followPositionLerp * dt, 0.0f, 1.0f);

                    transform.position.x += (targetPos.x - transform.position.x) * t;
                    transform.position.y += (targetPos.y - transform.position.y) * t;
                    transform.position.z += (targetPos.z - transform.position.z) * t;
                }
            }
            else
            {
                // fallback: snap (old behavior)
                transform.position = targetPos;
            }

            // =========================
            // STEP 3: LOOK AT TARGET
            // =========================
            if (targetEntity != INVALID_ENTITY &&
                registry.hasComponent<TransformComponent>(targetEntity))
            {
                auto& target = registry.getComponent<TransformComponent>(targetEntity);

                const float dx = (target.position.x + cam.lookAtOffset.x) - transform.position.x;
                const float dy = (target.position.y + cam.lookAtOffset.y) - transform.position.y;
                const float dz = (target.position.z + cam.lookAtOffset.z) - transform.position.z;

                const float horizontalDist =
                    std::sqrt(dx * dx + dz * dz);
                const float yaw   = std::atan2(dx, dz);
                const float pitch = std::atan2(dy, std::max(horizontalDist, 1.0e-5f));

                const float halfYaw   = yaw * 0.5f;
                const float halfPitch = pitch * 0.5f;
                const float sy        = std::sin(halfYaw);
                const float cy        = std::cos(halfYaw);
                const float sp        = std::sin(halfPitch);
                const float cp        = std::cos(halfPitch);

                // q = q_yaw * q_pitch (yaw around world Y, then pitch around local X)
                transform.rotation.x = cy * sp;
                transform.rotation.y = sy * cp;
                transform.rotation.z = -sy * sp;
                transform.rotation.w = cy * cp;
            }

            // =========================
            // STEP 4: VIEW MATRIX
            // =========================
            cam.viewMatrix = Mat4::FromTR(transform.position, transform.rotation);
        }
    }
};