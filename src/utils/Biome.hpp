#pragma once
#include "Noise.hpp"
#include "../components/BiomeComponent.hpp"

inline BiomeComponent getBiome(float worldX, float worldZ)
{
    float biomeValue = noise(worldX * 0.001f, worldZ * 0.001f);

    BiomeComponent biome;

    if (biomeValue < -0.3f) {
        // plains
        biome.heightMultiplier = 5.0f;
        biome.noiseScale = 0.02f;
    }
    else if (biomeValue < 0.3f) {
        // hills
        biome.heightMultiplier = 10.0f;
        biome.noiseScale = 0.05f;
    }
    else {
        // mountains
        biome.heightMultiplier = 25.0f;
        biome.noiseScale = 0.08f;
    }

    return biome;
}