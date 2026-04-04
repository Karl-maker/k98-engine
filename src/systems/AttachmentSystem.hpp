#pragma once

// =============================================================================
// AttachmentSystem — for entities with AttachComponent + TransformComponent,
// copies world position from `socketEntity`’s SocketComponent.worldTransform
// (plus offset) when inheritPosition is true; rotation from rotationOffset when
// inheritRotation is true.
//
// Registration:
//   registry.registerComponent<AttachComponent>();
//   registry.registerComponent<TransformComponent>();
//   SocketComponent must exist on socketEntity (registered separately).
//
// Example:
//   AttachmentSystem attach;
//   attach.update(registry);
//
// Order: after SocketSystem. If the attached entity has a RayComponent, run
// FacingRaySystem after this so ray.origin matches the final transform.
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/AttachComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/SocketComponent.hpp"

class AttachmentSystem {
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<AttachComponent, TransformComponent>();

        for (auto e : entities)
        {
            auto& attach = registry.getComponent<AttachComponent>(e);
            auto& transform = registry.getComponent<TransformComponent>(e);

            auto* socket = registry.tryGetComponent<SocketComponent>(attach.socketEntity);
            if (!socket) continue;

            if (attach.inheritPosition)
            {
                transform.position.x = socket->worldTransform.m[12] + attach.offset.x;
                transform.position.y = socket->worldTransform.m[13] + attach.offset.y;
                transform.position.z = socket->worldTransform.m[14] + attach.offset.z;
            }

            if (attach.inheritRotation)
            {
                transform.rotation = attach.rotationOffset;
            }
        }
    }
};
