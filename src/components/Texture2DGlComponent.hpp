#pragma once

#include <cstdint>
#include <string>

// -----------------------------------------------------------------------------
// Optional single 2D texture binding for simple passes. For full PBR sets
// (albedo, normal, AO, roughness, displacement) use PbrTextureSetComponent or
// PbrMaterialPresetComponent instead.
// -----------------------------------------------------------------------------

struct Texture2DGlComponent {
    std::uint32_t glTexture = 0;
    std::string sourcePath;
    bool sRgb = true;
};
