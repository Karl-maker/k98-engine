#pragma once

#include "../math/Mat4.hpp"
#include <vector>

/// Joint palette for GPU skinning: `jointMatrix[i] = globalBone[i] * inverseBind[i]`.
struct GpuSkinPaletteComponent {
    std::vector<Mat4> jointSkinMatrices;
};
