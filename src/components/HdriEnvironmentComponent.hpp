#pragma once

#include <string>

// -----------------------------------------------------------------------------
// HdriEnvironmentComponent — equirectangular environment map for IBL (diffuse +
// specular) in OpenGLRenderSystem. Only **one** active HDRI is used per frame (first
// enabled entity wins; additional entities are ignored with a one-time warning).
//
// Supports .png / .jpg / .jpeg / .bmp / .tga via stb_image, and .webp via libwebp.
//
// Register: registry.registerComponent<HdriEnvironmentComponent>();
// -----------------------------------------------------------------------------

struct HdriEnvironmentComponent {
    bool enabled = true;

    /// Filesystem path to equirectangular image (lat-long).
    std::string hdriAssetPath;

    /// Scales environment contribution.
    float intensity = 1.0f;

    /// Radians around world +Y; rotates the sampling direction before equirect lookup.
    float rotationY = 0.0f;

    /// How much diffuse IBL is mixed in (0 = off, 1 = full add).
    float diffuseEnvironmentWeight = 0.55f;

    /// Specular reflection from environment (scaled by roughness in shader).
    float specularEnvironmentWeight = 0.4f;
};
