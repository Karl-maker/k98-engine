#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../components/Position.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/Velocity.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/CollisionUtils.hpp"

#include <cmath>
#include <vector>

// -----------------------------------------------------------------------------
// Pushes non-static colliders out of static `blocksMovement` volumes using the
// smallest axis MTV (AABB vs AABB) or sphere-vs-AABB penetration. Run after
// CollisionSystem (so min/max are current) and after PositionToTransform.
//
// Clears inward velocity along the separation direction when significant.
// -----------------------------------------------------------------------------

class SolidCollisionResponseSystem {
public:
    void update(Registry& registry)
    {
        std::vector<Entity> staticSolids;
        for (Entity s : registry.getEntitiesWith<CollisionBoxComponent, TransformComponent>()) {
            auto& b = registry.getComponent<CollisionBoxComponent>(s);
            if (b.isStatic && b.blocksMovement)
                staticSolids.push_back(s);
        }
        if (staticSolids.empty())
            return;

        for (Entity e : registry.getEntitiesWith<Position, TransformComponent, CollisionBoxComponent>()) {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);
            if (box.isStatic || !box.blocksMovement)
                continue;

            auto& pos = registry.getComponent<Position>(e);
            auto& tf  = registry.getComponent<TransformComponent>(e);

            for (int pass = 0; pass < 4; ++pass) {
                syncBoundsFromTransform(box, tf.position);

                bool any = false;
                for (Entity sEnt : staticSolids) {
                    if (sEnt == e)
                        continue;
                    auto& ob = registry.getComponent<CollisionBoxComponent>(sEnt);
                    if ((box.layer & ob.collidesWithMask) == 0u || (ob.layer & box.collidesWithMask) == 0u)
                        continue;

                    if (!CollisionUtils::aabbAabb(box.min, box.max, ob.min, ob.max))
                        continue;

                    Vec3 mtv{};
                    if (box.primitive == CollisionPrimitive::Sphere) {
                        const float r =
                            box.halfSize.x > 1e-6f ? box.halfSize.x : 0.5f;
                        mtv = CollisionUtils::mtvSphereSeparateFromAabb(tf.position, r, ob.min, ob.max);
                    } else {
                        mtv = CollisionUtils::mtvAabbSeparateAFromB(box.min, box.max, ob.min, ob.max);
                    }

                    const float m2 = mtv.x * mtv.x + mtv.y * mtv.y + mtv.z * mtv.z;
                    if (m2 < 1e-10f)
                        continue;

                    pos.x += mtv.x;
                    pos.y += mtv.y;
                    pos.z += mtv.z;
                    tf.position.x += mtv.x;
                    tf.position.y += mtv.y;
                    tf.position.z += mtv.z;

                    if (registry.hasComponent<Velocity>(e)) {
                        auto& v  = registry.getComponent<Velocity>(e);
                        const float invLen = 1.0f / std::sqrt(m2);
                        const float nx     = mtv.x * invLen;
                        const float ny     = mtv.y * invLen;
                        const float nz     = mtv.z * invLen;
                        const float vn     = v.x * nx + v.y * ny + v.z * nz;
                        if (vn < 0.f) {
                            v.x -= vn * nx;
                            v.y -= vn * ny;
                            v.z -= vn * nz;
                        }
                    }
                    any = true;
                    break;
                }
                if (!any)
                    break;
            }
        }
    }

private:
    static void syncBoundsFromTransform(CollisionBoxComponent& box, const Vec3& c)
    {
        box.applyAuthoringToHalfExtents();
        if (box.primitive == CollisionPrimitive::Sphere) {
            const float r = box.halfSize.x > 1e-6f ? box.halfSize.x : 0.5f;
            box.min = c - Vec3{r, r, r};
            box.max = c + Vec3{r, r, r};
        } else {
            box.min = c - box.halfSize;
            box.max = c + box.halfSize;
        }
    }
};
