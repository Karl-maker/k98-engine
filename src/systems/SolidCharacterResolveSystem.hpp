#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../components/Position.hpp"
#include "../components/TransformComponent.hpp"
#include "../systems/SpacialGridSystem.hpp"

#include <algorithm>
#include <cmath>

// -----------------------------------------------------------------------------
// Depenetrates dynamic AABBs (Position + halfSize) from static solid volumes in XZ.
// Vertical separation is left to GravitySystem so characters can stand on platforms.
// Run after velocity integration, before GravitySystem, with a current SpatialGrid
// (statics use previous-frame box bounds; statics do not move).
// -----------------------------------------------------------------------------

class SolidCharacterResolveSystem {
public:
    void update(Registry& registry, SpatialGridSystem& grid) const
    {
        auto movers = registry.getEntitiesWith<Position, CollisionBoxComponent, TransformComponent>();

        for (Entity e : movers)
        {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);
            if (box.isStatic)
                continue;

            auto& pos = registry.getComponent<Position>(e);
            resolveAgainstStatics(registry, grid, pos, box.halfSize);
        }
    }

private:
    static void resolveAgainstStatics(
        Registry& registry,
        SpatialGridSystem& grid,
        Vec3& pos,
        const Vec3& half)
    {
        const Vec3 cmin{pos.x - half.x, pos.y - half.y, pos.z - half.z};
        const Vec3 cmax{pos.x + half.x, pos.y + half.y, pos.z + half.z};

        auto nearby = grid.getNearby(cmin, cmax);

        for (Entity s : nearby)
        {
            if (!registry.hasComponent<CollisionBoxComponent>(s))
                continue;
            const auto& sb = registry.getComponent<CollisionBoxComponent>(s);
            if (!sb.isStatic || !sb.solid)
                continue;

            const float cminy = pos.y - half.y;
            const bool  onTop = cminy >= sb.max.y - 0.12f;
            if (onTop)
                continue;

            const float oxp = std::min(cmax.x, sb.max.x) - std::max(cmin.x, sb.min.x);
            const float ozp = std::min(cmax.z, sb.max.z) - std::max(cmin.z, sb.min.z);
            if (oxp <= 0.0f || ozp <= 0.0f)
                continue;

            const float oyp = std::min(cmax.y, sb.max.y) - std::max(cmin.y, sb.min.y);
            if (oyp <= 0.0f)
                continue;

            const float sx = (sb.min.x + sb.max.x) * 0.5f;
            const float sz = (sb.min.z + sb.max.z) * 0.5f;

            if (oxp < ozp)
            {
                const float push = oxp;
                pos.x += (pos.x < sx) ? -push : push;
            }
            else
            {
                const float push = ozp;
                pos.z += (pos.z < sz) ? -push : push;
            }
        }
    }
};
