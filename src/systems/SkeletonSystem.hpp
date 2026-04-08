#pragma once

#include "../components/SkeletonComponent.hpp"

class SkeletonSystem
{
public:
    void update(Registry& registry, float dt)
    {
        auto entities = registry.getEntitiesWith<SkeletonComponent>();
        for (auto entity : entities)
        {
            auto& skeleton = registry.getComponent<SkeletonComponent>(entity);
            auto& transform = registry.getComponent<TransformComponent>(entity);
        }
    }
};