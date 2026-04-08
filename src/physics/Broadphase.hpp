#pragma once
#include "../ecs/Registry.hpp"
#include "../components/TransformComponent.hpp"
#include "../physics/SpatialGrid.hpp"

void buildSpatialGrid(Registry& registry, SpatialGrid& grid)
{
    grid.clear();

    auto entities = registry.getEntitiesWith<TransformComponent>();

    for (auto e : entities)
    {
        auto& t = registry.getComponent<TransformComponent>(e);
        grid.insert(e, t.position);
    }
}