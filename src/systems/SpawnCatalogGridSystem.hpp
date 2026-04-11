#pragma once

#include "../components/TransformComponent.hpp"
#include "../domain/spawn/SpawnArchetypeFactories.hpp"
#include "../domain/spawn/SpawnCatalog.hpp"
#include "../domain/spawn/SpawnContext.hpp"
#include "../domain/spawn/SpawnEventBus.hpp"
#include "../domain/spawn/SpawnFactoryRegistry.hpp"
#include "../domain/spawn/SpawnRuntimeState.hpp"
#include "../domain/spawn/SpawnStreamingSystem.hpp"
#include "../ecs/Entity.hpp"
#include "../ecs/Registry.hpp"
#include "../graphics/IGraphicsRenderer.hpp"
#include "../math/Vec3.hpp"
#include "../utils/TerrainHeightField.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

class AssetManager;

/// Owns grid-based spawn catalogs (`catalog_(gx,gz).json`), streaming spawn/despawn, and the factory registry.
///
/// **Archetypes** — the engine does not register any factory names by default. Call `factories().registerArchetype("name", ...)`
/// for each JSON `"archetype"` value your game uses. Implement `spawn::ISpawnArchetypeFactory` or reuse helpers from
/// `SpawnArchetypeFactories.hpp` (e.g. `PropSpawnFactory`, `EnemySpawnFactory`).
///
/// **Chunk changes** are debounced so walking along chunk borders does not reload JSON and destroy spawns every frame.
class SpawnCatalogGridSystem {
public:
    static constexpr const char* kDefaultSearchRoots[] = {
        "domain/spawns/",
        "../domain/spawns/",
        "../../domain/spawns/",
        "../../../game-engine/domain/spawns/",
    };

    /// How long the player must stay in a new chunk before the catalog swaps (seconds).
    static constexpr float kCellSwitchHoldSec = 0.35f;

    spawn::SpawnFactoryRegistry& factories() { return m_factories; }

    void setServices(Registry* registry, AssetManager* assets, IGraphicsRenderer* renderer, TerrainHeightField* terrainHeights)
    {
        m_registry = registry;
        m_assets = assets;
        m_renderer = renderer;
        m_terrainHeights = terrainHeights;
    }

    void setPlayerEntity(Entity player) { m_player = player; }

    void setTerrainGrid(int chunkSize, float scale)
    {
        m_chunkSize = chunkSize;
        m_scale = scale;
    }

    void setCatalogBaseName(std::string baseName) { m_catalogBaseName = std::move(baseName); }

    void setEventBus(spawn::SpawnEventBus* bus) { m_events = bus; }

    /// Same glTF path the player uses — passed to `SpawnContext` so enemies can use `spawnBusinessManCharacter`.
    void setSharedCharacterGltfPath(std::string path) { m_sharedCharacterGltfPath = std::move(path); }

    /// Grid cells around the player to merge spawn JSONs: `1` = 3×3 (center + 8 neighbors), `2` = 5×5, etc.
    void setCatalogNeighborRadius(int r) { m_catalogNeighborRadius = std::max(0, r); }

    /// How many queued spawn entries to resolve per frame (main thread). ECS/registry work cannot run on a worker thread.
    void setMaxSpawnsPerFrame(int n) { m_streaming.maxSpawnsPerFrame = std::max(1, n); }

    /// Frames to wait after queueing before `ISpawnArchetypeFactory::spawn` runs (heavy work is spread across frames).
    void setSpawnWarmupFrames(int n) { m_streaming.spawnWarmupFrames = std::max(0, n); }

    /// `dt` — use real frame time from the game loop; debounces chunk transitions. Default is fine for one-shot calls.
    void update(float dt = 1.f / 60.f)
    {
        if (!m_registry || m_player == INVALID_ENTITY || !m_registry->hasComponent<TransformComponent>(m_player))
            return;

        const float stride = static_cast<float>(m_chunkSize) * m_scale;
        const Vec3& pp = m_registry->getComponent<TransformComponent>(m_player).position;
        const int gx = static_cast<int>(std::floor(pp.x / stride));
        const int gz = static_cast<int>(std::floor(pp.z / stride));

        if (!m_cellGridInitialized) {
            loadCatalogForCell(gx, gz, stride);
            m_cellGridInitialized = true;
            m_cellX = gx;
            m_cellZ = gz;
            m_pendingGx = gx;
            m_pendingGz = gz;
            m_cellHoldTimer = kCellSwitchHoldSec;
        } else if (gx == m_cellX && gz == m_cellZ) {
            m_pendingGx = gx;
            m_pendingGz = gz;
            m_cellHoldTimer = kCellSwitchHoldSec;
        } else {
            if (gx != m_pendingGx || gz != m_pendingGz) {
                m_pendingGx = gx;
                m_pendingGz = gz;
                m_cellHoldTimer = kCellSwitchHoldSec;
            } else {
                m_cellHoldTimer -= std::max(0.f, dt);
                if (m_cellHoldTimer <= 0.f) {
                    loadCatalogForCell(m_pendingGx, m_pendingGz, stride);
                    m_cellX = m_pendingGx;
                    m_cellZ = m_pendingGz;
                    m_cellHoldTimer = kCellSwitchHoldSec;
                }
            }
        }

        m_ctx.worldSeed = m_catalog.worldSeed;
        m_ctx.registry = m_registry;
        m_ctx.assets = m_assets;
        m_ctx.renderer = m_renderer;
        m_ctx.playerEntity = m_player;
        m_ctx.terrainHeights = m_terrainHeights;
        m_ctx.sharedCharacterGltfPath = m_sharedCharacterGltfPath;
        m_streaming.update(m_ctx, m_catalog, m_runtime, m_factories, m_events);
    }

    void destroyAllSpawnedEntities() { destroySpawnRuntimeEntities(m_registry, m_runtime); }

private:
    void loadCatalogForCell(int gx, int gz, float stride)
    {
        /// Do not destroy spawned entities here — only `SpawnStreamingSystem` removes them when player exceeds `despawnRadius` from live positions.
        if (!spawn::loadSpawnCatalogMergedNeighborhood(
                kDefaultSearchRoots,
                sizeof(kDefaultSearchRoots) / sizeof(kDefaultSearchRoots[0]),
                m_catalogBaseName,
                gx,
                gz,
                stride,
                m_catalogNeighborRadius,
                m_catalog))
            m_catalog = {};
    }

    static void destroySpawnRuntimeEntities(Registry* reg, spawn::SpawnRuntimeState& st)
    {
        if (!reg)
            return;
        for (auto& kv : st.bySpawnId) {
            for (Entity e : kv.second.entities) {
                if (e != INVALID_ENTITY && reg->hasComponent<TransformComponent>(e))
                    reg->destroyEntity(e);
            }
        }
        st = {};
    }

    Registry* m_registry = nullptr;
    AssetManager* m_assets = nullptr;
    IGraphicsRenderer* m_renderer = nullptr;
    TerrainHeightField* m_terrainHeights = nullptr;
    Entity m_player = INVALID_ENTITY;

    int m_chunkSize = 32;
    float m_scale = 1.f;
    std::string m_catalogBaseName = "catalog";
    std::string m_sharedCharacterGltfPath{};

    spawn::SpawnCatalogData m_catalog{};
    spawn::SpawnRuntimeState m_runtime{};
    spawn::SpawnFactoryRegistry m_factories{};
    spawn::SpawnStreamingSystem m_streaming{};
    spawn::SpawnContext m_ctx{};
    spawn::SpawnEventBus* m_events = nullptr;

    bool m_cellGridInitialized = false;
    int m_cellX = 0;
    int m_cellZ = 0;
    int m_pendingGx = 0;
    int m_pendingGz = 0;
    float m_cellHoldTimer = 0.f;

    /// Merge catalog JSON from neighboring terrain cells so spawns just across a chunk boundary are evaluated.
    int m_catalogNeighborRadius = 1;
};
