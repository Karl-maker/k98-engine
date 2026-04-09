#pragma once

/// Y-up character capsule used for business/man rig physics, spawn height, and combat hurtboxes.
namespace game::factories {
inline constexpr float kManCapsuleRadius = 0.18f;
inline constexpr float kManCapsuleHalfHeight = 0.34f;
inline constexpr float kManCapsuleOffsetY = 0.74f;
/// Slightly larger than physics capsule for `HurtboxComponent` (skin).
inline constexpr float kManHurtboxRadius = 0.2f;
inline constexpr float kManHurtboxHalfHeight = 0.36f;
} // namespace game::factories
