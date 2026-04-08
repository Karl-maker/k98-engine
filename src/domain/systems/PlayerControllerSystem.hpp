#pragma once

#include "../../ecs/Registry.hpp"
#include "../../components/CameraComponent.hpp"
#include "../../components/PlayerTagComponent.hpp"
#include "../../components/ThirdPersonComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../components/MovementComponent.hpp"
#include "../states/MovementStateEnums.hpp"
#include "../../math/MathOps.hpp"
#include "../../math/Vec3.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class PlayerControllerSystem
{
public:
    void update(Registry& registry, GLFWwindow* window, Entity cameraEntity)
    {
        if (!window || cameraEntity == INVALID_ENTITY)
            return;
        if (!registry.hasComponent<CameraComponent>(cameraEntity) || !registry.hasComponent<ThirdPersonComponent>(cameraEntity))
            return;

        auto entities = registry.getEntitiesWith<MovementComponent, PlayerTagComponent>();

        const bool spaceNow = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        const bool jumpPress = spaceNow && !m_spaceWasHeld;
        m_spaceWasHeld = spaceNow;

        auto& cam = registry.getComponent<CameraComponent>(cameraEntity);
        auto& tp = registry.getComponent<ThirdPersonComponent>(cameraEntity);

        for (auto e : entities) {
            auto& move = registry.getComponent<MovementComponent>(e);

            if (cam.lookAtTarget == INVALID_ENTITY || !registry.hasComponent<TransformComponent>(cam.lookAtTarget)) {
                move.desiredDirection = {0.f, 0.f, 0.f};
                move.state = MovementState::Idle;
                move.wantsToJump = jumpPress;
                continue;
            }

            const Vec3 forward = tp.planarMoveForward;
            const Vec3 right = tp.planarMoveRight;

            float f = 0.f;
            float r = 0.f;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                f += 1.f;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                f -= 1.f;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                r -= 1.f;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                r += 1.f;

            const Vec3 wish{
                forward.x * f + right.x * r,
                0.f,
                forward.z * f + right.z * r};

            if (lengthSquared(wish) < 1e-8f) {
                move.desiredDirection = {0.f, 0.f, 0.f};
                move.state = MovementState::Idle;
            } else {
                move.desiredDirection = normalize(wish);
                const bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                    glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
                move.state = sprint ? MovementState::Sprint : MovementState::Walk;
            }

            move.wantsToJump = jumpPress;
        }
    }

private:
    bool m_spaceWasHeld = false;
};
