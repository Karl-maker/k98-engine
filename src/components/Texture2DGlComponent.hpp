#pragma once

#include <cstdint>
#include <string>

// -----------------------------------------------------------------------------
// Optional 2D texture binding for render passes. `glTexture` is 0 until a
// loader/upload step assigns it (MovementTutorial uses this for extension points).
// -----------------------------------------------------------------------------

struct Texture2DGlComponent {
    std::uint32_t glTexture = 0;
    std::string sourcePath;
    bool sRgb = true;
};
