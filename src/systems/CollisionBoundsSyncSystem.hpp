#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../components/TransformComponent.hpp"

// -----------------------------------------------------------------------------
// Updates CollisionBoxComponent min/max from TransformComponent.position (same
// rules as CollisionSystem). Run before SpatialGridSystem::update when broad-
// phase must match transforms without a full collision pass.
// -----------------------------------------------------------------------------

class CollisionBoundsSyncSystem {
public:
    void update(Registry& registry) const
    {
        for (Entity e : registry.getEntitiesWith<CollisionBoxComponent, TransformComponent>()) {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);
            auto& tf  = registry.getComponent<TransformComponent>(e);
            box.applyAuthoringToHalfExtents();
            if (box.primitive == CollisionPrimitive::Sphere) {
                const float r = box.halfSize.x > 1e-6f ? box.halfSize.x : 0.5f;
                box.min = tf.position - Vec3{r, r, r};
                box.max = tf.position + Vec3{r, r, r};
            } else {
                box.min = tf.position - box.halfSize;
                box.max = tf.position + box.halfSize;
            }
        }
    }
};
