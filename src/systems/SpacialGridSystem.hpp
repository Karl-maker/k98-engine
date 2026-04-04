#pragma once

// =============================================================================
// SpatialGridSystem — uniform grid broad-phase for collision and ray queries.
// `cellSize` should be tuned to your world scale (larger = fewer cells, coarser).
// `update(registry)` rebuilds cells from all entities with Transform + CollisionBox
// using each box’s `min` and `max`. CollisionSystem sets those from transform.position
// and halfSize, then calls `grid.update(registry)` internally before narrow-phase.
// If you call `grid.update` alone, ensure min/max are up to date first.
//
// Registration:
//   registry.registerComponent<TransformComponent>();
//   registry.registerComponent<CollisionBoxComponent>();
//
// Example:
//   SpatialGridSystem grid;
//   grid.cellSize = 4.f;
//   grid.update(registry);
//   collisionSystem.update(registry, grid);
//   raycastSystem.update(registry); // internal use of grid
//
// `getNearby(min, max)` returns entities overlapping AABB keys (used by
// CollisionSystem and RaycastSystem).
// =============================================================================

#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <cmath>

#include "../math/Vec3.hpp"
#include "../ecs/Registry.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../ecs/Entity.hpp"

// =========================
// Grid Key
// =========================
struct GridKey {
    int x, y, z;

    bool operator==(const GridKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct GridKeyHash {
    size_t operator()(const GridKey& k) const {
        return ((k.x * 73856093) ^ (k.y * 19349663) ^ (k.z * 83492791));
    }
};

// =========================
// Spatial Grid System
// =========================
class SpatialGridSystem {
public:
    float cellSize = 5.0f;

    // =========================
    // 🔥 IMPORTANT: Needed for DDA
    // =========================
    GridKey getKey(const Vec3& pos) const
    {
        return {
            static_cast<int>(std::floor(pos.x / cellSize)),
            static_cast<int>(std::floor(pos.y / cellSize)),
            static_cast<int>(std::floor(pos.z / cellSize))
        };
    }

    // =========================
    // Fast cell lookup (no allocation)
    // =========================
    const std::vector<Entity>& getCell(int x, int y, int z) const
    {
        static const std::vector<Entity> empty;

        GridKey key{x, y, z};

        auto it = cells.find(key);
        return (it != cells.end()) ? it->second : empty;
    }

    // =========================
    // Update grid (multi-cell AABB)
    // =========================
    void update(Registry& registry)
    {
        cells.clear();

        auto entities = registry.getEntitiesWith<TransformComponent, CollisionBoxComponent>();

        for (auto e : entities)
        {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);

            // ⚠️ Ensure bounds are valid before inserting
            insert(e, box.min, box.max);
        }
    }

    // =========================
    // Broad query (fallback use)
    // =========================
    std::vector<Entity> getNearby(const Vec3& min, const Vec3& max) const
    {
        std::unordered_set<Entity> resultSet;

        GridKey minKey = getKey(min);
        GridKey maxKey = getKey(max);

        for (int x = minKey.x; x <= maxKey.x; x++)
        for (int y = minKey.y; y <= maxKey.y; y++)
        for (int z = minKey.z; z <= maxKey.z; z++)
        {
            const auto& cell = getCell(x, y, z);

            for (auto e : cell)
                resultSet.insert(e);
        }

        return std::vector<Entity>(resultSet.begin(), resultSet.end());
    }

private:
    std::unordered_map<GridKey, std::vector<Entity>, GridKeyHash> cells;

    // =========================
    // MULTI-CELL INSERTION
    // =========================
    void insert(Entity e, const Vec3& min, const Vec3& max)
    {
        GridKey minKey = getKey(min);
        GridKey maxKey = getKey(max);

        for (int x = minKey.x; x <= maxKey.x; x++)
        for (int y = minKey.y; y <= maxKey.y; y++)
        for (int z = minKey.z; z <= maxKey.z; z++)
        {
            cells[{x, y, z}].push_back(e);
        }
    }
};