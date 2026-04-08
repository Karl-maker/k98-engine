#pragma once

#include <cstdint>

// -----------------------------------------------------------------------------
// GameplayTags — empty marker components for queries (rendering, AI, tests).
// No runtime cost beyond presence in the signature bitmask.
//
// Registration (once at startup, before creating entities):
//   registry.registerComponent<PlayerTagComponent>();
//   registry.registerComponent<EnemyTagComponent>();
//
// Usage:
//   registry.addComponent(playerEntity, PlayerTagComponent{});
//   EnemyTagComponent tag{}; tag.slot = 2; registry.addComponent(enemyEntity, tag);
//
// Query:
//   for (Entity e : registry.getEntitiesWith<PlayerTagComponent, TransformComponent>()) { ... }
// Renderer passes use PlayerTag / EnemyTag to find drawables without hardcoded entity ids.
// -----------------------------------------------------------------------------

/// Marks the controllable / primary actor. At most one is typical for this demo.
struct PlayerTagComponent {};

/// Marks hostile or non-player pawns. `slot` is optional editor/debug index.
struct EnemyTagComponent {
    std::uint32_t slot = 0;
};
