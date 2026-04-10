#pragma once

#include <string>

struct TerrainSettingsComponent {
    int chunkSize = 32;
    float scale = 1.0f;

    int renderRadius = 2; // 5x5 chunks

    /// When true, chunk heightmaps are filled with `flatTerrainHeight` (no noise).
    bool flatTerrain = false;
    float flatTerrainHeight = 0.0f;

    // --- Optional glTF floor tile per chunk (same mesh repeated at each chunk origin) ---
    /// Non-empty: bake triangle heights into the chunk grid so `TerrainHeightField` / grounding match the mesh.
    /// Example: `"assets/terrain/floor_tile.glb"`.
    std::string gltfFloorAssetPath{};
    /// Which `ModelAsset::meshes[]` entry to bake (most floor tiles use mesh 0).
    int gltfFloorMeshIndex = 0;
    /// Model units → world: tune so the mesh spans one chunk (`chunkSize * scale` world units on X/Z).
    float gltfFloorUniformScale = 1.0f;
    /// Height used where the mesh does not cover a grid sample (and for procedural fallback if bake fails).
    float gltfHeightBakeFallback = 0.0f;
    /// If true (and `gltfFloorAssetPath` set), chunk entity gets `RenderableMeshComponent` + GPU upload; procedural terrain mesh is skipped.
    bool useGltfFloorVisual = true;

    /// Optional: override glTF material textures for floor chunks (empty = use baked glTF maps). Paths are passed to `stbi_load` (absolute or cwd-relative).
    std::string gltfFloorAlbedoOverride{};
    std::string gltfFloorNormalOverride{};
    std::string gltfFloorOcclusionOverride{};
    std::string gltfFloorMetallicRoughnessOverride{};
};