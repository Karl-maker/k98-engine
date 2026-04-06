#pragma once

#include <cstdint>

// -----------------------------------------------------------------------------
// Suggested layer bits for CollisionBoxComponent::layer / collidesWithMask.
// Games pick subsets; pairs collide when (A.layer & B.collidesWithMask) &&
// (B.layer & A.collidesWithMask).
// -----------------------------------------------------------------------------

namespace CollisionLayers {
constexpr uint32_t Player = 1u << 0;
constexpr uint32_t Enemy  = 1u << 1;
constexpr uint32_t Static = 1u << 2;
constexpr uint32_t All    = 0xFFFFFFFFu;
} // namespace CollisionLayers
