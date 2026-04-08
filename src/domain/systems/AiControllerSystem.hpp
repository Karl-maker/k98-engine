#pragma once

#include "../../ecs/Registry.hpp"
#include "../../components/TransformComponent.hpp"
#include "../components/ChaseComponent.hpp"
#include "../components/MovementComponent.hpp"
#include "../states/MovementStateEnums.hpp"
#include "../../math/MathOps.hpp"
#include "../../math/Vec3.hpp"

class AIControllerSystem
{
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<MovementComponent, ChaseComponent, TransformComponent>();

        for (auto e : entities) {
            auto& move = registry.getComponent<MovementComponent>(e);
            auto& chase = registry.getComponent<ChaseComponent>(e);
            auto& transform = registry.getComponent<TransformComponent>(e);

            if (chase.chaseTarget == INVALID_ENTITY || !registry.hasComponent<TransformComponent>(chase.chaseTarget)) {
                move.desiredDirection = {0.f, 0.f, 0.f};
                move.state = MovementState::Idle;
                continue;
            }

            const auto& targetTf = registry.getComponent<TransformComponent>(chase.chaseTarget);
            Vec3 toTarget{
                targetTf.position.x - transform.position.x,
                0.f,
                targetTf.position.z - transform.position.z};

            if (lengthSquared(toTarget) < 1e-4f) {
                move.desiredDirection = {0.f, 0.f, 0.f};
                move.state = MovementState::Idle;
            } else {
                move.desiredDirection = normalize(toTarget);
                move.state = MovementState::Run;
            }
        }
    }
};
