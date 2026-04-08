#pragma once

#include <cstdint>

/// Bit flags for `ColliderFilterComponent::categoryBits` and ray `layerMask` filtering.
namespace CollisionLayer {
constexpr uint32_t Default = 1u << 0;
constexpr uint32_t Player = 1u << 1;
constexpr uint32_t Environment = 1u << 2;
constexpr uint32_t Prop = 1u << 3;
/// Hits colliders on any layer except the Player bit (line-of-sight from character).
constexpr uint32_t MaskAllButPlayer = 0xFFFFFFFFu ^ Player;
} // namespace CollisionLayer
