#pragma once
#include "../math/Mat4.hpp"

// -----------------------------------------------------------------------------
// WorldTransformComponent — cached world matrix for an entity. Filled by
// TransformSystem (hierarchy), BoneSyncSystem (bones), or custom writers.
// `dirty` is for consumers that want to skip work when unchanged.
//
// Register: registry.registerComponent<WorldTransformComponent>();
// Query with TransformComponent on the same entity for normal actors.
// -----------------------------------------------------------------------------

struct WorldTransformComponent {
    Mat4 world;
    bool dirty{true};
};