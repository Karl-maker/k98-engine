#pragma once

#include <cstdint>

using TextureHandle = unsigned int;

/// GPU texture handles for PBR drawing (filled by OpenGL upload paths).
struct PbrTextureSetComponent {
    TextureHandle albedo = 0;
    TextureHandle normalMap = 0;
    TextureHandle occlusionMap = 0;
    TextureHandle roughnessMap = 0;
    TextureHandle metallicRoughnessMap = 0;
    TextureHandle displacementMap = 0;
};
