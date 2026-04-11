#pragma once

#include "SpawnContext.hpp"
#include "SpawnTypes.hpp"

#include <string>
#include <vector>

namespace spawn {

class ISpawnArchetypeFactory {
public:
    virtual ~ISpawnArchetypeFactory() = default;
    virtual SpawnResult spawn(SpawnContext& ctx, const SpawnEntryDesc& entry) = 0;

    /// Asset paths to start loading when a spawn is queued (CPU decode off-thread). Empty = no wait.
    virtual void collectPrefetchAssetPaths(SpawnContext& ctx, const SpawnEntryDesc& entry, std::vector<std::string>& out)
    {
        (void)ctx;
        (void)entry;
        (void)out;
    }
};

} // namespace spawn
