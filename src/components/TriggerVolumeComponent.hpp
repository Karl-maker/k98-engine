#pragma once

#include "../math/Vec3.hpp"
#include "../physics/CombatOverlap.hpp"
#include <cstdint>

/// Non-blocking overlap volume. Overlaps with other entities (by collider category mask) are reported each frame.
/// Does not add physics — use only `TransformComponent` + this component.
struct TriggerVolumeComponent {
    CombatShape shape = CombatShape::Box;
    Vec3 halfExtents{1.f, 1.f, 1.f};
    float radius = 1.f;
    float capsuleHalfHeight = 0.5f;
    Vec3 offset{};

    /// Overlap reported when `(overlapMask & otherColliderCategoryBits) != 0` (see `ColliderFilterComponent`).
    uint32_t overlapMask = 0xFFFFFFFFu;

    bool enabled = true;
};
