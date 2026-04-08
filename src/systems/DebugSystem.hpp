#pragma once
#include "../ecs/Registry.hpp"
#include "../components/StateMachineComponent.hpp"

class DebugSystem
{
public:
    void update(Registry& registry, float dt)
    {
        auto entities = registry.getEntitiesWith<StateMachineComponent>();

        for (auto e : entities)
        {
            auto& sm = registry.getComponent<StateMachineComponent>(e).machine;
        }
    }
};