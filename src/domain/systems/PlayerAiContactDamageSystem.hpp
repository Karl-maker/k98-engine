#pragma once

#include "../../components/CapsuleColliderComponent.hpp"
#include "../../components/HealthComponent.hpp"
#include "../../components/RigidBodyComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../ecs/Entity.hpp"
#include "../../ecs/Registry.hpp"
#include "../../physics/CombatOverlap.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

struct ContactDamageEvent {
    Entity victim = INVALID_ENTITY;
    Entity attacker = INVALID_ENTITY;
    float damage = 0.f;
};

/// When the AI capsule overlaps the player capsule and horizontal speed exceeds `minAttackerSpeed`,
/// applies damage once per cooldown and records a `ContactDamageEvent`.
class PlayerAiContactDamageSystem {
public:
    const std::vector<ContactDamageEvent>& events() const { return m_events; }

    void update(
        Registry& registry,
        Entity player,
        Entity ai,
        float dt,
        float damagePerContact,
        float minAttackerSpeed,
        float cooldownSeconds)
    {
        m_events.clear();
        if (player == INVALID_ENTITY || ai == INVALID_ENTITY)
            return;
        if (!registry.hasComponent<HealthComponent>(player) || !registry.hasComponent<TransformComponent>(player) ||
            !registry.hasComponent<CapsuleColliderComponent>(player))
            return;
        if (!registry.hasComponent<TransformComponent>(ai) || !registry.hasComponent<CapsuleColliderComponent>(ai) ||
            !registry.hasComponent<RigidBodyComponent>(ai))
            return;

        auto& hp = registry.getComponent<HealthComponent>(player);
        hp.contactHitCooldown = std::max(0.f, hp.contactHitCooldown - dt);
        if (hp.current <= 0.f)
            return;

        Vec3 pMin{};
        Vec3 pMax{};
        Vec3 aMin{};
        Vec3 aMax{};
        capsuleWorldAABB(registry, player, pMin, pMax);
        capsuleWorldAABB(registry, ai, aMin, aMax);

        if (!aabbOverlapsAABB(pMin, pMax, aMin, aMax))
            return;

        const auto& aiBody = registry.getComponent<RigidBodyComponent>(ai);
        const float vx = aiBody.velocity.x;
        const float vz = aiBody.velocity.z;
        const float horizSpeed = std::sqrt(vx * vx + vz * vz);
        if (horizSpeed < minAttackerSpeed)
            return;

        if (hp.contactHitCooldown > 0.f)
            return;

        hp.current -= damagePerContact;
        if (hp.current < 0.f)
            hp.current = 0.f;
        hp.contactHitCooldown = cooldownSeconds;

        ContactDamageEvent ev{};
        ev.victim = player;
        ev.attacker = ai;
        ev.damage = damagePerContact;
        m_events.push_back(ev);
    }

private:
    static void capsuleWorldAABB(Registry& registry, Entity e, Vec3& outMin, Vec3& outMax)
    {
        const auto& t = registry.getComponent<TransformComponent>(e);
        const auto& c = registry.getComponent<CapsuleColliderComponent>(e);
        const Vec3 cc{t.position.x + c.offset.x, t.position.y + c.offset.y, t.position.z + c.offset.z};
        const float r = c.radius;
        const float hh = c.halfHeight;
        outMin = {cc.x - r, cc.y - hh - r, cc.z - r};
        outMax = {cc.x + r, cc.y + hh + r, cc.z + r};
    }

    std::vector<ContactDamageEvent> m_events;
};
