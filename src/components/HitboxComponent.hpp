#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../physics/CombatOverlap.hpp"
#include <cstdint>

/// Offensive volume (attack). Does not participate in physics blocking — overlap is reported by `HitHurtTriggerSystem`.
struct HitboxComponent {
    CombatShape shape = CombatShape::Box;
    Vec3 halfExtents{0.25f, 0.25f, 0.25f};
    float radius = 0.25f;
    float capsuleHalfHeight = 0.35f;
    Vec3 offset{};

    /// Entity that owns this attack (e.g. character). Used for team / same-owner checks.
    Entity owner = INVALID_ENTITY;
    /// If true, does not generate hits when `owner` matches the hurtbox owner.
    bool skipSameOwner = true;

    /// Hit connects when `(validHurtLayersMask & hurt.layerBits) != 0`.
    uint32_t validHurtLayersMask = 0xFFFFFFFFu;

    bool active = true;
    int damage = 1;
};
