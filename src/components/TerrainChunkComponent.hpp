#pragma once

struct TerrainChunkComponent {
    float scale = 1.f;
    int size = 32;        // grid resolution

    int chunkX;
    int chunkZ;

    bool generated = false;
};
