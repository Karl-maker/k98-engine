#pragma once
#include <cmath>
#include "../components/BiomeComponent.hpp"

inline float noise(float x, float z)
{
    return sin(x * 0.1f) * cos(z * 0.1f);
}

inline float fbm(float x, float z, const BiomeComponent& biome)
{
    float total = 0.0f;
    float frequency = biome.noiseScale;
    float amplitude = 1.0f;

    for (int i = 0; i < biome.octaves; i++)
    {
        total += noise(x * frequency, z * frequency) * amplitude;

        amplitude *= biome.persistence;
        frequency *= 2.0f;
    }

    return total;
}