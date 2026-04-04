#pragma once

// =============================================================================
// SocketSystem — for each SocketComponent, sets `socket.worldTransform` from the
// parent entity’s WorldTransformComponent and the socket’s local TRS.
//
// Registration:
//   registry.registerComponent<SocketComponent>();
//   registry.registerComponent<WorldTransformComponent>(); // on parentEntity
//
// Example:
//   SocketSystem sockets;
//   sockets.update(registry);
//
// Order: run after TransformSystem so parent `world` is current. Required before
// AttachmentSystem and FacingRaySystem (ray origins on attached entities).
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/SocketComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Mat4.hpp"

class SocketSystem {
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<SocketComponent>();

        for (auto e : entities)
        {
            auto& socket = registry.getComponent<SocketComponent>(e);

            auto* parentWorld = registry.tryGetComponent<WorldTransformComponent>(socket.parentEntity);
            if (!parentWorld) continue;

            Mat4 local = Mat4::FromTRS(socket.localOffset, socket.localRotation, {1.0f, 1.0f, 1.0f});
            socket.worldTransform = mat4Mul(parentWorld->world, local);
        }
    }
};
