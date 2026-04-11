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
#include "../math/Vec3.hpp"
#include "../utils/Biome.hpp"
#include "../utils/GltfTerrainHeightBake.hpp"
#include "../utils/Noise.hpp"
#include "../utils/TerrainHeightField.hpp"
#include "../domain/terrain/TerrainWorldMap.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <future>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

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
// glTF + embedded textures load via AssetManager::loadSharedAsync (single worker
// queue — no sync decode on the main thread). Chunks appear with procedural
// heights first; after decode, heights are re-baked from the mesh and visuals
// are attached (budgeted per frame to limit GPU upload spikes).
// -----------------------------------------------------------------------------

class TerrainChunkSystem {
public:
    /// Max chunk entities finalized per frame after async glTF decode (GPU upload + height rebake).
    int maxTerrainGltfFinalizesPerFrame = 2;

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

        tryFinalizePendingGltf(registry, settings, heightField, stride, cellSize, assets, renderer);

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
                removePendingForEntity(e);
                registry.destroyEntity(e);
            }
        }
    }

private:
    struct PendingTerrainGltf {
        Entity entity = INVALID_ENTITY;
        int cx = 0;
        int cz = 0;
        std::string meshPath;
        std::shared_future<std::shared_ptr<IAsset>> loadFut;
        float uniformScale = 1.f;
        int meshIndex = 0;
        bool addRenderableVisual = false;
        float stride = 0.f;
        float cellSize = 0.f;
        float heightBakeFallback = 0.f;
        std::string albedoPath;
        std::string normalPath;
        std::string occlusionPath;
        std::string metallicRoughnessPath;
    };

    std::vector<PendingTerrainGltf> m_pendingGltf;

    void removePendingForEntity(Entity e)
    {
        m_pendingGltf.erase(
            std::remove_if(
                m_pendingGltf.begin(),
                m_pendingGltf.end(),
                [e](const PendingTerrainGltf& p) { return p.entity == e; }),
            m_pendingGltf.end());
    }

    static void fillHeightMapProcedural(
        HeightMapComponent& hm,
        int cx,
        int cz,
        float stride,
        float cellSize,
        const TerrainSettingsComponent& settings,
        const TerrainWorldMap* worldMap)
    {
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

    void tryFinalizePendingGltf(
        Registry& registry,
        TerrainSettingsComponent& settings,
        TerrainHeightField* heightField,
        float stride,
        float cellSize,
        AssetManager* assets,
        IGraphicsRenderer* renderer)
    {
        if (m_pendingGltf.empty())
            return;

        int budget = std::max(0, maxTerrainGltfFinalizesPerFrame);
        static std::unordered_set<std::string> uploadedMeshKeys;

        for (auto it = m_pendingGltf.begin(); it != m_pendingGltf.end() && budget > 0;) {
            PendingTerrainGltf& p = *it;

            if (p.entity == INVALID_ENTITY || !registry.hasComponent<TerrainChunkComponent>(p.entity) ||
                !registry.hasComponent<HeightMapComponent>(p.entity)) {
                it = m_pendingGltf.erase(it);
                continue;
            }

            auto& tc = registry.getComponent<TerrainChunkComponent>(p.entity);
            if (tc.chunkX != p.cx || tc.chunkZ != p.cz) {
                it = m_pendingGltf.erase(it);
                continue;
            }

            if (p.loadFut.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                ++it;
                continue;
            }

            std::shared_ptr<IAsset> a = p.loadFut.get();
            auto model = std::dynamic_pointer_cast<ModelAsset>(a);
            if (!model || model->meshes.empty() || p.meshIndex < 0 ||
                static_cast<size_t>(p.meshIndex) >= model->meshes.size()) {
                it = m_pendingGltf.erase(it);
                continue;
            }

            auto& hm = registry.getComponent<HeightMapComponent>(p.entity);
            const Mesh& mesh = model->meshes[static_cast<size_t>(p.meshIndex)];
            if (!mesh.vertices.empty())
                bakeMeshTrianglesToHeightMap(
                    mesh, p.cx, p.cz, p.stride, p.cellSize, p.uniformScale, hm, p.heightBakeFallback);

            if (heightField) {
                heightField->unregisterChunk(p.cx, p.cz);
                heightField->registerChunk(p.cx, p.cz, hm.size, settings.scale, hm.heights);
            }

            if (p.addRenderableVisual && renderer && assets) {
                if (uploadedMeshKeys.insert(p.meshPath).second)
                    renderer->uploadStaticModel(*model, p.meshPath);

                if (!registry.hasComponent<RenderableMeshComponent>(p.entity)) {
                    RenderableMeshComponent rm{};
                    rm.assetCacheKey = p.meshPath;
                    rm.gpuRegistered = true;
                    rm.uniformScale = p.uniformScale;
                    registry.addComponent(p.entity, std::move(rm));
                    tc.skipProceduralTerrainGpuMesh = true;
                }

                const bool hasMo = !p.albedoPath.empty() || !p.normalPath.empty() || !p.occlusionPath.empty() ||
                    !p.metallicRoughnessPath.empty();
                if (hasMo && !registry.hasComponent<StaticMeshMaterialOverrideComponent>(p.entity)) {
                    StaticMeshMaterialOverrideComponent mo{};
                    mo.albedoTexturePath = p.albedoPath;
                    mo.normalTexturePath = p.normalPath;
                    mo.occlusionTexturePath = p.occlusionPath;
                    mo.metallicRoughnessTexturePath = p.metallicRoughnessPath;
                    registry.addComponent(p.entity, std::move(mo));
                }
            }

            it = m_pendingGltf.erase(it);
            --budget;
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

    void createChunk(
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

        float us = settings.gltfFloorUniformScale;
        int mi = settings.gltfFloorMeshIndex;
        if (tileSet) {
            if (tileSet->uniformScale > 0.f)
                us = tileSet->uniformScale;
            if (tileSet->meshIndex >= 0)
                mi = tileSet->meshIndex;
        }

        std::string meshPath;
        bool useTileSet = tileSet && !tileSet->meshPathFormat.empty();
        if (useTileSet)
            meshPath = formatChunkPath(tileSet->meshPathFormat, cx, cz);
        else if (!settings.gltfFloorAssetPath.empty())
            meshPath = settings.gltfFloorAssetPath;

        const bool wantsGltfMesh = !meshPath.empty() && assets;

        fillHeightMapProcedural(hm, cx, cz, stride, cellSize, settings, worldMap);

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

        if (wantsGltfMesh) {
            PendingTerrainGltf p{};
            p.entity = e;
            p.cx = cx;
            p.cz = cz;
            p.meshPath = meshPath;
            p.loadFut = assets->loadSharedAsync(meshPath);
            p.uniformScale = us;
            p.meshIndex = mi;
            p.addRenderableVisual = settings.useGltfFloorVisual && renderer != nullptr;
            p.stride = stride;
            p.cellSize = cellSize;
            p.heightBakeFallback = settings.gltfHeightBakeFallback;

            if (useTileSet &&
                (!tileSet->albedoPathFormat.empty() || !tileSet->normalPathFormat.empty() ||
                    !tileSet->occlusionPathFormat.empty() || !tileSet->metallicRoughnessPathFormat.empty())) {
                if (!tileSet->albedoPathFormat.empty())
                    p.albedoPath = formatChunkPath(tileSet->albedoPathFormat, cx, cz);
                if (!tileSet->normalPathFormat.empty())
                    p.normalPath = formatChunkPath(tileSet->normalPathFormat, cx, cz);
                if (!tileSet->occlusionPathFormat.empty())
                    p.occlusionPath = formatChunkPath(tileSet->occlusionPathFormat, cx, cz);
                if (!tileSet->metallicRoughnessPathFormat.empty())
                    p.metallicRoughnessPath = formatChunkPath(tileSet->metallicRoughnessPathFormat, cx, cz);
            } else if (
                !settings.gltfFloorAlbedoOverride.empty() || !settings.gltfFloorNormalOverride.empty() ||
                !settings.gltfFloorOcclusionOverride.empty() || !settings.gltfFloorMetallicRoughnessOverride.empty()) {
                p.albedoPath = settings.gltfFloorAlbedoOverride;
                p.normalPath = settings.gltfFloorNormalOverride;
                p.occlusionPath = settings.gltfFloorOcclusionOverride;
                p.metallicRoughnessPath = settings.gltfFloorMetallicRoughnessOverride;
            }

            m_pendingGltf.push_back(std::move(p));
        }

        registry.addComponent(e, tc);

        if (heightField) {
            const auto& stored = registry.getComponent<HeightMapComponent>(e);
            heightField->registerChunk(cx, cz, stored.size, settings.scale, stored.heights);
        }
    }
};
