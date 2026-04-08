#pragma once

#include <string>
#include "../math/Vec2.hpp"
#include "../texture/TextureTypes.hpp"

struct TextureComponent
{
    std::string path;     // file path or asset id
    TextureType type;

    int width = 0;
    int height = 0;
    int channels = 0;

    // Optional UV tiling
    Vec2 tiling = {1.0f, 1.0f};

    TextureComponent() = default;

    TextureComponent(const std::string& path, TextureType type)
        : path(path), type(type) {}
};