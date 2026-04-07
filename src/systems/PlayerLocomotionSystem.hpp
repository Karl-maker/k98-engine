#pragma once

#include "../ecs/Registry.hpp"
#include "../game/StateEventType.hpp"
#include "../components/GroundingStateComponent.hpp"
#include "../components/LocomotionRuntimeComponent.hpp"
#include "../components/LocomotorControlComponent.hpp"
#include "../components/MassComponent.hpp"
#include "../components/MovementComponent.hpp"
#include "../components/PlayerMovementIntentComponent.hpp"
#include "../components/Position.hpp"
#include "../components/StateMachineComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/Velocity.hpp"
#include "../components/GameplayTags.hpp"
#include "../statemachine/StateMachine.hpp"
#include "../systems/GravitySystem.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/TerrainHeightField.hpp"

#include <cmath>
#include <string>

// -----------------------------------------------------------------------------
// Player-only: FSM events (Idle / Moving / Sprinting / Jumping), horizontal
// velocity from MovementComponent, gravity integration, ground snap.
// -----------------------------------------------------------------------------

class PlayerLocomotionSystem {
public:
    void update(
        Registry& registry,
        SpatialGridSystem& grid,
        TerrainHeightField* terrain,
        float worldGravityY,
        float floorY,
        double dt)
    {
        const float fdt = static_cast<float>(dt);

        for (Entity e : registry.getEntitiesWith<
                 PlayerTagComponent,
                 Position,
                 Velocity,
                 StateMachineComponent,
                 LocomotorControlComponent,
                 MovementComponent,
                 MassComponent,
                 LocomotionRuntimeComponent,
                 GroundingStateComponent>()) {
            auto& sm   = registry.getComponent<StateMachineComponent>(e).machine;
            auto& pos  = registry.getComponent<Position>(e);
            auto& vel  = registry.getComponent<Velocity>(e);
            auto& ctrl = registry.getComponent<LocomotorControlComponent>(e);
            auto& mv   = registry.getComponent<MovementComponent>(e);

            auto& run = registry.getComponent<LocomotionRuntimeComponent>(e);
            auto& ground = registry.getComponent<GroundingStateComponent>(e);

            // Before integration: used for jump / locomotion FSM input (must have support at frame start).
            const bool groundedStart =
                GravitySystem::isGroundedForJump(registry, e, grid, terrain);
            ground.grounded = groundedStart;

            const float inputMag =
                std::abs(ctrl.forward) + std::abs(ctrl.right);
            const bool wantsMove = inputMag > 0.02f;

            if (groundedStart) {
                if (wantsMove)
                    sm.handleEvent(StateEventType::MoveUp);

                if (ctrl.sprintHeld && !run.sprintWasHeldLastFrame)
                    sm.handleEvent(StateEventType::SprintPress);
                else if (!ctrl.sprintHeld && run.sprintWasHeldLastFrame)
                    sm.handleEvent(StateEventType::SprintRelease);

                if (!wantsMove)
                    sm.handleEvent(StateEventType::Stop);
            }

            if (ctrl.jumpRequested && groundedStart) {
                sm.handleEvent(StateEventType::Jump);
                ctrl.jumpRequested = false;
            } else if (ctrl.jumpRequested && !groundedStart) {
                ctrl.jumpRequested = false;
            }

            sm.update(dt);

            applyHorizontalFromState(registry, e, sm, mv, vel, groundedStart, fdt);

            GravitySystem::applyGravityForEntity(registry, e, worldGravityY, fdt);

            pos.x += vel.x * fdt;
            pos.y += vel.y * fdt;
            pos.z += vel.z * fdt;

            if (registry.hasComponent<MassComponent>(e)) {
                const auto& mass = registry.getComponent<MassComponent>(e);
                GravitySystem::resolveGroundPenetration(registry, grid, terrain, e, mass);
            } else if (pos.y < floorY) {
                pos.y = floorY;
                vel.y = 0.0f;
            }

            if (registry.hasComponent<TransformComponent>(e)) {
                auto& tf = registry.getComponent<TransformComponent>(e);
                tf.position.x = pos.x;
                tf.position.y = pos.y;
                tf.position.z = pos.z;
            }

            // Landed must run *after* integration: on the first frame feet touch support,
            // groundedStart is still false at the top of the frame; only now are we grounded.
            const bool groundedEnd =
                GravitySystem::isGroundedForJump(registry, e, grid, terrain);
            ground.grounded = groundedEnd;
            if (groundedEnd && !run.wasGroundedLastFrame)
                sm.handleEvent(StateEventType::Landed);

            // Keep legacy intent zeroed so other tooling does not double-apply.
            if (registry.hasComponent<PlayerMovementIntentComponent>(e)) {
                auto& intent        = registry.getComponent<PlayerMovementIntentComponent>(e);
                intent.horizontalVelX = 0.0f;
                intent.horizontalVelZ = 0.0f;
                intent.jumpPressed    = false;
            }

            run.wasGroundedLastFrame = groundedEnd;
            run.sprintWasHeldLastFrame = ctrl.sprintHeld;
        }
    }

private:
    static void applyHorizontalFromState(
        Registry& registry,
        Entity e,
        StateMachine& sm,
        const MovementComponent& mv,
        Velocity& vel,
        bool grounded,
        float /*fdt*/)
    {
        const std::string& st = sm.getCurrentState();
        const Vec3& d         = mv.desiredWorldVelocityXZ;

        auto applyXZ = [&](float scale) {
            vel.x = d.x * scale;
            vel.z = d.z * scale;
        };

        if (st == "Idle") {
            if (grounded) {
                vel.x = 0.0f;
                vel.z = 0.0f;
            } else
                applyXZ(mv.airControl);
            return;
        }

        if (st == "Moving") {
            applyXZ(grounded ? 1.0f : mv.airControl);
            return;
        }

        if (st == "Sprinting") {
            const float s = grounded ? mv.sprintMultiplier : mv.airControl;
            applyXZ(s);
            return;
        }

        if (st == "Jumping") {
            applyXZ(mv.airControl);
            return;
        }

        applyXZ(grounded ? 1.0f : mv.airControl);
    }
};
