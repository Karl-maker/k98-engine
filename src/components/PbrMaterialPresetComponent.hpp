#pragma once

#include "PbrTextureSetComponent.hpp"

#include <string>

// -----------------------------------------------------------------------------
// PbrMaterialPresetComponent — optional folder of PBR maps + GPU handles.
// Shared upload path: IGraphicsRenderer::uploadPbrMaterialPresets (OpenGLVer2Renderer).
//
// `presetRootRelative` is relative to GAME_ENGINE_PROJECT_ROOT (e.g. assets/textures/Hex-Tile).
// `surfaceUvRepeats` scales UV tiling along one mesh edge (e.g. terrain chunk width in cells).
// -----------------------------------------------------------------------------

struct PbrMaterialPresetComponent {
    std::string presetRootRelative;

    /// Texture repeats across one tiled edge (e.g. terrain chunk width in mesh cells).
    float surfaceUvRepeats = 4.0f;

    PbrTextureSetComponent maps{};

    bool gpuUploaded = false;
};
