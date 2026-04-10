#pragma once

#include "../ecs/Registry.hpp"
#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../components/GameplayTags.hpp"
#include "../components/Position.hpp"
#include "../systems/TerrainChunkSystem.hpp"
#include "../systems/GravitySystem.hpp"
#include "../systems/CollisionBoundsSyncSystem.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/TerrainHeightField.hpp"

class AssetManager;
class IGraphicsRenderer;

// -----------------------------------------------------------------------------
// Owns procedural terrain streaming (TerrainChunkSystem) and the CPU heightfield
// mirror. Call updateStreaming once per frame before gameplay integration.
//
// snapGroundedActorsToSurface — one-time foot placement on support at startup
// (requires CollisionBoundsSyncSystem + grid before queries).
// -----------------------------------------------------------------------------

class TerrainEnvironmentSystem {
public:
    TerrainHeightField&       heightField() { return m_heights; }
    const TerrainHeightField& heightField() const { return m_heights; }

    void updateStreaming(
        Registry& registry,
        Vec3 viewerWorldPos,
        AssetManager* assets = nullptr,
        IGraphicsRenderer* renderer = nullptr)
    {
        m_chunks.update(registry, viewerWorldPos, &m_heights, nullptr, assets, renderer);
    }

    void snapGroundedActorsToSurface(Registry& registry, SpatialGridSystem& grid)
    {
        Vec3 viewer{0.0f, 0.0f, 0.0f};
        for (Entity e : registry.getEntitiesWith<PlayerTagComponent, Position>()) {
            viewer = registry.getComponent<Position>(e);
            break;
        }
        m_chunks.update(registry, viewer, &m_heights, nullptr, nullptr, nullptr);

        m_boundsSync.update(registry);
        grid.update(registry);
        GravitySystem::placeFeetOnSupportAll(registry, grid, &m_heights);
    }

private:
    TerrainChunkSystem         m_chunks;
    TerrainHeightField         m_heights;
    CollisionBoundsSyncSystem  m_boundsSync;
};