#pragma once

#include "../../components/BoxColliderComponent.hpp"
#include "../../components/CapsuleColliderComponent.hpp"
#include "../../components/RigidBodyComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../ecs/Registry.hpp"

/// Runs after `PhysicsSystem`. Extends terrain grounding with feet-on-support: static boxes, dynamic boxes, and other capsules.
class CollisionSystem
{
public:
    void update(Registry& registry, float /*dt*/)
    {
        for (auto e : registry.getEntitiesWith<TransformComponent, RigidBodyComponent, CapsuleColliderComponent>()) {
            auto& body = registry.getComponent<RigidBodyComponent>(e);
            if (body.invMass <= 0.f)
                continue;

            const auto& t = registry.getComponent<TransformComponent>(e);
            const auto& cap = registry.getComponent<CapsuleColliderComponent>(e);
            const float cx = t.position.x + cap.offset.x;
            const float cz = t.position.z + cap.offset.z;
            const float capBottom = t.position.y + cap.offset.y - cap.halfHeight - cap.radius;

            bool onSupport = false;

            for (auto other : registry.getEntitiesWith<TransformComponent, RigidBodyComponent, BoxColliderComponent>()) {
                if (other == e)
                    continue;

                const auto& bt = registry.getComponent<TransformComponent>(other);
                const auto& box = registry.getComponent<BoxColliderComponent>(other);
                const float minX = bt.position.x + box.offset.x - box.halfExtents.x;
                const float maxX = bt.position.x + box.offset.x + box.halfExtents.x;
                const float minZ = bt.position.z + box.offset.z - box.halfExtents.z;
                const float maxZ = bt.position.z + box.offset.z + box.halfExtents.z;
                const float topY = bt.position.y + box.offset.y + box.halfExtents.y;

                if (cx >= minX && cx <= maxX && cz >= minZ && cz <= maxZ && capBottom <= topY + 0.12f && capBottom >= topY - 0.28f) {
                    onSupport = true;
                    break;
                }
            }

            if (!onSupport) {
                for (auto other : registry.getEntitiesWith<TransformComponent, RigidBodyComponent, CapsuleColliderComponent>()) {
                    if (other == e)
                        continue;
                    const auto& bt = registry.getComponent<TransformComponent>(other);
                    const auto& ocap = registry.getComponent<CapsuleColliderComponent>(other);
                    const float ocx = bt.position.x + ocap.offset.x;
                    const float ocz = bt.position.z + ocap.offset.z;
                    const float otherTop = bt.position.y + ocap.offset.y + ocap.halfHeight + ocap.radius;
                    const float dx = cx - ocx;
                    const float dz = cz - ocz;
                    const float horiz = cap.radius + ocap.radius + 0.08f;
                    if (dx * dx + dz * dz > horiz * horiz)
                        continue;
                    if (capBottom <= otherTop + 0.14f && capBottom >= otherTop - 0.35f) {
                        onSupport = true;
                        break;
                    }
                }
            }

            body.isGrounded = body.isGrounded || onSupport;
        }
    }
};
