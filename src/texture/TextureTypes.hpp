#pragma once

enum class TextureType
{
    Diffuse,        // base color (albedo)
    Normal,         // normal map
    Specular,       // specular intensity
    Roughness,      // roughness map
    Metallic,       // metallic map
    AmbientOcclusion,
    Emissive,
    Opacity
};