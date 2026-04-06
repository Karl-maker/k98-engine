#pragma once

struct TerrainSettingsComponent {
    int chunkSize = 32;
    float scale = 1.0f;

    int renderRadius = 2; // 5x5 chunks
};