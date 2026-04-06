#pragma once
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"
#include <cstdint>
#include <vector>

// -----------------------------------------------------------------------------
// CollisionBoxComponent — world-axis-aligned volume for broad-phase / queries.
// `halfSize` is user-authored (for spheres, use halfSize.x = radius and set `primitive`).
// `min`/`max` are updated by CollisionSystem from TransformComponent.position.
//
// `layer` — bitmask for “what I am”. `collidesWithMask` — which layers I test against.
// Pair tests when: (a.layer & b.collidesWithMask) && (b.layer & a.collidesWithMask).
//
// `blocksMovement` — if false, volume is a trigger (no SolidCollisionResponse push).
// Statics with blocksMovement=true stop dynamics from overlapping.
//
// Register: registry.registerComponent<CollisionBoxComponent>();
// Used with: TransformComponent + CollisionSystem + SpatialGridSystem.
// -----------------------------------------------------------------------------

enum class CollisionPrimitive : std::uint8_t {
    Aabb   = 0,
    Sphere = 1,
};

struct CollisionBoxComponent {
    /// Full axis lengths in world units: **width** (X), **height** (Y), **depth** (Z).
    /// If a component is **> 0**, that axis uses `halfSize = 0.5f * size` (exact dimensions).
    /// If **0**, the matching `halfSize` axis is left as manually set (legacy).
    Vec3 size{0.0f, 0.0f, 0.0f};

    /// Half-extents; updated from `size` each frame when authoring via `size` (see `applyAuthoringToHalfExtents`).
    Vec3 halfSize;

    // Cached bounds (for Aabb: from center ± halfSize; for Sphere: tight AABB around sphere)
    Vec3 min;
    Vec3 max;

    // Movement cache
    Vec3 lastPosition;

    // Current + previous collisions
    std::vector<Entity> touching;
    std::vector<Entity> previousTouching;

    uint32_t layer             = 1u;
    uint32_t collidesWithMask  = ~0u;
    bool     isStatic          = false;
    bool     blocksMovement    = true;
    CollisionPrimitive primitive = CollisionPrimitive::Aabb;

    /// Call before computing `min`/`max` from `halfSize` when using `size` authoring.
    void applyAuthoringToHalfExtents()
    {
        if (primitive == CollisionPrimitive::Sphere) {
            if (size.x > 0.0f) {
                const float r = size.x * 0.5f;
                halfSize      = Vec3{r, r, r};
            }
        } else {
            if (size.x > 0.0f)
                halfSize.x = size.x * 0.5f;
            if (size.y > 0.0f)
                halfSize.y = size.y * 0.5f;
            if (size.z > 0.0f)
                halfSize.z = size.z * 0.5f;
        }
    }

    float width() const { return halfSize.x * 2.0f; }
    float height() const { return halfSize.y * 2.0f; }
    float depth() const { return halfSize.z * 2.0f; }

    void setBoxSize(float width, float height, float depth)
    {
        size.x = width;
        size.y = height;
        size.z = depth;
        applyAuthoringToHalfExtents();
    }
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