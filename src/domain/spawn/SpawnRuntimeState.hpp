#pragma once

#include "../../ecs/Entity.hpp"
#include "../../core/assets/IAsset.hpp"

#include <deque>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace spawn {

struct SpawnEntryRuntimeState {
    std::vector<Entity> entities;
    bool spawnRollAttempted = false;
    /// Copied from JSON when instances are created — used if this id disappears from the loaded catalog (chunk change).
    float despawnRadiusCached = 0.f;
};

/// Queued spawn: wait `framesUntilInstantiate` frames (0 = ready) before running the factory on the main thread.
struct PendingSpawnSlot {
    std::string entryId;
    int framesUntilInstantiate = 0;
    /// CPU decode for these paths runs in the background while warmup counts down; factory runs when all are ready.
    std::vector<std::shared_future<std::shared_ptr<IAsset>>> pendingAssetLoads;
};

struct SpawnRuntimeState {
    std::unordered_map<std::string, SpawnEntryRuntimeState> bySpawnId;
    std::deque<PendingSpawnSlot> pendingSpawnPipeline;
};

} // namespace spawn
