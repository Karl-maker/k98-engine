#pragma once

#include "SpawnRng.hpp"

#include "../../ecs/Entity.hpp"

#include <string>

class Registry;
class AssetManager;
class IGraphicsRenderer;
class TerrainHeightField;

namespace spawn {

/// Passed to archetype factories — extend with game state as needed.
struct SpawnContext {
    Registry* registry = nullptr;
    AssetManager* assets = nullptr;
    IGraphicsRenderer* renderer = nullptr;
    Entity playerEntity = INVALID_ENTITY;
    TerrainHeightField* terrainHeights = nullptr;
    uint32_t worldSeed = 0;
    /// Seeded for this entry + instance index — factories use for attribute probability.
    SpawnRng* rng = nullptr;
    /// When set (e.g. same path as the player), `EnemySpawnFactory` can spawn rigged glTF characters instead of capsules.
    std::string sharedCharacterGltfPath{};
};

} // namespace spawn
