#pragma once

#include <string>

/// Per-entity overrides for static mesh texturing (`drawTexturedModel` / skinned path).
/// Empty string for a slot keeps the texture baked at `uploadStaticModel` time from the glTF material.
struct StaticMeshMaterialOverrideComponent {
    std::string albedoTexturePath;
    std::string normalTexturePath;
    std::string occlusionTexturePath;
    std::string metallicRoughnessTexturePath;
};
