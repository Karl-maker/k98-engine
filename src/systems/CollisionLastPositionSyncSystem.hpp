#pragma once

// =============================================================================
// CollisionLastPositionSyncSystem
// -----------------------------------------------------------------------------
// Seeds CollisionBoxComponent::lastPosition from TransformComponent::position.
// Use **once** after transforms are valid at load (or after teleport), **not**
// every frame — CollisionSystem uses lastPosition vs current position to detect
// movement for broad-phase and swept logic.
//
// Registration:
//   registry.registerComponent<CollisionBoxComponent>();
//   registry.registerComponent<TransformComponent>();
//
// Example (level start, after first PositionToTransformSystem::update):
//   positionToTransformSystem.update(registry);
//   collisionLastPositionSyncSystem.seed(registry);
//   collisionSystem.update(registry, grid);
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../components/TransformComponent.hpp"

class CollisionLastPositionSyncSystem {
public:
    void seed(Registry& registry) const
    {
        auto entities = registry.getEntitiesWith<CollisionBoxComponent, TransformComponent>();
        for (Entity e : entities)
        {
            const auto& t = registry.getComponent<TransformComponent>(e).position;
            auto&       box = registry.getComponent<CollisionBoxComponent>(e);
            box.lastPosition = t;
        }
    }
};
