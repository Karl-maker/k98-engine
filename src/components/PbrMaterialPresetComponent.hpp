#pragma once

#include "PbrTextureSetComponent.hpp"
#include <string>

struct PbrMaterialPresetComponent {
    std::string presetRootRelative;
    bool gpuUploaded = false;
    float surfaceUvRepeats = 4.f;
    PbrTextureSetComponent maps{};
};
