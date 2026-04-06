#pragma once

#include "../ecs/Registry.hpp"
#include "../ecs/Entity.hpp"
#include "../components/MassComponent.hpp"
#include "../components/Position.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/Velocity.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/TerrainHeightField.hpp"

#include <cmath>

// -----------------------------------------------------------------------------
// World gravity integration (Velocity) for bodies with MassComponent, plus
// terrain/solid support queries and penetration fix. `mass <= 0` skips gravity.
//
// Performance: TerrainHeightField O(1) sample; statics via grid.getNearby only.
// -----------------------------------------------------------------------------

class GravitySystem {
public:
    static constexpr float kDefaultGroundedEps = 0.12f;
    static constexpr float kSolidProbeHalfXZ   = 0.2f;
    static float computeSupportSurfaceY(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain,
        const Vec3& pos,
        const MassComponent& m)
    {
        float th = 0.0f;
        float best;
        const bool hasTerrain = terrain && terrain->trySampleHeight(pos.x, pos.z, th);
        if (hasTerrain)
            best = th;
        else
            best = m.fallbackGroundY;

        const float footY = pos.y - m.footOffset;

        const Vec3 probeMin{pos.x - kSolidProbeHalfXZ, pos.y - 6.0f, pos.z - kSolidProbeHalfXZ};
        const Vec3 probeMax{pos.x + kSolidProbeHalfXZ, pos.y + 2.0f, pos.z + kSolidProbeHalfXZ};

        const auto nearby = grid.getNearby(probeMin, probeMax);
        for (Entity s : nearby) {
            if (!registry.hasComponent<CollisionBoxComponent>(s))
                continue;
            auto& ob = registry.getComponent<CollisionBoxComponent>(s);
            if (!ob.isStatic || !ob.blocksMovement)
                continue;
            if ((m.solidGroundMask & ob.layer) == 0u)
                continue;

            if (pos.x < ob.min.x || pos.x > ob.max.x || pos.z < ob.min.z || pos.z > ob.max.z)
                continue;

            const float top = ob.max.y;
            if (footY >= top - kDefaultGroundedEps && footY <= top + 0.4f)
                best = std::max(best, top);
        }

        return best;
    }

    static float sampleGroundHeightAtXZ(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain,
        float worldX,
        float worldZ,
        float fallbackY,
        uint32_t solidLayerMask)
    {
        float th = 0.0f;
        float best;
        if (terrain && terrain->trySampleHeight(worldX, worldZ, th))
            best = th;
        else
            best = fallbackY;

        const Vec3 probeMin{worldX - kSolidProbeHalfXZ, -1.0e6f, worldZ - kSolidProbeHalfXZ};
        const Vec3 probeMax{worldX + kSolidProbeHalfXZ, 1.0e6f, worldZ + kSolidProbeHalfXZ};

        const auto nearby = grid.getNearby(probeMin, probeMax);
        for (Entity s : nearby) {
            if (!registry.hasComponent<CollisionBoxComponent>(s))
                continue;
            auto& ob = registry.getComponent<CollisionBoxComponent>(s);
            if (!ob.isStatic || !ob.blocksMovement)
                continue;
            if ((solidLayerMask & ob.layer) == 0u)
                continue;
            if (worldX < ob.min.x || worldX > ob.max.x || worldZ < ob.min.z || worldZ > ob.max.z)
                continue;
            best = std::max(best, ob.max.y);
        }
        return best;
    }

    static bool isGroundedForJump(
        Registry& registry,
        Entity e,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain,
        float groundedEps = kDefaultGroundedEps)
    {
        if (!registry.hasComponent<MassComponent>(e) || !registry.hasComponent<Position>(e) ||
            !registry.hasComponent<Velocity>(e))
            return false;
        const auto& pos = registry.getComponent<Position>(e);
        const auto& m   = registry.getComponent<MassComponent>(e);
        const auto& v   = registry.getComponent<Velocity>(e);

        const float surfaceY = computeSupportSurfaceY(registry, grid, terrain, pos, m);
        const float footY    = pos.y - m.footOffset;
        return footY <= surfaceY + groundedEps && v.y <= 0.0f;
    }

    /// `worldGravityY` negative (e.g. -28). Uses mass: heavier ⇒ same default accel;
    /// scale with `gravityScale`. `mass <= 0` skips.
    static void applyGravityForEntity(Registry& registry, Entity e, float worldGravityY, float dt)
    {
        if (!registry.hasComponent<Velocity>(e) || !registry.hasComponent<MassComponent>(e))
            return;
        auto& mass = registry.getComponent<MassComponent>(e);
        if (mass.mass <= 0.0f)
            return;
        auto& vel = registry.getComponent<Velocity>(e);
        vel.y += worldGravityY * mass.gravityScale * dt;
    }

    static void applyGravityAll(Registry& registry, float worldGravityY, float dt)
    {
        for (Entity e : registry.getEntitiesWith<Position, Velocity, MassComponent>())
            applyGravityForEntity(registry, e, worldGravityY, dt);
    }

    static void resolveGroundPenetration(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain,
        Entity e,
        const MassComponent& m)
    {
        if (!registry.hasComponent<Position>(e))
            return;
        auto& pos = registry.getComponent<Position>(e);

        const float surfaceY = computeSupportSurfaceY(registry, grid, terrain, pos, m);
        const float footY    = pos.y - m.footOffset;
        const float eps      = m.penetrationFixEps > 0.f ? m.penetrationFixEps : 1e-3f;

        if (footY < surfaceY - eps) {
            const float targetY = surfaceY + m.footOffset;
            pos.y               = targetY;
            if (registry.hasComponent<TransformComponent>(e))
                registry.getComponent<TransformComponent>(e).position.y = targetY;
            if (registry.hasComponent<Velocity>(e)) {
                auto& v = registry.getComponent<Velocity>(e);
                if (v.y < 0.0f)
                    v.y = 0.0f;
            }
        }
    }

    static void resolveGroundPenetrationAll(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain)
    {
        for (Entity e : registry.getEntitiesWith<Position, MassComponent>()) {
            const auto& m = registry.getComponent<MassComponent>(e);
            resolveGroundPenetration(registry, grid, terrain, e, m);
        }
    }

    static void placeFeetOnSupport(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain,
        Entity e,
        const MassComponent& m)
    {
        if (!registry.hasComponent<Position>(e))
            return;
        auto& pos     = registry.getComponent<Position>(e);
        const float y = computeSupportSurfaceY(registry, grid, terrain, pos, m) + m.footOffset;
        pos.y         = y;
        if (registry.hasComponent<TransformComponent>(e))
            registry.getComponent<TransformComponent>(e).position.y = y;
    }

    static void placeFeetOnSupportAll(
        Registry& registry,
        SpatialGridSystem& grid,
        const TerrainHeightField* terrain)
    {
        for (Entity e : registry.getEntitiesWith<Position, MassComponent>()) {
            const auto& m = registry.getComponent<MassComponent>(e);
            placeFeetOnSupport(registry, grid, terrain, e, m);
        }
    }
};
