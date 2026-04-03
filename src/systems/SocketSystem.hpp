#pragma once

#include "../ecs/Registry.hpp"
#include "../components/SocketComponent.hpp"
#include "../components/WorldTransformComponent.hpp"

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

            socket.worldTransform = parentWorld->world;

            socket.worldTransform.m[12] += socket.localOffset.x;
            socket.worldTransform.m[13] += socket.localOffset.y;
            socket.worldTransform.m[14] += socket.localOffset.z;
        }
    }
};