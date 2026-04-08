#pragma once
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cmath>
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"

struct SpatialGrid
{
    float cellSize = 5.0f;

    std::unordered_map<int64_t, std::vector<Entity>> cells;

    int64_t hash(float xf, float yf, float zf)
    {
        const int x = static_cast<int>(xf);
        const int y = static_cast<int>(yf);
        const int z = static_cast<int>(zf);
        return (static_cast<int64_t>(x) << 42) ^ (static_cast<int64_t>(y) << 21) ^ static_cast<int64_t>(z);
    }

    Vec3 toCell(const Vec3& pos)
    {
        return Vec3{
            std::floor(pos.x / cellSize),
            std::floor(pos.y / cellSize),
            std::floor(pos.z / cellSize)};
    }

    void insert(Entity e, const Vec3& pos)
    {
        const Vec3 c = toCell(pos);
        cells[hash(c.x, c.y, c.z)].push_back(e);
    }

    std::vector<Entity> queryNearby(const Vec3& pos)
    {
        std::vector<Entity> result;
        const Vec3 c = toCell(pos);

        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
                for (int z = -1; z <= 1; ++z) {
                    const int64_t key =
                        hash(c.x + static_cast<float>(x), c.y + static_cast<float>(y), c.z + static_cast<float>(z));
                    if (cells.count(key))
                        result.insert(result.end(), cells[key].begin(), cells[key].end());
                }

        return result;
    }

    void clear() { cells.clear(); }
};