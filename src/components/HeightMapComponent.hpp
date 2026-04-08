#pragma once

#include <algorithm>
#include <vector>

struct HeightMapComponent {
    int size = 0;
    std::vector<float> heights;

    float get(int x, int z) const
    {
        if (size <= 0 || heights.empty())
            return 0.f;
        x = std::max(0, std::min(x, size - 1));
        z = std::max(0, std::min(z, size - 1));
        return heights[static_cast<size_t>(z * size + x)];
    }
};
