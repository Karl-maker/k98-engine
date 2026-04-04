#pragma once
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"
#include <vector>

// -----------------------------------------------------------------------------
// CollisionBoxComponent — axis-aligned box collider. `halfSize` is user-authored;
// `min`/`max` are updated by CollisionSystem from TransformComponent.position.
// `layer` is a bitmask; pairs must share overlapping layers to collide.
// `lastPosition` seeds movement detection; use CollisionLastPositionSyncSystem::seed at load.
//
// Register: registry.registerComponent<CollisionBoxComponent>();
// Used with: TransformComponent + CollisionSystem + SpatialGridSystem.
// -----------------------------------------------------------------------------

struct CollisionBoxComponent {
    Vec3 halfSize;

    // Cached bounds
    Vec3 min;
    Vec3 max;

    // Movement cache
    Vec3 lastPosition;

    // Current + previous collisions
    std::vector<Entity> touching;
    std::vector<Entity> previousTouching;
    uint32_t layer{1};
    bool isStatic{false};
};

enum class CollisionEventType {
    Enter,
    Stay,
    Exit
};

// Fired to CollisionSystem handlers when a pair’s touching state changes.
struct CollisionEvent {
    Entity a;
    Entity b;
    CollisionEventType type;
};