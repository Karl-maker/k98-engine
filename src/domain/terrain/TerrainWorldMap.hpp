#pragma once

#include "../../components/BiomeComponent.hpp"
#include "../../utils/Biome.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

/// Optional biome overrides per chunk (loaded from `domain/terrain/world_map.txt`).
/// Heights still use `fbm` with biome parameters; coordinates match `TerrainChunkSystem` chunk indices.
class TerrainWorldMap {
public:
    int chunkSize = 32;
    float scale = 1.f;
    int renderRadius = 3;
    bool loaded = false;

    bool loadFromFile(const std::string& path)
    {
        chunkSize = 32;
        scale = 1.f;
        renderRadius = 3;
        overrides.clear();
        loaded = false;

        std::ifstream in(path);
        if (!in)
            return false;

        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            std::istringstream iss(line);
            std::string key;
            iss >> key;
            if (key == "chunkSize") {
                iss >> chunkSize;
                if (chunkSize < 4)
                    chunkSize = 4;
            } else if (key == "scale") {
                iss >> scale;
                if (scale < 1e-4f)
                    scale = 1.f;
            } else if (key == "renderRadius") {
                iss >> renderRadius;
                if (renderRadius < 1)
                    renderRadius = 1;
                if (renderRadius > 12)
                    renderRadius = 12;
            } else if (key == "biome") {
                int cx = 0;
                int cz = 0;
                int id = 0;
                iss >> cx >> cz >> id;
                overrides[pack(cx, cz)] = id;
            }
        }
        loaded = true;
        return true;
    }

    /// Biome parameters for world XZ (uses chunk override or procedural `getBiome`).
    BiomeComponent sampleBiome(float worldX, float worldZ) const
    {
        const float stride = static_cast<float>(chunkSize) * scale;
        if (stride < 1e-4f)
            return getBiome(worldX, worldZ);
        const int cx = static_cast<int>(std::floor(worldX / stride));
        const int cz = static_cast<int>(std::floor(worldZ / stride));
        auto it = overrides.find(pack(cx, cz));
        if (it == overrides.end())
            return getBiome(worldX, worldZ);
        return biomeFromPresetId(it->second);
    }

private:
    static uint64_t pack(int cx, int cz)
    {
        return (uint64_t(static_cast<uint32_t>(cx)) << 32) | uint64_t(static_cast<uint32_t>(cz));
    }

    static BiomeComponent biomeFromPresetId(int id)
    {
        BiomeComponent b;
        switch (id) {
        case 0: // plains
            b.heightMultiplier = 5.f;
            b.noiseScale = 0.02f;
            b.octaves = 4;
            b.persistence = 0.5f;
            break;
        case 1: // hills
            b.heightMultiplier = 10.f;
            b.noiseScale = 0.05f;
            b.octaves = 4;
            b.persistence = 0.5f;
            break;
        case 2: // mountains
            b.heightMultiplier = 25.f;
            b.noiseScale = 0.08f;
            b.octaves = 5;
            b.persistence = 0.52f;
            break;
        case 3: // flat / shore
            b.heightMultiplier = 1.5f;
            b.noiseScale = 0.015f;
            b.octaves = 2;
            b.persistence = 0.4f;
            break;
        default:
            return getBiome(0.f, 0.f);
        }
        return b;
    }

    std::unordered_map<uint64_t, int> overrides;
};
