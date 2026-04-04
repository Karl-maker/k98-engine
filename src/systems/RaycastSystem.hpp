#pragma once

// =============================================================================
// RaycastSystem — tests RayComponent (+ optional sweep radius) against dynamic
// AABBs in SpatialGridSystem; writes RaycastHitComponent (closest hit).
//
// Registration:
//   registry.registerComponent<RayComponent>();
//   registry.registerComponent<RaycastHitComponent>();
//
// Construction: holds a reference to the same SpatialGridSystem instance that
// CollisionSystem updated this frame (grid must be current after grid.update).
//
// Example:
//   SpatialGridSystem grid;
//   RaycastSystem raycast{grid};
//   grid.update(registry);
//   collisionSystem.update(registry, grid);
//   facingRaySystem.update(registry);
//   raycast.update(registry);
//
// Preconditions: ray.direction normalized; box.min/max valid for colliders.
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/RayComponent.hpp"
#include "../components/RayHitComponent.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/RaycastUtils.hpp"

#include <cmath>

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

            Vec3 origin = ray.origin;
            Vec3 dir    = ray.direction; // MUST be normalized

            float cellSize = grid.cellSize;

            // =========================
            // START CELL
            // =========================
            auto key = grid.getKey(origin);

            int x = key.x;
            int y = key.y;
            int z = key.z;

            int stepX = (dir.x > 0) ? 1 : -1;
            int stepY = (dir.y > 0) ? 1 : -1;
            int stepZ = (dir.z > 0) ? 1 : -1;

            float nextBoundaryX = (x + (stepX > 0 ? 1 : 0)) * cellSize;
            float nextBoundaryY = (y + (stepY > 0 ? 1 : 0)) * cellSize;
            float nextBoundaryZ = (z + (stepZ > 0 ? 1 : 0)) * cellSize;

            float tMaxX = (dir.x != 0.0f) ? (nextBoundaryX - origin.x) / dir.x : 1e9f;
            float tMaxY = (dir.y != 0.0f) ? (nextBoundaryY - origin.y) / dir.y : 1e9f;
            float tMaxZ = (dir.z != 0.0f) ? (nextBoundaryZ - origin.z) / dir.z : 1e9f;

            float tDeltaX = (dir.x != 0.0f) ? cellSize / std::abs(dir.x) : 1e9f;
            float tDeltaY = (dir.y != 0.0f) ? cellSize / std::abs(dir.y) : 1e9f;
            float tDeltaZ = (dir.z != 0.0f) ? cellSize / std::abs(dir.z) : 1e9f;

            float t = 0.0f;

            // =========================
            // DDA LOOP
            // =========================
            while (t <= ray.maxDistance)
            {
                const auto& cellEntities = grid.getCell(x, y, z);

                for (auto target : cellEntities)
                {
                    if (target == ray.ignoreEntity)
                        continue;

                    if (!registry.hasComponent<CollisionBoxComponent>(target))
                        continue;

                    auto& box = registry.getComponent<CollisionBoxComponent>(target);

                    // =========================
                    // 🔥 LAYER FILTER
                    // =========================
                    if ((box.layer & ray.layerMask) == 0)
                        continue;

                    float hitT;

                    bool hitSomething = false;

                    if (ray.radius > 0.0f)
                    {
                        // =========================
                        // 🔥 SPHERE CAST
                        // =========================
                        Vec3 point = origin + dir * t;

                        if (RaycastUtils::sphereAabb(point, ray.radius, box.min, box.max))
                        {
                            hitT = t;
                            hitSomething = true;
                        }
                    }
                    else
                    {
                        // =========================
                        // 🔥 NORMAL RAY
                        // =========================
                        if (RaycastUtils::rayAabb(
                                origin,
                                dir,
                                box.min,
                                box.max,
                                hitT))
                        {
                            hitSomething = true;
                        }
                    }

                    if (hitSomething)
                    {
                        if (hitT >= 0.0f && hitT < hit.distance && hitT <= ray.maxDistance)
                        {
                            hit.hit      = true;
                            hit.entity   = target;
                            hit.distance = hitT;
                            hit.point    = origin + dir * hitT;
                        }
                    }
                }

                // =========================
                // EARLY EXIT
                // =========================
                if (hit.hit && hit.distance < t)
                    break;

                // =========================
                // STEP GRID
                // =========================
                if (tMaxX < tMaxY && tMaxX < tMaxZ)
                {
                    x += stepX;
                    t = tMaxX;
                    tMaxX += tDeltaX;
                }
                else if (tMaxY < tMaxZ)
                {
                    y += stepY;
                    t = tMaxY;
                    tMaxY += tDeltaY;
                }
                else
                {
                    z += stepZ;
                    t = tMaxZ;
                    tMaxZ += tDeltaZ;
                }
            }
        }
    }

private:
    SpatialGridSystem& grid;
};