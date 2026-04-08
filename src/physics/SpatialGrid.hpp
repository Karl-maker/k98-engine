#pragma once
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cmath>
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"

struct SpatialGrid
{
    float cellSize = 5.0f;

    std::unordered_map<int64_t, std::vector<Entity>> cells;

    int64_t hash(float xf, float yf, float zf) const
    {
        const int x = static_cast<int>(xf);
        const int y = static_cast<int>(yf);
        const int z = static_cast<int>(zf);
        return (static_cast<int64_t>(x) << 42) ^ (static_cast<int64_t>(y) << 21) ^ static_cast<int64_t>(z);
    }

    Vec3 toCell(const Vec3& pos) const
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

    /// Broadphase: all grid cells intersecting the axis-aligned bounding box of segment [a,b], deduplicated.
    void querySegment(const Vec3& a, const Vec3& b, std::vector<Entity>& out) const
    {
        out.clear();
        const float mnX = std::min(a.x, b.x);
        const float mnY = std::min(a.y, b.y);
        const float mnZ = std::min(a.z, b.z);
        const float mxX = std::max(a.x, b.x);
        const float mxY = std::max(a.y, b.y);
        const float mxZ = std::max(a.z, b.z);

        const int minCx = static_cast<int>(std::floor(mnX / cellSize));
        const int maxCx = static_cast<int>(std::floor(mxX / cellSize));
        const int minCy = static_cast<int>(std::floor(mnY / cellSize));
        const int maxCy = static_cast<int>(std::floor(mxY / cellSize));
        const int minCz = static_cast<int>(std::floor(mnZ / cellSize));
        const int maxCz = static_cast<int>(std::floor(mxZ / cellSize));

        std::unordered_set<Entity> seen;
        for (int cx = minCx; cx <= maxCx; ++cx) {
            for (int cy = minCy; cy <= maxCy; ++cy) {
                for (int cz = minCz; cz <= maxCz; ++cz) {
                    const int64_t key = hash(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
                    auto it = cells.find(key);
                    if (it == cells.end())
                        continue;
                    for (Entity e : it->second) {
                        if (seen.insert(e).second)
                            out.push_back(e);
                    }
                }
            }
        }
    }

    void clear() { cells.clear(); }
};