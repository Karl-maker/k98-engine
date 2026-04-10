#pragma once

#include "../core/assets/AssetManager.hpp"
#include "../core/assets/IAsset.hpp"
#include "../core/assets/ModelAsset.hpp"
#include "../components/HeightMapComponent.hpp"
#include "../components/RenderableMeshComponent.hpp"
#include "../components/StaticMeshMaterialOverrideComponent.hpp"
#include "../components/TerrainChunkComponent.hpp"
#include "../components/TerrainChunkTileSetComponent.hpp"
#include "../components/TerrainSettingsComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../graphics/IGraphicsRenderer.hpp"
#include "../utils/Biome.hpp"
#include "../utils/GltfTerrainHeightBake.hpp"
#include "../utils/Noise.hpp"
#include "../utils/TerrainHeightField.hpp"
#include "../domain/terrain/TerrainWorldMap.hpp"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_set>

namespace {

inline std::string formatChunkPath(const std::string& fmt, int cx, int cz)
{
    if (fmt.empty())
        return {};
    char buf[2048];
    const int n = std::snprintf(buf, sizeof(buf), fmt.c_str(), cx, cz);
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf))
        return {};
    return std::string(buf);
}

} // namespace

// -----------------------------------------------------------------------------
// Streams heightmap terrain chunks around the player. Fills HeightMapComponent
// using biome noise, a single repeating glTF (`TerrainSettingsComponent`), or
// per-chunk paths from `TerrainChunkTileSetComponent` (e.g. chunk_0_0.glb).
// Mirrors heights into TerrainHeightField for CPU sampling / physics.
//
// Pass non-null AssetManager (+ IGraphicsRenderer for textured tiles).
// -----------------------------------------------------------------------------

class TerrainChunkSystem {
public:
    void update(
        Registry& registry,
        Vec3 playerPos,
        TerrainHeightField* heightField,
        const TerrainWorldMap* worldMap = nullptr,
        AssetManager* assets = nullptr,
        IGraphicsRenderer* renderer = nullptr)
    {
        auto settingsEntities = registry.getEntitiesWith<TerrainSettingsComponent>();
        if (settingsEntities.empty())
            return;

        Entity settingsEntity = settingsEntities[0];
        auto& settings = registry.getComponent<TerrainSettingsComponent>(settingsEntity);

        const TerrainChunkTileSetComponent* tileSet = nullptr;
        if (registry.hasComponent<TerrainChunkTileSetComponent>(settingsEntity))
            tileSet = &registry.getComponent<TerrainChunkTileSetComponent>(settingsEntity);

        const int chunkSize = settings.chunkSize;
        const float cellSize = settings.scale;
        const float stride = static_cast<float>(chunkSize) * cellSize;
        const int radius = settings.renderRadius;

        const int playerChunkX = static_cast<int>(std::floor(playerPos.x / stride));
        const int playerChunkZ = static_cast<int>(std::floor(playerPos.z / stride));

        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                const int cx = playerChunkX + x;
                const int cz = playerChunkZ + z;

                if (!chunkExists(registry, cx, cz))
                    createChunk(
                        registry,
                        cx,
                        cz,
                        settings,
                        tileSet,
                        heightField,
                        stride,
                        cellSize,
                        worldMap,
                        assets,
                        renderer);
            }
        }

        auto chunks = registry.getEntitiesWith<TerrainChunkComponent>();

        for (Entity e : chunks) {
            auto& chunk = registry.getComponent<TerrainChunkComponent>(e);

            const int dx = chunk.chunkX - playerChunkX;
            const int dz = chunk.chunkZ - playerChunkZ;

            if (std::abs(dx) > radius || std::abs(dz) > radius) {
                if (heightField)
                    heightField->unregisterChunk(chunk.chunkX, chunk.chunkZ);
                registry.destroyEntity(e);
            }
        }
    }

private:
    static void fillHeightMap(
        HeightMapComponent& hm,
        int cx,
        int cz,
        float stride,
        float cellSize,
        const TerrainSettingsComponent& settings,
        const TerrainWorldMap* worldMap,
        AssetManager* assets,
        const TerrainChunkTileSetComponent* tileSet)
    {
        auto tryBakeFromPath = [&](const std::string& path, float uniformScale, int meshIndex) -> bool {
            if (path.empty() || !assets)
                return false;
            std::shared_ptr<IAsset> a = assets->load(path);
            auto model = std::dynamic_pointer_cast<ModelAsset>(a);
            if (!model || meshIndex < 0 || static_cast<size_t>(meshIndex) >= model->meshes.size())
                return false;
            const Mesh& mesh = model->meshes[static_cast<size_t>(meshIndex)];
            if (mesh.vertices.empty())
                return false;
            bakeMeshTrianglesToHeightMap(
                mesh,
                cx,
                cz,
                stride,
                cellSize,
                uniformScale,
                hm,
                settings.gltfHeightBakeFallback);
            return true;
        };

        float us = settings.gltfFloorUniformScale;
        int mi = settings.gltfFloorMeshIndex;
        if (tileSet) {
            if (tileSet->uniformScale > 0.f)
                us = tileSet->uniformScale;
            if (tileSet->meshIndex >= 0)
                mi = tileSet->meshIndex;
        }

        if (tileSet && !tileSet->meshPathFormat.empty() && assets) {
            const std::string path = formatChunkPath(tileSet->meshPathFormat, cx, cz);
            if (tryBakeFromPath(path, us, mi))
                return;
        }

        if (!settings.gltfFloorAssetPath.empty() && assets) {
            if (tryBakeFromPath(settings.gltfFloorAssetPath, settings.gltfFloorUniformScale, settings.gltfFloorMeshIndex))
                return;
        }

        const int n = hm.size;
        if (settings.flatTerrain) {
            for (int z = 0; z < n; ++z) {
                for (int x = 0; x < n; ++x)
                    hm.set(x, z, settings.flatTerrainHeight);
            }
            return;
        }
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const float wx = static_cast<float>(cx) * stride + static_cast<float>(x) * cellSize;
                const float wz = static_cast<float>(cz) * stride + static_cast<float>(z) * cellSize;
                BiomeComponent biome = (worldMap && worldMap->loaded) ? worldMap->sampleBiome(wx, wz) : getBiome(wx, wz);
                const float h = fbm(wx, wz, biome) * biome.heightMultiplier;
                hm.set(x, z, h);
            }
        }
    }

    static bool chunkExists(Registry& registry, int x, int z)
    {
        for (Entity e : registry.getEntitiesWith<TerrainChunkComponent>()) {
            const auto& c = registry.getComponent<TerrainChunkComponent>(e);
            if (c.chunkX == x && c.chunkZ == z)
                return true;
        }
        return false;
    }

    static void createChunk(
        Registry& registry,
        int cx,
        int cz,
        TerrainSettingsComponent& settings,
        const TerrainChunkTileSetComponent* tileSet,
        TerrainHeightField* heightField,
        float stride,
        float cellSize,
        const TerrainWorldMap* worldMap,
        AssetManager* assets,
        IGraphicsRenderer* renderer)
    {
        Entity e = registry.createEntity();

        HeightMapComponent hm{};
        hm.size = settings.chunkSize + 1;
        hm.heights.assign(static_cast<size_t>(hm.size * hm.size), 0.0f);
        fillHeightMap(hm, cx, cz, stride, cellSize, settings, worldMap, assets, tileSet);

        TerrainChunkComponent tc{};
        tc.chunkX = cx;
        tc.chunkZ = cz;
        tc.size = settings.chunkSize;
        tc.scale = settings.scale;
        tc.generated = true;
        tc.skipProceduralTerrainGpuMesh = false;

        TransformComponent tf{};
        tf.position = {cx * stride, 0.0f, cz * stride};

        registry.addComponent(e, hm);
        registry.addComponent(e, tf);
        registry.addComponent(e, WorldTransformComponent{});

        float us = settings.gltfFloorUniformScale;
        int mi = settings.gltfFloorMeshIndex;
        if (tileSet) {
            if (tileSet->uniformScale > 0.f)
                us = tileSet->uniformScale;
            if (tileSet->meshIndex >= 0)
                mi = tileSet->meshIndex;
        }

        if (assets && renderer && settings.useGltfFloorVisual) {
            std::string meshPath;
            bool useTileSet = tileSet && !tileSet->meshPathFormat.empty();
            if (useTileSet)
                meshPath = formatChunkPath(tileSet->meshPathFormat, cx, cz);
            else if (!settings.gltfFloorAssetPath.empty())
                meshPath = settings.gltfFloorAssetPath;

            if (!meshPath.empty()) {
                std::shared_ptr<IAsset> a = assets->load(meshPath);
                auto model = std::dynamic_pointer_cast<ModelAsset>(a);
                if (model && !model->meshes.empty() && mi >= 0 && static_cast<size_t>(mi) < model->meshes.size()) {
                    static std::unordered_set<std::string> uploadedKeys;
                    if (uploadedKeys.insert(meshPath).second)
                        renderer->uploadStaticModel(*model, meshPath);

                    RenderableMeshComponent rm{};
                    rm.assetCacheKey = meshPath;
                    rm.gpuRegistered = true;
                    rm.uniformScale = us;
                    registry.addComponent(e, std::move(rm));
                    tc.skipProceduralTerrainGpuMesh = true;

                    if (useTileSet &&
                        (!tileSet->albedoPathFormat.empty() || !tileSet->normalPathFormat.empty() ||
                            !tileSet->occlusionPathFormat.empty() || !tileSet->metallicRoughnessPathFormat.empty())) {
                        StaticMeshMaterialOverrideComponent mo{};
                        if (!tileSet->albedoPathFormat.empty())
                            mo.albedoTexturePath = formatChunkPath(tileSet->albedoPathFormat, cx, cz);
                        if (!tileSet->normalPathFormat.empty())
                            mo.normalTexturePath = formatChunkPath(tileSet->normalPathFormat, cx, cz);
                        if (!tileSet->occlusionPathFormat.empty())
                            mo.occlusionTexturePath = formatChunkPath(tileSet->occlusionPathFormat, cx, cz);
                        if (!tileSet->metallicRoughnessPathFormat.empty())
                            mo.metallicRoughnessTexturePath = formatChunkPath(tileSet->metallicRoughnessPathFormat, cx, cz);
                        registry.addComponent(e, std::move(mo));
                    } else if (
                        !settings.gltfFloorAlbedoOverride.empty() || !settings.gltfFloorNormalOverride.empty() ||
                        !settings.gltfFloorOcclusionOverride.empty() || !settings.gltfFloorMetallicRoughnessOverride.empty()) {
                        StaticMeshMaterialOverrideComponent mo{};
                        mo.albedoTexturePath = settings.gltfFloorAlbedoOverride;
                        mo.normalTexturePath = settings.gltfFloorNormalOverride;
                        mo.occlusionTexturePath = settings.gltfFloorOcclusionOverride;
                        mo.metallicRoughnessTexturePath = settings.gltfFloorMetallicRoughnessOverride;
                        registry.addComponent(e, std::move(mo));
                    }
                }
            }
        }

        registry.addComponent(e, tc);

        if (heightField) {
            const auto& stored = registry.getComponent<HeightMapComponent>(e);
            heightField->registerChunk(cx, cz, stored.size, settings.scale, stored.heights);
        }
    }
};
