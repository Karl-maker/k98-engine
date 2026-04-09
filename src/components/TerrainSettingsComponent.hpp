#pragma once

struct TerrainSettingsComponent {
    int chunkSize = 32;
    float scale = 1.0f;

    int renderRadius = 2; // 5x5 chunks

    /// When true, chunk heightmaps are filled with `flatTerrainHeight` (no noise).
    bool flatTerrain = false;
    float flatTerrainHeight = 0.0f;
};