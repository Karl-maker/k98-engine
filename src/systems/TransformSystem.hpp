#pragma once

#include "../ecs/Registry.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../ecs/Entity.hpp"
#include "../math/Mat4.hpp"

class TransformSystem
{
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<TransformComponent, WorldTransformComponent>();

        for (auto e : entities)
        {
            auto& local = registry.getComponent<TransformComponent>(e);
            auto& world = registry.getComponent<WorldTransformComponent>(e);

            // Root entity
            if (local.parent == INVALID_ENTITY)
            {
                world.world = Mat4::Identity();
                world.world.m[12] = local.position.x;
                world.world.m[13] = local.position.y;
                world.world.m[14] = local.position.z;
                continue;
            }

            // Child entity: inherit parent translation, then add local offset
            if (!registry.hasComponent<WorldTransformComponent>(local.parent))
            {
                world.world = Mat4::Identity();
                world.world.m[12] = local.position.x;
                world.world.m[13] = local.position.y;
                world.world.m[14] = local.position.z;
                continue;
            }

            auto& parentWorld = registry.getComponent<WorldTransformComponent>(local.parent);

            world.world = Mat4::Identity();
            world.world.m[12] = parentWorld.world.m[12] + local.position.x;
            world.world.m[13] = parentWorld.world.m[13] + local.position.y;
            world.world.m[14] = parentWorld.world.m[14] + local.position.z;
        }
    }
};