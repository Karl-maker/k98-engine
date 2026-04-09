#pragma once

#include "../../components/BoxColliderComponent.hpp"
#include "../../components/CapsuleColliderComponent.hpp"
#include "../../components/ColliderFilterComponent.hpp"
#include "../../components/HitboxComponent.hpp"
#include "../../components/HurtboxComponent.hpp"
#include "../../components/SphereColliderComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/TriggerVolumeComponent.hpp"
#include "../../ecs/Entity.hpp"
#include "../../ecs/Registry.hpp"
#include "../../math/Vec3.hpp"
#include "../../physics/CombatOverlap.hpp"
#include "../../physics/SpatialGrid.hpp"

#include <cstdint>
#include <unordered_set>
#include <vector>

struct HitEvent {
    Entity hitboxEntity = INVALID_ENTITY;
    Entity hurtboxEntity = INVALID_ENTITY;
    Entity attacker = INVALID_ENTITY;
    Entity victim = INVALID_ENTITY;
    int damage = 0;
};

struct TriggerOverlapEvent {
    Entity triggerEntity = INVALID_ENTITY;
    Entity otherEntity = INVALID_ENTITY;
};

/// Resolves hitbox↔hurtbox overlaps (combat) and trigger↔entity overlaps (non-blocking). Uses the same axis-aligned
/// bounds style as physics colliders. Runs after transforms are updated for the frame.
class HitHurtTriggerSystem {
public:
    const std::vector<HitEvent>& hitEvents() const { return m_hits; }
    const std::vector<TriggerOverlapEvent>& triggerEvents() const { return m_triggers; }

    void update(Registry& registry, SpatialGrid& grid)
    {
        m_hits.clear();
        m_triggers.clear();

        hitHurtPass(registry, grid);
        triggerPass(registry, grid);
    }

private:
    static uint32_t colliderCategory(Registry& registry, Entity e)
    {
        if (registry.hasComponent<ColliderFilterComponent>(e))
            return registry.getComponent<ColliderFilterComponent>(e).categoryBits;
        return 0xFFFFFFFFu;
    }

    static bool entityCombatAABB(Registry& registry, Entity e, Vec3& outMin, Vec3& outMax)
    {
        if (!registry.hasComponent<TransformComponent>(e))
            return false;
        const auto& t = registry.getComponent<TransformComponent>(e);

        if (registry.hasComponent<HurtboxComponent>(e)) {
            const auto& h = registry.getComponent<HurtboxComponent>(e);
            combatVolumeToAABB(t, h.offset, h.shape, h.halfExtents, h.radius, h.capsuleHalfHeight, outMin, outMax);
            return true;
        }
        if (registry.hasComponent<BoxColliderComponent>(e)) {
            const auto& b = registry.getComponent<BoxColliderComponent>(e);
            outMin = {t.position.x + b.offset.x - b.halfExtents.x, t.position.y + b.offset.y - b.halfExtents.y,
                t.position.z + b.offset.z - b.halfExtents.z};
            outMax = {t.position.x + b.offset.x + b.halfExtents.x, t.position.y + b.offset.y + b.halfExtents.y,
                t.position.z + b.offset.z + b.halfExtents.z};
            return true;
        }
        if (registry.hasComponent<CapsuleColliderComponent>(e)) {
            const auto& c = registry.getComponent<CapsuleColliderComponent>(e);
            const Vec3 cc{t.position.x + c.offset.x, t.position.y + c.offset.y, t.position.z + c.offset.z};
            const float r = c.radius;
            const float hh = c.halfHeight;
            outMin = {cc.x - r, cc.y - hh - r, cc.z - r};
            outMax = {cc.x + r, cc.y + hh + r, cc.z + r};
            return true;
        }
        if (registry.hasComponent<SphereColliderComponent>(e)) {
            const auto& s = registry.getComponent<SphereColliderComponent>(e);
            const Vec3 c{t.position.x + s.offset.x, t.position.y + s.offset.y, t.position.z + s.offset.z};
            outMin = {c.x - s.radius, c.y - s.radius, c.z - s.radius};
            outMax = {c.x + s.radius, c.y + s.radius, c.z + s.radius};
            return true;
        }
        return false;
    }

    void hitHurtPass(Registry& registry, SpatialGrid& grid)
    {
        for (Entity he : registry.getEntitiesWith<HitboxComponent, TransformComponent>()) {
            auto& hb = registry.getComponent<HitboxComponent>(he);
            if (!hb.active)
                continue;
            const auto& ht = registry.getComponent<TransformComponent>(he);
            Vec3 hMin{};
            Vec3 hMax{};
            combatVolumeToAABB(ht, hb.offset, hb.shape, hb.halfExtents, hb.radius, hb.capsuleHalfHeight, hMin, hMax);

            const Vec3 center{
                (hMin.x + hMax.x) * 0.5f,
                (hMin.y + hMax.y) * 0.5f,
                (hMin.z + hMax.z) * 0.5f,
            };
            const std::vector<Entity> nearby = grid.queryNearby(center);

            std::unordered_set<Entity> seen;
            for (Entity n : nearby) {
                if (!registry.hasComponent<HurtboxComponent>(n))
                    continue;
                seen.insert(n);
            }

            for (Entity victimE : seen) {
                if (victimE == he)
                    continue;
                auto& hurt = registry.getComponent<HurtboxComponent>(victimE);
                if (!hurt.active)
                    continue;
                if (hb.skipSameOwner && hb.owner != INVALID_ENTITY && hb.owner == hurt.owner)
                    continue;
                if ((hb.validHurtLayersMask & hurt.layerBits) == 0u)
                    continue;

                const auto& vt = registry.getComponent<TransformComponent>(victimE);
                Vec3 vMin{};
                Vec3 vMax{};
                combatVolumeToAABB(vt, hurt.offset, hurt.shape, hurt.halfExtents, hurt.radius, hurt.capsuleHalfHeight, vMin, vMax);

                if (!aabbOverlapsAABB(hMin, hMax, vMin, vMax))
                    continue;

                HitEvent ev{};
                ev.hitboxEntity = he;
                ev.hurtboxEntity = victimE;
                ev.attacker = hb.owner != INVALID_ENTITY ? hb.owner : he;
                ev.victim = hurt.owner != INVALID_ENTITY ? hurt.owner : victimE;
                ev.damage = hb.damage;
                m_hits.push_back(ev);
            }
        }
    }

    void triggerPass(Registry& registry, SpatialGrid& grid)
    {
        for (Entity te : registry.getEntitiesWith<TriggerVolumeComponent, TransformComponent>()) {
            auto& tv = registry.getComponent<TriggerVolumeComponent>(te);
            if (!tv.enabled)
                continue;
            const auto& tt = registry.getComponent<TransformComponent>(te);
            Vec3 tMin{};
            Vec3 tMax{};
            combatVolumeToAABB(tt, tv.offset, tv.shape, tv.halfExtents, tv.radius, tv.capsuleHalfHeight, tMin, tMax);

            const Vec3 center{
                (tMin.x + tMax.x) * 0.5f,
                (tMin.y + tMax.y) * 0.5f,
                (tMin.z + tMax.z) * 0.5f,
            };
            const std::vector<Entity> nearby = grid.queryNearby(center);

            for (Entity other : nearby) {
                if (other == te)
                    continue;
                const uint32_t cat = colliderCategory(registry, other);
                if ((tv.overlapMask & cat) == 0u)
                    continue;

                Vec3 oMin{};
                Vec3 oMax{};
                if (!entityCombatAABB(registry, other, oMin, oMax))
                    continue;

                if (!aabbOverlapsAABB(tMin, tMax, oMin, oMax))
                    continue;

                TriggerOverlapEvent ev{};
                ev.triggerEntity = te;
                ev.otherEntity = other;
                m_triggers.push_back(ev);
            }
        }
    }

    std::vector<HitEvent> m_hits;
    std::vector<TriggerOverlapEvent> m_triggers;
};
