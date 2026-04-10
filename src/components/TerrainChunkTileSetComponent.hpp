#pragma once

#include <string>

/// Per-chunk terrain pieces: paths are built from chunk grid indices (may be negative), e.g. (0,0), (1,-1).
/// Use `printf`-style format with **two int** arguments `(chunkX, chunkZ)`:
///   mesh:  "assets/terrain/tiles/chunk_%d_%d.glb"
///   albedo: "assets/terrain/tiles/chunk_%d_%d_albedo.png"
///
/// "Stitching": each chunk entity's transform is at `(chunkX * stride, 0, chunkZ * stride)`; each mesh should
/// cover one chunk footprint in local space (typically 0 .. chunkSize*scale on X/Z after `uniformScale`).
struct TerrainChunkTileSetComponent {
    /// Required for per-chunk loading: non-empty enables tile-set mode (overrides `TerrainSettingsComponent::gltfFloorAssetPath` for mesh source).
    std::string meshPathFormat{};

    std::string albedoPathFormat{};
    std::string normalPathFormat{};
    std::string occlusionPathFormat{};
    std::string metallicRoughnessPathFormat{};

    /// Per-chunk `ModelAsset::meshes[]` index; **-1** = use `TerrainSettingsComponent::gltfFloorMeshIndex`.
    int meshIndex = -1;
    /// **0** = use `TerrainSettingsComponent::gltfFloorUniformScale`.
    float uniformScale = 0.f;
};
