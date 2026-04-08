#pragma once

#include <cstdint>

// -----------------------------------------------------------------------------
// PbrTextureSetComponent — GPU handles for a standard PBR texture set used by
// terrain and any mesh that shares the same fragment shader binding layout.
//
// Convention (matches OpenGLVer2 textured shader):
//   - Albedo: sRGB sample
//   - Normal: tangent-space, OpenGL normal map (Y-up green)
//   - Occlusion: R channel (linear)
//   - Roughness-only maps: bound as “metallicRoughness”; fragment reads .g for roughness
//   - Displacement: height in R (linear), optional cavity / future parallax
// -----------------------------------------------------------------------------

struct PbrTextureSetComponent {
    std::uint32_t albedo        = 0;
    std::uint32_t normalMap     = 0;
    std::uint32_t occlusionMap  = 0;
    std::uint32_t roughnessMap  = 0;
    std::uint32_t displacementMap = 0;
};
