#pragma once

#include "SpawnContext.hpp"
#include "SpawnEventBus.hpp"
#include "SpawnFactoryRegistry.hpp"
#include "SpawnRng.hpp"
#include "SpawnRuntimeState.hpp"
#include "SpawnTypes.hpp"

#include "../../components/TransformComponent.hpp"
#include "../../core/assets/AssetManager.hpp"
#include "../../ecs/Registry.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <vector>

namespace spawn {

class SpawnStreamingSystem {
public:
    /// After a spawn is queued, wait this many full frames before calling the factory (spreads heavy glTF / setup).
    int spawnWarmupFrames = 2;
    /// After warmup, instantiate at most this many catalog entries per frame.
    int maxSpawnsPerFrame = 2;

    void update(
        SpawnContext& ctx,
        const SpawnCatalogData& catalog,
        SpawnRuntimeState& state,
        SpawnFactoryRegistry& factories,
        SpawnEventBus* events = nullptr)
    {
        if (!ctx.registry)
            return;
        Entity player = ctx.playerEntity;
        if (player == INVALID_ENTITY || !ctx.registry->hasComponent<TransformComponent>(player))
            return;

        Registry& reg = *ctx.registry;
        const Vec3 pp = reg.getComponent<TransformComponent>(player).position;

        advanceSpawnPipelineAndInstantiate(ctx, catalog, state, factories, events, pp);

        std::unordered_set<std::string> catalogIds;
        catalogIds.reserve(catalog.spawns.size());
        for (const SpawnEntryDesc& e : catalog.spawns)
            catalogIds.insert(e.id);

        for (const SpawnEntryDesc& entry : catalog.spawns) {
            SpawnEntryRuntimeState& rs = state.bySpawnId[entry.id];

            pruneDeadEntities(reg, rs.entities);

            const float spawnR = entry.spawnRadius;
            const float despawnR = entry.despawnRadius;

            const float anchorDx = pp.x - entry.position.x;
            const float anchorDz = pp.z - entry.position.z;
            const float anchorDistSq = anchorDx * anchorDx + anchorDz * anchorDz;

            if (!rs.entities.empty()) {
                rs.despawnRadiusCached = despawnR;
                tryDespawnByLiveDistanceOnly(reg, pp, despawnR, rs, entry.id, events);
                continue;
            }

            if (anchorDistSq > spawnR * spawnR) {
                rs.spawnRollAttempted = false;
                continue;
            }

            if (rs.spawnRollAttempted)
                continue;

            rs.spawnRollAttempted = true;

            SpawnRng rng(hashSpawnId(entry.id) ^ (static_cast<uint64_t>(catalog.worldSeed) * 0x100000001ULL));
            ctx.rng = &rng;
            if (rng.nextU01() > entry.spawnProbability) {
                continue;
            }

            if (!factories.tryGet(entry.archetype)) {
                std::cerr << "SpawnStreamingSystem: unknown archetype \"" << entry.archetype << "\" for id \"" << entry.id
                          << "\"\n";
                continue;
            }

            if (pipelineContainsId(state, entry.id))
                continue;

            PendingSpawnSlot slot{};
            slot.entryId = entry.id;
            slot.framesUntilInstantiate = std::max(0, spawnWarmupFrames);
            if (ctx.assets) {
                ISpawnArchetypeFactory* prefetchFac = factories.tryGet(entry.archetype);
                if (prefetchFac) {
                    std::vector<std::string> paths;
                    prefetchFac->collectPrefetchAssetPaths(ctx, entry, paths);
                    for (const std::string& p : paths) {
                        if (!p.empty())
                            slot.pendingAssetLoads.push_back(ctx.assets->loadSharedAsync(p));
                    }
                }
            }
            state.pendingSpawnPipeline.push_back(std::move(slot));
        }

        /// Entries still in the world but no longer in the current catalog file (e.g. new chunk) — despawn only by cached live distance.
        for (auto it = state.bySpawnId.begin(); it != state.bySpawnId.end();) {
            if (catalogIds.count(it->first) != 0) {
                ++it;
                continue;
            }
            SpawnEntryRuntimeState& rs = it->second;
            pruneDeadEntities(reg, rs.entities);
            if (rs.entities.empty()) {
                it = state.bySpawnId.erase(it);
                continue;
            }
            const float useR = rs.despawnRadiusCached > 0.f ? rs.despawnRadiusCached : 1e6f;
            if (tryDespawnByLiveDistanceOnly(reg, pp, useR, rs, it->first, events))
                it = state.bySpawnId.erase(it);
            else
                ++it;
        }
    }

private:
    /// Returns true if everything was despawned (entities cleared). **Never** despawns unless planar distance to the
    /// closest live instance exceeds `despawnR` (and `despawnR` > 0). If distance cannot be measured, keeps instances.
    static bool tryDespawnByLiveDistanceOnly(
        Registry& reg,
        const Vec3& pp,
        float despawnR,
        SpawnEntryRuntimeState& rs,
        const std::string& entryId,
        SpawnEventBus* events)
    {
        if (despawnR <= 0.f)
            return false;
        if (rs.entities.empty())
            return false;

        float minEntityDistSq = std::numeric_limits<float>::infinity();
        for (Entity e : rs.entities) {
            if (e == INVALID_ENTITY || !reg.hasComponent<TransformComponent>(e))
                continue;
            const Vec3 ep = reg.getComponent<TransformComponent>(e).position;
            const float edx = pp.x - ep.x;
            const float edz = pp.z - ep.z;
            const float d2 = edx * edx + edz * edz;
            minEntityDistSq = std::min(minEntityDistSq, d2);
        }

        if (!std::isfinite(minEntityDistSq))
            return false;

        if (minEntityDistSq <= despawnR * despawnR)
            return false;

        for (Entity e : rs.entities) {
            if (e != INVALID_ENTITY && reg.hasComponent<TransformComponent>(e))
                reg.destroyEntity(e);
        }
        rs.entities.clear();
        if (events)
            events->notifyDespawn(entryId);
        rs.spawnRollAttempted = false;
        return true;
    }

    static bool pipelineContainsId(const SpawnRuntimeState& state, const std::string& id)
    {
        for (const PendingSpawnSlot& s : state.pendingSpawnPipeline) {
            if (s.entryId == id)
                return true;
        }
        return false;
    }

    static void pruneDeadEntities(Registry& reg, std::vector<Entity>& entities)
    {
        entities.erase(
            std::remove_if(
                entities.begin(),
                entities.end(),
                [&](Entity e) { return e == INVALID_ENTITY || !reg.hasComponent<TransformComponent>(e); }),
            entities.end());
    }

    static const SpawnEntryDesc* findEntryById(const SpawnCatalogData& catalog, const std::string& id)
    {
        for (const SpawnEntryDesc& e : catalog.spawns) {
            if (e.id == id)
                return &e;
        }
        return nullptr;
    }

    void advanceSpawnPipelineAndInstantiate(
        SpawnContext& ctx,
        const SpawnCatalogData& catalog,
        SpawnRuntimeState& state,
        SpawnFactoryRegistry& factories,
        SpawnEventBus* events,
        const Vec3& pp)
    {
        for (PendingSpawnSlot& slot : state.pendingSpawnPipeline) {
            if (slot.framesUntilInstantiate > 0)
                slot.framesUntilInstantiate--;
        }

        int budget = std::max(1, maxSpawnsPerFrame);

        for (auto it = state.pendingSpawnPipeline.begin(); it != state.pendingSpawnPipeline.end() && budget > 0;) {
            if (it->framesUntilInstantiate > 0) {
                ++it;
                continue;
            }

            const SpawnEntryDesc* entry = findEntryById(catalog, it->entryId);
            if (!entry) {
                it = state.pendingSpawnPipeline.erase(it);
                continue;
            }

            SpawnEntryRuntimeState& rs = state.bySpawnId[entry->id];
            if (!rs.entities.empty()) {
                it = state.pendingSpawnPipeline.erase(it);
                continue;
            }

            const float spawnR = entry->spawnRadius;
            const float anchorDx = pp.x - entry->position.x;
            const float anchorDz = pp.z - entry->position.z;
            if (anchorDx * anchorDx + anchorDz * anchorDz > spawnR * spawnR * 4.f) {
                rs.spawnRollAttempted = false;
                it = state.pendingSpawnPipeline.erase(it);
                --budget;
                continue;
            }

            ISpawnArchetypeFactory* fac = factories.tryGet(entry->archetype);
            if (!fac) {
                rs.spawnRollAttempted = false;
                it = state.pendingSpawnPipeline.erase(it);
                --budget;
                continue;
            }

            bool assetsReady = it->pendingAssetLoads.empty();
            if (!assetsReady) {
                assetsReady = true;
                for (const auto& sf : it->pendingAssetLoads) {
                    if (sf.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                        assetsReady = false;
                        break;
                    }
                }
            }
            if (!assetsReady) {
                ++it;
                continue;
            }

            SpawnRng rng(hashSpawnId(entry->id) ^ (static_cast<uint64_t>(catalog.worldSeed) * 0x100000001ULL));
            ctx.rng = &rng;
            SpawnResult res = fac->spawn(ctx, *entry);
            if (res.entities.empty()) {
                rs.spawnRollAttempted = false;
                it = state.pendingSpawnPipeline.erase(it);
                --budget;
                continue;
            }

            rs.entities = std::move(res.entities);
            rs.despawnRadiusCached = entry->despawnRadius;
            if (events)
                events->notifySpawn(entry->id, *entry, rs.entities);
            it = state.pendingSpawnPipeline.erase(it);
            --budget;
        }
    }
};

} // namespace spawn
