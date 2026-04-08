#pragma once

#include <vector>
#include "TextureComponent.hpp"
#include "math/Vec3.hpp"

struct MaterialComponent
{
    std::vector<TextureComponent> textures;

    // fallback values (if no textures)
    Vec3 baseColor = {1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;

    bool useLighting = true;
};