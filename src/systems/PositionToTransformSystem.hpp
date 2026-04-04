#pragma once

// =============================================================================
// PositionToTransformSystem
// -----------------------------------------------------------------------------
// Copies simulation Position into TransformComponent.position for every entity
// that has both. Run after integrating velocity into Position (gameplay / physics
// step) and before systems that read transforms (animation, sockets, render).
//
// Registration:
//   registry.registerComponent<Position>();
//   registry.registerComponent<TransformComponent>();
//
// Example (game loop):
//   updateActors(dt);  // writes Position
//   positionToTransformSystem.update(registry);
//   animationSystem.update(registry, dt);
//
// Order: typically early each frame, after Position is authoritative.
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/Position.hpp"
#include "../components/TransformComponent.hpp"

class PositionToTransformSystem {
public:
    void update(Registry& registry) const
    {
        auto entities = registry.getEntitiesWith<Position, TransformComponent>();

        for (Entity e : entities)
        {
            const auto& pos       = registry.getComponent<Position>(e);
            auto&       transform = registry.getComponent<TransformComponent>(e);
            transform.position    = {pos.x, pos.y, pos.z};
        }
    }
};
