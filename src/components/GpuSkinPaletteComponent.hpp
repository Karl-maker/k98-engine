#pragma once

#include "../math/Mat4.hpp"
#include <vector>

// -----------------------------------------------------------------------------
// GpuSkinPaletteComponent — joint skin matrices (global * inverseBind) for the
// entity's rig, uploaded by gameplay each frame after sampling animation.
// OpenGLVer2Renderer uses this with StaticMeshComponent when non-empty.
// Register: registry.registerComponent<GpuSkinPaletteComponent>();
// -----------------------------------------------------------------------------

struct GpuSkinPaletteComponent {
    std::vector<Mat4> jointSkinMatrices;
};
