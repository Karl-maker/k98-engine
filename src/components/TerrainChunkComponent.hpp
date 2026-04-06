#pragma once

struct TerrainChunkComponent {
    int chunkX;
    int chunkZ;

    int size = 32;        // grid resolution
    float scale = 1.0f;

    bool generated = false;
};