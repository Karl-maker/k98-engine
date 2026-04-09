#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../physics/CombatOverlap.hpp"
#include <cstdint>

/// Damage-receiving volume. Pairwise overlap with active `HitboxComponent` is reported by `HitHurtTriggerSystem`.
struct HurtboxComponent {
    CombatShape shape = CombatShape::Capsule;
    Vec3 halfExtents{0.35f, 0.9f, 0.35f};
    float radius = 0.35f;
    float capsuleHalfHeight = 0.45f;
    Vec3 offset{};

    Entity owner = INVALID_ENTITY;
    /// Bit flags — must overlap hitbox `validHurtLayersMask` for a hit to register.
    uint32_t layerBits = 1u << 0;

    bool active = true;
};
