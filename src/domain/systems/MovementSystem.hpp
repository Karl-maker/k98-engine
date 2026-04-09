#pragma once

#include "../../ecs/Registry.hpp"
#include "../../components/PlayerTagComponent.hpp"
#include "../../components/RigidBodyComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../components/MovementComponent.hpp"
#include "../MovementHelpers.hpp"
#include "../states/MovementStateEnums.hpp"
#include "../../math/MathOps.hpp"

#include <algorithm>
#include <cmath>

class MovementSystem
{
public:
    void update(Registry& registry, float dt)
    {
        auto entities = registry.getEntitiesWith<MovementComponent, RigidBodyComponent, TransformComponent>();

        for (auto e : entities) {
            auto& move = registry.getComponent<MovementComponent>(e);
            auto& body = registry.getComponent<RigidBodyComponent>(e);
            auto& transform = registry.getComponent<TransformComponent>(e);

            const float targetSpeed = getSpeed(move);
            const Vec3 flatDir{move.desiredDirection.x, 0.f, move.desiredDirection.z};
            const float flatLen = length(flatDir);
            Vec3 desiredHoriz{0.f, 0.f, 0.f};
            if (flatLen > 1e-5f) {
                const Vec3 n = {
                    flatDir.x / flatLen,
                    0.f,
                    flatDir.z / flatLen};
                desiredHoriz = {n.x * targetSpeed, 0.f, n.z * targetSpeed};
            }

            float accel = body.isGrounded ? move.acceleration : move.acceleration * move.airControl;
            const float t = std::min(1.f, accel * dt);
            body.velocity.x = body.velocity.x + (desiredHoriz.x - body.velocity.x) * t;
            body.velocity.z = body.velocity.z + (desiredHoriz.z - body.velocity.z) * t;

            if (move.wantsToJump && body.isGrounded) {
                body.velocity.y = move.jumpForce;
                body.isGrounded = false;
                move.state = MovementState::Jump;
            }
            move.wantsToJump = false;

            move.currentSpeed = std::sqrt(body.velocity.x * body.velocity.x + body.velocity.z * body.velocity.z);

            if (!body.isGrounded && !registry.hasComponent<PlayerTagComponent>(e))
                move.state = body.velocity.y > 0.35f ? MovementState::Jump : MovementState::Falling;

            if (!registry.hasComponent<PlayerTagComponent>(e) && move.state != MovementState::Idle &&
                length(move.desiredDirection) > 0.01f)
                transform.rotation = smoothYawTowardDirection(transform.rotation, move.desiredDirection, dt, 12.f);
        }
    }

private:
    static float getSpeed(const MovementComponent& m)
    {
        switch (m.state) {
        case MovementState::Walk:
            return m.walkSpeed;
        case MovementState::Run:
            return m.runSpeed;
        case MovementState::Sprint:
            return m.sprintSpeed;
        case MovementState::Crouch:
            return m.crouchSpeed;
        case MovementState::Crawl:
            return m.crawlSpeed;
        case MovementState::Jump:
        case MovementState::Falling:
            return m.walkSpeed * 0.85f;
        case MovementState::Idle:
        default:
            return 0.0f;
        }
    }
};
