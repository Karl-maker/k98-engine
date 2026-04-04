#pragma once

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
            if (!socket) continue; // 🚀 CRITICAL FIX

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
