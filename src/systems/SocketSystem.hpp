#pragma once

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
