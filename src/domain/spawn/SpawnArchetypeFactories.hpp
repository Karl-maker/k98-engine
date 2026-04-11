#pragma once

#include "ISpawnArchetypeFactory.hpp"
#include "SpawnCatalogSourceComponent.hpp"
#include "SpawnClusterLayout.hpp"
#include "SpawnFactoryRegistry.hpp"

#include "../components/ChaseComponent.hpp"
#include "../components/MovementComponent.hpp"

#include "../../components/BoxColliderComponent.hpp"
#include "../../components/CapsuleColliderComponent.hpp"
#include "../../components/ColliderFilterComponent.hpp"
#include "../../components/HealthComponent.hpp"
#include "../../components/PrimitiveBoxComponent.hpp"
#include "../../components/RigidBodyComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/WorldTransformComponent.hpp"
#include "../../ecs/Registry.hpp"
#include "../../game/factories/BusinessManSceneFactory.hpp"
#include "../../game/factories/CharacterCapsuleDefaults.hpp"
#include "../../physics/CollisionLayers.hpp"
#include "../../systems/PhysicsSystem.hpp"

#include <cmath>
#include <iostream>

namespace spawn {

namespace detail {

inline float attrFloat(const SpawnEntryDesc& e, const char* key, float defaultVal)
{
    if (!e.attributes.is_object() || !e.attributes.contains(key))
        return defaultVal;
    if (e.attributes[key].is_number())
        return static_cast<float>(e.attributes[key].get<double>());
    return defaultVal;
}

inline Entity spawnPropAt(
    Registry& registry,
    const Vec3& pos,
    const std::string& spawnId,
    const SpawnEntryDesc& entry)
{
    Entity ent = registry.createEntity();
    registry.addComponent(ent, TransformComponent{});
    registry.addComponent(ent, WorldTransformComponent{});
    auto& tf = registry.getComponent<TransformComponent>(ent);
    tf.position = pos;

    const float hx = attrFloat(entry, "boxHalfX", 0.25f);
    const float hy = attrFloat(entry, "boxHalfY", 0.25f);
    const float hz = attrFloat(entry, "boxHalfZ", 0.25f);

    RigidBodyComponent rb{};
    rb.mass = 0.f;
    rb.invMass = 0.f;
    rb.linearDamping = 4.f;
    rb.friction = 0.85f;
    registry.addComponent(ent, rb);

    BoxColliderComponent box{};
    box.halfExtents = {hx, hy, hz};
    box.offset = {0.f, 0.f, 0.f};
    registry.addComponent(ent, box);

    ColliderFilterComponent layers{};
    layers.categoryBits = CollisionLayer::Prop;
    layers.collideMask = 0xFFFFFFFFu;
    registry.addComponent(ent, layers);

    PrimitiveBoxComponent vis{};
    vis.halfExtents = box.halfExtents;
    vis.color[0] = 0.35f;
    vis.color[1] = 0.65f;
    vis.color[2] = 0.85f;
    registry.addComponent(ent, vis);

    SpawnCatalogSourceComponent src{};
    src.spawnEntryId = spawnId;
    registry.addComponent(ent, std::move(src));
    return ent;
}

inline Entity spawnHumanoidSkeleton(
    Registry& registry,
    const Vec3& pos,
    const std::string& spawnId,
    const SpawnEntryDesc& entry,
    Entity chaseTarget)
{
    Entity ent = registry.createEntity();
    registry.addComponent(ent, TransformComponent{});
    registry.addComponent(ent, WorldTransformComponent{});
    auto& tf = registry.getComponent<TransformComponent>(ent);
    tf.position = pos;

    RigidBodyComponent rb{};
    rb.mass = 1.f;
    rb.invMass = 1.f;
    rb.linearDamping = 2.8f;
    rb.friction = 0.38f;
    registry.addComponent(ent, rb);

    CapsuleColliderComponent cap{};
    cap.radius = game::factories::kManCapsuleRadius;
    cap.halfHeight = game::factories::kManCapsuleHalfHeight;
    cap.offset = {0.f, game::factories::kManCapsuleOffsetY, 0.f};
    registry.addComponent(ent, cap);

    ColliderFilterComponent layers{};
    layers.categoryBits = CollisionLayer::Default;
    layers.collideMask = 0xFFFFFFFFu;
    registry.addComponent(ent, layers);

    MovementComponent move{};
    move.walkSpeed = attrFloat(entry, "walkSpeed", 2.f);
    move.runSpeed = attrFloat(entry, "runSpeed", 4.f);
    move.sprintSpeed = attrFloat(entry, "sprintSpeed", 5.f);
    registry.addComponent(ent, move);

    HealthComponent hp{};
    hp.max = attrFloat(entry, "health", 100.f);
    hp.current = hp.max;
    registry.addComponent(ent, hp);

    if (chaseTarget != INVALID_ENTITY) {
        ChaseComponent ch{};
        ch.chaseTarget = chaseTarget;
        registry.addComponent(ent, ch);
    }

    SpawnCatalogSourceComponent src{};
    src.spawnEntryId = spawnId;
    registry.addComponent(ent, std::move(src));
    return ent;
}

} // namespace detail

class PropSpawnFactory final : public ISpawnArchetypeFactory {
public:
    SpawnResult spawn(SpawnContext& ctx, const SpawnEntryDesc& entry) override
    {
        if (!ctx.registry)
            return {};
        auto positions = clusterWorldPositions(entry.position, entry.cluster);
        std::vector<Entity> out;
        out.reserve(positions.size());
        for (const Vec3& p : positions)
            out.push_back(detail::spawnPropAt(*ctx.registry, p, entry.id, entry));
        return ok(std::move(out));
    }
};

class NpcSpawnFactory final : public ISpawnArchetypeFactory {
public:
    SpawnResult spawn(SpawnContext& ctx, const SpawnEntryDesc& entry) override
    {
        if (!ctx.registry)
            return {};
        auto positions = clusterWorldPositions(entry.position, entry.cluster);
        std::vector<Entity> out;
        out.reserve(positions.size());
        for (const Vec3& p : positions)
            out.push_back(detail::spawnHumanoidSkeleton(*ctx.registry, p, entry.id, entry, INVALID_ENTITY));
        return ok(std::move(out));
    }
};

class EnemySpawnFactory final : public ISpawnArchetypeFactory {
public:
    void collectPrefetchAssetPaths(SpawnContext& ctx, const SpawnEntryDesc& entry, std::vector<std::string>& out) override
    {
        (void)entry;
        if (!ctx.sharedCharacterGltfPath.empty())
            out.push_back(ctx.sharedCharacterGltfPath);
    }

    SpawnResult spawn(SpawnContext& ctx, const SpawnEntryDesc& entry) override
    {
        if (!ctx.registry || ctx.playerEntity == INVALID_ENTITY) {
            std::cerr << "EnemySpawnFactory: missing registry or player entity\n";
            return {};
        }
        auto positions = clusterWorldPositions(entry.position, entry.cluster);
        std::vector<Entity> out;
        out.reserve(positions.size());

        if (!ctx.sharedCharacterGltfPath.empty() && ctx.assets && ctx.renderer) {
            for (const Vec3& p : positions) {
                const float yawDeg = detail::attrFloat(entry, "yawDegrees", 0.f);
                const float yawRad = yawDeg * 3.14159265f / 180.f;
                Entity e = game::factories::spawnBusinessManCharacter(
                    *ctx.registry,
                    *ctx.renderer,
                    *ctx.assets,
                    ctx.sharedCharacterGltfPath,
                    p,
                    yawRad,
                    false,
                    CollisionLayer::Default);
                if (e == INVALID_ENTITY) {
                    std::cerr << "EnemySpawnFactory: failed glTF spawn for \"" << entry.id << "\"\n";
                    continue;
                }
                float groundY = p.y;
                if (ctx.terrainHeights && ctx.terrainHeights->trySampleHeight(p.x, p.z, groundY)) {
                    auto& tf = ctx.registry->getComponent<TransformComponent>(e);
                    tf.position.y = groundY + kTerrainFootClearance - game::factories::kManCapsuleOffsetY +
                        game::factories::kManCapsuleHalfHeight + game::factories::kManCapsuleRadius;
                }
                MovementComponent move{};
                move.walkSpeed = detail::attrFloat(entry, "walkSpeed", 2.f);
                move.runSpeed = detail::attrFloat(entry, "runSpeed", 4.f);
                move.sprintSpeed = detail::attrFloat(entry, "sprintSpeed", 5.f);
                move.acceleration = detail::attrFloat(entry, "acceleration", 14.f);
                ctx.registry->addComponent(e, move);
                ChaseComponent chase{};
                chase.chaseTarget = ctx.playerEntity;
                ctx.registry->addComponent(e, chase);
                HealthComponent hp{};
                hp.max = detail::attrFloat(entry, "health", 100.f);
                hp.current = hp.max;
                ctx.registry->addComponent(e, hp);
                SpawnCatalogSourceComponent src{};
                src.spawnEntryId = entry.id;
                ctx.registry->addComponent(e, std::move(src));
                out.push_back(e);
            }
            if (!out.empty())
                return ok(std::move(out));
        }

        for (const Vec3& p : positions)
            out.push_back(detail::spawnHumanoidSkeleton(*ctx.registry, p, entry.id, entry, ctx.playerEntity));
        return ok(std::move(out));
    }
};

class AnimalSpawnFactory final : public ISpawnArchetypeFactory {
public:
    SpawnResult spawn(SpawnContext& ctx, const SpawnEntryDesc& entry) override
    {
        if (!ctx.registry)
            return {};
        auto positions = clusterWorldPositions(entry.position, entry.cluster);
        std::vector<Entity> out;
        out.reserve(positions.size());
        for (const Vec3& p : positions)
            out.push_back(detail::spawnHumanoidSkeleton(*ctx.registry, p, entry.id, entry, INVALID_ENTITY));
        return ok(std::move(out));
    }
};

/// Ready-made factories (`PropSpawnFactory`, `EnemySpawnFactory`, etc.) are optional — the game registers
/// whichever archetypes it needs via `SpawnCatalogGridSystem::factories().registerArchetype(name, ...)`.

} // namespace spawn
