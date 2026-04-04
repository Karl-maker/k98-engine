#pragma once

#include "../ecs/Registry.hpp"
#include "../components/RayComponent.hpp"
#include "../components/RayHitComponent.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/RaycastUtils.hpp"

#include <algorithm>

class RaycastSystem
{
public:
    explicit RaycastSystem(SpatialGridSystem& grid)
        : grid(grid)
    {
    }

    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<RayComponent, RaycastHitComponent>();

        for (auto e : entities)
        {
            auto& ray = registry.getComponent<RayComponent>(e);
            auto& hit = registry.getComponent<RaycastHitComponent>(e);

            hit.hit      = false;
            hit.entity   = INVALID_ENTITY;
            hit.distance = ray.maxDistance;

            const Vec3 rayEnd = ray.origin + ray.direction * ray.maxDistance;

            // getNearby expects axis-aligned min/max; origin and end are not ordered along the ray.
            const Vec3 segMin{
                std::min(ray.origin.x, rayEnd.x),
                std::min(ray.origin.y, rayEnd.y),
                std::min(ray.origin.z, rayEnd.z)};
            const Vec3 segMax{
                std::max(ray.origin.x, rayEnd.x),
                std::max(ray.origin.y, rayEnd.y),
                std::max(ray.origin.z, rayEnd.z)};

            const auto candidates = grid.getNearby(segMin, segMax);

            for (auto target : candidates)
            {
                if (target == ray.ignoreEntity)
                {
                    continue;
                }
                if (!registry.hasComponent<CollisionBoxComponent>(target))
                {
                    continue;
                }

                auto& box = registry.getComponent<CollisionBoxComponent>(target);

                float t = 0.0f;
                if (RaycastUtils::rayAabb(
                        ray.origin,
                        ray.direction,
                        box.min,
                        box.max,
                        t))
                {
                    if (t >= 0.0f && t < hit.distance && t <= ray.maxDistance)
                    {
                        hit.hit      = true;
                        hit.entity   = target;
                        hit.distance = t;
                        hit.point    = ray.origin + ray.direction * t;
                    }
                }
            }
        }
    }

private:
    SpatialGridSystem& grid;
};
