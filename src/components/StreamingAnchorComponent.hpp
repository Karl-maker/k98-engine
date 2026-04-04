#pragma once

#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"

// -----------------------------------------------------------------------------
// StreamingAnchorComponent — viewer-relative streaming volume. Update
// `worldPosition` each frame (e.g. from player transform). `viewerEntity` is the
// entity whose streaming cache should be considered (optional, service-specific).
//
// Register: registry.registerComponent<StreamingAnchorComponent>();
// Used with StreamingLoadService + StreamableModelComponent.
// -----------------------------------------------------------------------------

struct StreamingAnchorComponent {
    Vec3 worldPosition{};
    float loadRadius = 80.f;
    float unloadRadius = 120.f;
    Entity viewerEntity = INVALID_ENTITY;
};
