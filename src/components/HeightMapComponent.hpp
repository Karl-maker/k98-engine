#pragma once
#include <vector>

struct HeightMapComponent {
    int size;
    std::vector<float> heights;

    float get(int x, int z) const {
        return heights[z * size + x];
    }

    void set(int x, int z, float h) {
        heights[z * size + x] = h;
    }
};
