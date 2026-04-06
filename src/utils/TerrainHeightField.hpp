#pragma once

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// CPU heightfield mirror of active terrain chunks for O(1) chunk lookup + O(1)
// bilinear sample (per-chunk grid). Used by grounding / gameplay — not GPU data.
// -----------------------------------------------------------------------------

class TerrainHeightField {
public:
    struct ChunkData {
        int chunkX = 0;
        int chunkZ = 0;
        int gridSize = 33; // vertices per axis (chunkSize + 1)
        float cellSize = 1.0f;
        float worldStride = 32.0f; // chunk spacing in world units (chunkSize * cellSize)
        std::vector<float> heights;
    };

    void clear() { m_chunks.clear(); }

    void registerChunk(int chunkX, int chunkZ, int gridSize, float cellSize, std::vector<float> heights)
    {
        ChunkData d;
        d.chunkX = chunkX;
        d.chunkZ = chunkZ;
        d.gridSize = gridSize;
        d.cellSize = cellSize;
        d.worldStride = static_cast<float>(gridSize - 1) * cellSize;
        d.heights = std::move(heights);
        m_worldStride = d.worldStride;
        m_chunks[pack(chunkX, chunkZ)] = std::move(d);
    }

    void unregisterChunk(int chunkX, int chunkZ)
    {
        m_chunks.erase(pack(chunkX, chunkZ));
    }

    /// Bilinear height in world space; false if no chunk registered at that cell.
    bool trySampleHeight(float worldX, float worldZ, float& outHeight) const
    {
        if (m_chunks.empty() || m_worldStride <= 1e-6f)
            return false;

        const int cx = static_cast<int>(std::floor(worldX / m_worldStride));
        const int cz = static_cast<int>(std::floor(worldZ / m_worldStride));
        auto it = m_chunks.find(pack(cx, cz));
        if (it == m_chunks.end())
            return false;

        const ChunkData& c = it->second;
        const float ox = static_cast<float>(c.chunkX) * m_worldStride;
        const float oz = static_cast<float>(c.chunkZ) * m_worldStride;

        const float lx = worldX - ox;
        const float lz = worldZ - oz;
        const float fx = lx / c.cellSize;
        const float fz = lz / c.cellSize;
        const int ix = static_cast<int>(std::floor(fx));
        const int iz = static_cast<int>(std::floor(fz));
        const int n = c.gridSize;
        if (ix < 0 || iz < 0 || ix >= n - 1 || iz >= n - 1)
            return false;

        const float tx = fx - static_cast<float>(ix);
        const float tz = fz - static_cast<float>(iz);

        const float h00 = c.heights[iz * n + ix];
        const float h10 = c.heights[iz * n + (ix + 1)];
        const float h01 = c.heights[(iz + 1) * n + ix];
        const float h11 = c.heights[(iz + 1) * n + (ix + 1)];

        const float h0 = h00 * (1.0f - tx) + h10 * tx;
        const float h1 = h01 * (1.0f - tx) + h11 * tx;
        outHeight = h0 * (1.0f - tz) + h1 * tz;
        return true;
    }

    bool empty() const { return m_chunks.empty(); }

private:
    static uint64_t pack(int cx, int cz)
    {
        return (uint64_t(static_cast<uint32_t>(cx)) << 32) | uint64_t(static_cast<uint32_t>(cz));
    }

    float m_worldStride = 32.0f;
    std::unordered_map<uint64_t, ChunkData> m_chunks;
};
