#pragma once

#include "../ecs/Entity.hpp"
#include <cstdint>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// StreamableModelComponent — async model load request. `modelPath` is loaded when
// StreamingLoadService decides the anchor is in range; `spawnedEntities` holds
// created entities (root first) for teardown on unload.
//
// Register: registry.registerComponent<StreamableModelComponent>();
// Works with StreamingAnchorComponent + StreamingLoadService (see that service).
// -----------------------------------------------------------------------------

enum class StreamableLoadState : std::uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Unloading
};

struct StreamableModelComponent {
    std::string modelPath;
    StreamableLoadState state = StreamableLoadState::Unloaded;
    /// All entities created for this spawn (for teardown). First entry is skeleton root.
    std::vector<Entity> spawnedEntities;
};
