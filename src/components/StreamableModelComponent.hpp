#pragma once

#include "../ecs/Entity.hpp"
#include <cstdint>
#include <string>
#include <vector>

enum class StreamableLoadState : std::uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Unloading
};

/// Requests async load of a model path when anchor enters load radius.
struct StreamableModelComponent {
    std::string modelPath;
    StreamableLoadState state = StreamableLoadState::Unloaded;
    /// All entities created for this spawn (for teardown). First entry is skeleton root.
    std::vector<Entity> spawnedEntities;
};
