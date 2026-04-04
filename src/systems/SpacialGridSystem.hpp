#pragma once
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

    void update(Registry& registry)
    {
        cells.clear();

        auto entities = registry.getEntitiesWith<TransformComponent, CollisionBoxComponent>();

        for (auto e : entities)
        {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);
            insert(e, box.min, box.max);
        }
    }

    // =========================
    // Query nearby entities
    // =========================
    std::vector<Entity> getNearby(const Vec3& min, const Vec3& max)
    {
        std::unordered_set<Entity> resultSet;

        GridKey minKey = getKey(min);
        GridKey maxKey = getKey(max);

        for (int x = minKey.x; x <= maxKey.x; x++)
        for (int y = minKey.y; y <= maxKey.y; y++)
        for (int z = minKey.z; z <= maxKey.z; z++)
        {
            GridKey key{x, y, z};

            auto it = cells.find(key);
            if (it != cells.end())
            {
                for (auto e : it->second)
                    resultSet.insert(e);
            }
        }

        return std::vector<Entity>(resultSet.begin(), resultSet.end());
    }

private:
    std::unordered_map<GridKey, std::vector<Entity>, GridKeyHash> cells;

    GridKey getKey(const Vec3& pos)
    {
        // Floor so negative world coordinates map to correct negative cell indices
        // (truncation toward zero would put small negatives in cell 0 and break min/max loops).
        return {
            static_cast<int>(std::floor(pos.x / cellSize)),
            static_cast<int>(std::floor(pos.y / cellSize)),
            static_cast<int>(std::floor(pos.z / cellSize))
        };
    }

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
            GridKey key{x, y, z};
            cells[key].push_back(e);
        }
    }
};