#pragma once

#include "../components/BoxColliderComponent.hpp"
#include "../components/CapsuleColliderComponent.hpp"
#include "../components/RigidBodyComponent.hpp"
#include "../components/SphereColliderComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../math/Vec3.hpp"
#include "../physics/Collision.hpp"
#include "../physics/CollisionFilter.hpp"
#include "../physics/SpatialGrid.hpp"
#include "../utils/TerrainHeightField.hpp"
#include <cmath>
#include <vector>

/// Added to sampled terrain height so rig feet clear the ground mesh (capsule vs visual).
inline constexpr float kTerrainFootClearance = 0.16f;

class PhysicsSystem {
public:
    Vec3 gravity{0.f, -9.81f, 0.f};
    SpatialGrid grid;
    /// When false, skips sinusoidal terrain grounding (use static colliders only).
    bool useProceduralTerrainGround = true;
    /// When set, `trySampleHeight` is used before the legacy sinusoidal fallback (streaming terrain).
    TerrainHeightField* terrainHeightField = nullptr;

    void update(Registry& registry, float dt)
    {
        integrate(registry, dt);
        buildSpatialGrid(registry);

        auto entities = registry.getEntitiesWith<TransformComponent, RigidBodyComponent>();

        for (auto e : entities) {
            auto& t = registry.getComponent<TransformComponent>(e);
            auto& body = registry.getComponent<RigidBodyComponent>(e);

            if (body.invMass <= 0.f)
                continue;

            const std::vector<Entity> nearby = grid.queryNearby(t.position);

            for (Entity other : nearby) {
                if (other == e)
                    continue;
                if (!registry.hasComponent<TransformComponent>(other) || !registry.hasComponent<RigidBodyComponent>(other))
                    continue;

                auto& tB = registry.getComponent<TransformComponent>(other);
                auto& bodyB = registry.getComponent<RigidBodyComponent>(other);

                // Dynamic–dynamic pairs were resolved twice (once per entity as `e`). Static bodies never
                // enter the outer loop, so static–dynamic is already single-sided.
                if (body.invMass > 0.f && bodyB.invMass > 0.f && other < e)
                    continue;

                if (!entitiesCollideByLayer(registry, e, other))
                    continue;

                Vec3 normal{};
                float penetration = 0.f;

                if (registry.hasComponent<SphereColliderComponent>(e) && registry.hasComponent<SphereColliderComponent>(other)) {
                    const auto& aCol = registry.getComponent<SphereColliderComponent>(e);
                    const auto& bCol = registry.getComponent<SphereColliderComponent>(other);
                    const Vec3 aPos{t.position.x + aCol.offset.x, t.position.y + aCol.offset.y, t.position.z + aCol.offset.z};
                    const Vec3 bPos{tB.position.x + bCol.offset.x, tB.position.y + bCol.offset.y, tB.position.z + bCol.offset.z};
                    if (sphereVsSphere(aPos, aCol.radius, bPos, bCol.radius, normal, penetration))
                        resolveCollision(body, bodyB, t, tB, normal, penetration);
                }

                if (registry.hasComponent<BoxColliderComponent>(e) && registry.hasComponent<BoxColliderComponent>(other)) {
                    const auto& aCol = registry.getComponent<BoxColliderComponent>(e);
                    const auto& bCol = registry.getComponent<BoxColliderComponent>(other);

                    const Vec3 aMin{t.position.x + aCol.offset.x - aCol.halfExtents.x,
                        t.position.y + aCol.offset.y - aCol.halfExtents.y,
                        t.position.z + aCol.offset.z - aCol.halfExtents.z};
                    const Vec3 aMax{t.position.x + aCol.offset.x + aCol.halfExtents.x,
                        t.position.y + aCol.offset.y + aCol.halfExtents.y,
                        t.position.z + aCol.offset.z + aCol.halfExtents.z};
                    const Vec3 bMin{tB.position.x + bCol.offset.x - bCol.halfExtents.x,
                        tB.position.y + bCol.offset.y - bCol.halfExtents.y,
                        tB.position.z + bCol.offset.z - bCol.halfExtents.z};
                    const Vec3 bMax{tB.position.x + bCol.offset.x + bCol.halfExtents.x,
                        tB.position.y + bCol.offset.y + bCol.halfExtents.y,
                        tB.position.z + bCol.offset.z + bCol.halfExtents.z};

                    Vec3 n{};
                    float pen = 0.f;
                    if (aabbVsAABB(aMin, aMax, bMin, bMax, n, pen) && pen > 0.f)
                        resolveCollision(body, bodyB, t, tB, n, pen);
                }

                if (registry.hasComponent<CapsuleColliderComponent>(e) && registry.hasComponent<BoxColliderComponent>(other)) {
                    const auto& cap = registry.getComponent<CapsuleColliderComponent>(e);
                    const auto& bCol = registry.getComponent<BoxColliderComponent>(other);
                    const Vec3 capCenter{t.position.x + cap.offset.x, t.position.y + cap.offset.y, t.position.z + cap.offset.z};
                    const Vec3 bMin{tB.position.x + bCol.offset.x - bCol.halfExtents.x,
                        tB.position.y + bCol.offset.y - bCol.halfExtents.y,
                        tB.position.z + bCol.offset.z - bCol.halfExtents.z};
                    const Vec3 bMax{tB.position.x + bCol.offset.x + bCol.halfExtents.x,
                        tB.position.y + bCol.offset.y + bCol.halfExtents.y,
                        tB.position.z + bCol.offset.z + bCol.halfExtents.z};
                    Vec3 n{};
                    float pen = 0.f;
                    if (capsuleVsAABB(capCenter, cap.radius, cap.halfHeight, bMin, bMax, n, pen))
                        resolveCollision(body, bodyB, t, tB, n, pen);
                }

                if (registry.hasComponent<BoxColliderComponent>(e) && registry.hasComponent<CapsuleColliderComponent>(other)) {
                    const auto& bCol = registry.getComponent<BoxColliderComponent>(e);
                    const auto& cap = registry.getComponent<CapsuleColliderComponent>(other);
                    const Vec3 capCenter{tB.position.x + cap.offset.x, tB.position.y + cap.offset.y, tB.position.z + cap.offset.z};
                    const Vec3 bMin{t.position.x + bCol.offset.x - bCol.halfExtents.x,
                        t.position.y + bCol.offset.y - bCol.halfExtents.y,
                        t.position.z + bCol.offset.z - bCol.halfExtents.z};
                    const Vec3 bMax{t.position.x + bCol.offset.x + bCol.halfExtents.x,
                        t.position.y + bCol.offset.y + bCol.halfExtents.y,
                        t.position.z + bCol.offset.z + bCol.halfExtents.z};
                    Vec3 n{};
                    float pen = 0.f;
                    if (capsuleVsAABB(capCenter, cap.radius, cap.halfHeight, bMin, bMax, n, pen)) {
                        const Vec3 nFlip{-n.x, -n.y, -n.z};
                        resolveCollision(body, bodyB, t, tB, nFlip, pen);
                    }
                }

                /// Capsule–capsule: bounding spheres at capsule centers (conservative; prevents dynamic characters passing through each other).
                if (registry.hasComponent<CapsuleColliderComponent>(e) && registry.hasComponent<CapsuleColliderComponent>(other)) {
                    const auto& aCol = registry.getComponent<CapsuleColliderComponent>(e);
                    const auto& bCol = registry.getComponent<CapsuleColliderComponent>(other);
                    const Vec3 aPos{t.position.x + aCol.offset.x, t.position.y + aCol.offset.y, t.position.z + aCol.offset.z};
                    const Vec3 bPos{tB.position.x + bCol.offset.x, tB.position.y + bCol.offset.y, tB.position.z + bCol.offset.z};
                    const float aBoundR = aCol.halfHeight + aCol.radius;
                    const float bBoundR = bCol.halfHeight + bCol.radius;
                    Vec3 n{};
                    float pen = 0.f;
                    if (sphereVsSphere(aPos, aBoundR, bPos, bBoundR, n, pen))
                        resolveCollision(body, bodyB, t, tB, n, pen);
                }
            }

            float groundH = 0.f;
            bool haveTerrainGround = false;
            float sampleX = t.position.x;
            float sampleZ = t.position.z;
            if (registry.hasComponent<CapsuleColliderComponent>(e)) {
                const auto& cap = registry.getComponent<CapsuleColliderComponent>(e);
                sampleX += cap.offset.x;
                sampleZ += cap.offset.z;
            }
            if (terrainHeightField && !terrainHeightField->empty()) {
                haveTerrainGround = terrainHeightField->trySampleHeight(sampleX, sampleZ, groundH);
            } else if (useProceduralTerrainGround) {
                groundH = getTerrainHeight(t.position.x, t.position.z);
                haveTerrainGround = true;
            }
            if (haveTerrainGround) {
                if (registry.hasComponent<CapsuleColliderComponent>(e))
                    resolveGround(body, t, groundH, &registry.getComponent<CapsuleColliderComponent>(e));
                else
                    resolveGround(body, t, groundH, nullptr);
            }
        }
    }

private:
    void integrate(Registry& registry, float dt)
    {
        for (auto e : registry.getEntitiesWith<TransformComponent, RigidBodyComponent>()) {
            auto& transform = registry.getComponent<TransformComponent>(e);
            auto& body = registry.getComponent<RigidBodyComponent>(e);
            if (body.invMass <= 0.f)
                continue;

            body.velocity.x += gravity.x * dt;
            body.velocity.y += gravity.y * dt;
            body.velocity.z += gravity.z * dt;
            body.velocity.x += body.forces.x * body.invMass * dt;
            body.velocity.y += body.forces.y * body.invMass * dt;
            body.velocity.z += body.forces.z * body.invMass * dt;

            if (body.linearDamping > 0.f) {
                const float damp = std::exp(-body.linearDamping * dt);
                body.velocity.x *= damp;
                body.velocity.y *= damp;
                body.velocity.z *= damp;
            }

            transform.position.x += body.velocity.x * dt;
            transform.position.y += body.velocity.y * dt;
            transform.position.z += body.velocity.z * dt;

            body.forces = {0.f, 0.f, 0.f};
        }
    }

    void buildSpatialGrid(Registry& registry)
    {
        grid.clear();
        for (auto e : registry.getEntitiesWith<TransformComponent>()) {
            auto& t = registry.getComponent<TransformComponent>(e);
            grid.insert(e, t.position);
        }
    }

    static float getTerrainHeight(float x, float z)
    {
        return std::sin(x * 0.1f) * 2.0f + std::cos(z * 0.1f) * 2.0f;
    }

    /// `groundHeight` is world Y of the terrain surface. When a capsule is present, root Y is solved so the
    /// lowest capsule point matches the surface (avoids Walk/Falling flicker from comparing root to surface).
    static void resolveGround(
        RigidBodyComponent& body,
        TransformComponent& transform,
        float groundHeight,
        const CapsuleColliderComponent* capsule)
    {
        const float surfaceY = groundHeight + kTerrainFootClearance;
        float rootOnSurface = surfaceY;
        if (capsule) {
            // Lowest point of Y-axis capsule: center.y - halfHeight - radius (in world space along offset).
            rootOnSurface = surfaceY - capsule->offset.y + capsule->halfHeight + capsule->radius;
        }
        constexpr float kSnapDown = 0.22f;
        constexpr float vMaxForSnap = 0.65f;
        const float y = transform.position.y;
        if (y < rootOnSurface) {
            transform.position.y = rootOnSurface;
            if (body.velocity.y < 0.f)
                body.velocity.y = 0.f;
            body.isGrounded = true;
        } else if (y <= rootOnSurface + kSnapDown && body.velocity.y <= vMaxForSnap) {
            transform.position.y = rootOnSurface;
            if (body.velocity.y < 0.f)
                body.velocity.y = 0.f;
            body.isGrounded = true;
        } else {
            body.isGrounded = false;
        }
    }
};
