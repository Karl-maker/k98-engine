#pragma once

#include "../ecs/Registry.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Mat4.hpp"

class WorldTransformSyncSystem
{
public:
    void update(Registry& registry)
    {
        for (Entity e : registry.getEntitiesWith<TransformComponent, WorldTransformComponent>()) {
            auto& t = registry.getComponent<TransformComponent>(e);
            auto& w = registry.getComponent<WorldTransformComponent>(e);
            w.world = Mat4::FromTRS(t.position, t.rotation, t.scale);
        }
    }
};
