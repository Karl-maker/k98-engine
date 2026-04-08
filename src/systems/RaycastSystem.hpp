#pragma once

#include "../components/BoneAttachmentComponent.hpp"
#include "../components/BoxColliderComponent.hpp"
#include "../components/CapsuleColliderComponent.hpp"
#include "../components/ColliderFilterComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/RaycastComponent.hpp"
#include "../components/RenderableMeshComponent.hpp"
#include "../components/SphereColliderComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"
#include "../math/MathOps.hpp"
#include "../physics/CollisionFilter.hpp"
#include "../physics/Raycast.hpp"
#include "../physics/SpatialGrid.hpp"

#include <cmath>
#include <unordered_set>
#include <vector>

namespace raycast_detail {

inline Mat4 skinnedEntityModelMatrix(Registry& registry, Entity skelE)
{
    const auto& t = registry.getComponent<TransformComponent>(skelE);
    Mat4 base = Mat4::Identity();
    if (registry.hasComponent<WorldTransformComponent>(skelE)) {
        const auto& w = registry.getComponent<WorldTransformComponent>(skelE).world;
        const Vec3 wp{w.m[12], w.m[13], w.m[14]};
        base = Mat4::FromTRS(wp, t.rotation, t.scale);
    } else {
        base = Mat4::FromTRS(t.position, t.rotation, t.scale);
    }

    if (registry.hasComponent<RenderableMeshComponent>(skelE)) {
        const auto& smc = registry.getComponent<RenderableMeshComponent>(skelE);
        const Mat4 R = Mat4::FromTR({0.f, 0.f, 0.f}, smc.modelSpaceRotation);
        const Mat4 S = Mat4::FromScale({smc.uniformScale, smc.uniformScale, smc.uniformScale});
        const Mat4 Tm = Mat4::FromTranslation(smc.modelSpaceTranslation);
        return mat4Mul(mat4Mul(mat4Mul(base, R), S), Tm);
    }
    return base;
}

inline void worldRayFromEntity(Registry& registry, Entity e, const RaycastComponent& rc, Vec3& originOut, Vec3& dirOut)
{
    originOut = registry.getComponent<TransformComponent>(e).position;
    dirOut = rc.localDirection;

    if (registry.hasComponent<BoneAttachmentComponent>(e)) {
        const auto& attach = registry.getComponent<BoneAttachmentComponent>(e);
        const Entity skelE = (attach.skeletonEntity != INVALID_ENTITY) ? attach.skeletonEntity : e;
        if (registry.hasComponent<PoseComponent>(skelE) && attach.boneIndex >= 0) {
            auto& pose = registry.getComponent<PoseComponent>(skelE);
            if (static_cast<size_t>(attach.boneIndex) < pose.worldMatrix.size()) {
                const Mat4& boneW = pose.worldMatrix[static_cast<size_t>(attach.boneIndex)];
                const Mat4 localA = Mat4::FromTRS(attach.localOffset, attach.localRotation, {1.f, 1.f, 1.f});
                const Mat4 model = skinnedEntityModelMatrix(registry, skelE);
                const Mat4 combined = mat4Mul(model, mat4Mul(boneW, localA));
                originOut = {combined.m[12], combined.m[13], combined.m[14]};
                dirOut = Mat4::transformDirection(combined, rc.localDirection);
            }
        }
    } else {
        const auto& t = registry.getComponent<TransformComponent>(e);
        const Mat4 rotOnly = Mat4::FromQuat(t.rotation);
        dirOut = Mat4::transformDirection(rotOnly, rc.localDirection);
    }

    dirOut = normalize(dirOut);
    if (lengthSquared(dirOut) < 1e-12f)
        dirOut = {0.f, 0.f, -1.f};
}

inline uint32_t colliderCategory(Registry& registry, Entity e)
{
    if (registry.hasComponent<ColliderFilterComponent>(e))
        return registry.getComponent<ColliderFilterComponent>(e).categoryBits;
    return 0xFFFFFFFFu;
}

/// Builds unit directions: center = `axis`, then rings of rays tilted from the axis. Returns count (>= 1).
inline int buildConeRayDirections(
    const Vec3& axisIn,
    float halfAngleDeg,
    int rings,
    int segments,
    Vec3* out,
    int maxOut)
{
    if (maxOut < 1)
        return 0;
    Vec3 ax = normalize(axisIn);
    if (lengthSquared(ax) < 1e-12f)
        ax = {0.f, 0.f, -1.f};
    if (halfAngleDeg <= 1e-4f || rings < 1 || segments < 1) {
        out[0] = ax;
        return 1;
    }

    Vec3 up = std::abs(ax.y) < 0.99f ? Vec3{0.f, 1.f, 0.f} : Vec3{1.f, 0.f, 0.f};
    Vec3 u = normalize(cross(ax, up));
    Vec3 v = cross(ax, u);
    const float halfRad = halfAngleDeg * 3.14159265f / 180.f;

    int n = 0;
    out[n++] = ax;
    for (int r = 1; r <= rings && n < maxOut; ++r) {
        const float alpha = halfRad * (static_cast<float>(r) / static_cast<float>(rings));
        const float sa = std::sin(alpha);
        const float ca = std::cos(alpha);
        for (int s = 0; s < segments && n < maxOut; ++s) {
            const float phi = 2.f * 3.14159265f * static_cast<float>(s) / static_cast<float>(segments);
            const float cp = std::cos(phi);
            const float sp = std::sin(phi);
            Vec3 dir{
                ax.x * ca + sa * (cp * u.x + sp * v.x),
                ax.y * ca + sa * (cp * u.y + sp * v.y),
                ax.z * ca + sa * (cp * u.z + sp * v.z)};
            out[n++] = normalize(dir);
        }
    }
    return n;
}

inline bool testRayAgainstEntity(
    Registry& registry,
    Entity target,
    const Vec3& o,
    const Vec3& d,
    float maxT,
    RaycastHitData& hit)
{
    if (!registry.hasComponent<TransformComponent>(target))
        return false;
    const auto& t = registry.getComponent<TransformComponent>(target);

    if (registry.hasComponent<BoxColliderComponent>(target)) {
        const auto& box = registry.getComponent<BoxColliderComponent>(target);
        const Vec3 bMin{t.position.x + box.offset.x - box.halfExtents.x,
            t.position.y + box.offset.y - box.halfExtents.y,
            t.position.z + box.offset.z - box.halfExtents.z};
        const Vec3 bMax{t.position.x + box.offset.x + box.halfExtents.x,
            t.position.y + box.offset.y + box.halfExtents.y,
            t.position.z + box.offset.z + box.halfExtents.z};
        float th = 0.f;
        Vec3 n{};
        if (!rayVsAABB(o, d, maxT, bMin, bMax, th, n))
            return false;
        hit.t = th;
        hit.point = {o.x + d.x * th, o.y + d.y * th, o.z + d.z * th};
        hit.normal = n;
        return true;
    }
    if (registry.hasComponent<SphereColliderComponent>(target)) {
        const auto& sp = registry.getComponent<SphereColliderComponent>(target);
        const Vec3 c{t.position.x + sp.offset.x, t.position.y + sp.offset.y, t.position.z + sp.offset.z};
        float th = 0.f;
        Vec3 n{};
        if (!rayVsSphere(o, d, maxT, c, sp.radius, th, n))
            return false;
        hit.t = th;
        hit.point = {o.x + d.x * th, o.y + d.y * th, o.z + d.z * th};
        hit.normal = n;
        return true;
    }
    if (registry.hasComponent<CapsuleColliderComponent>(target)) {
        const auto& cap = registry.getComponent<CapsuleColliderComponent>(target);
        const Vec3 cc{t.position.x + cap.offset.x, t.position.y + cap.offset.y, t.position.z + cap.offset.z};
        float th = 0.f;
        Vec3 n{};
        if (!rayVsCapsuleY(o, d, maxT, cc, cap.radius, cap.halfHeight, th, n))
            return false;
        hit.t = th;
        hit.point = {o.x + d.x * th, o.y + d.y * th, o.z + d.z * th};
        hit.normal = n;
        return true;
    }
    return false;
}

} // namespace raycast_detail

class RaycastSystem {
public:
    void update(Registry& registry, SpatialGrid& grid)
    {
        grid.clear();
        for (auto e : registry.getEntitiesWith<TransformComponent>()) {
            auto& t = registry.getComponent<TransformComponent>(e);
            grid.insert(e, t.position);
        }

        std::vector<Entity> broadTmp;
        std::unordered_set<Entity> candidates;

        for (auto e : registry.getEntitiesWith<RaycastComponent, TransformComponent>()) {
            auto& rc = registry.getComponent<RaycastComponent>(e);
            Vec3 o{};
            Vec3 axis{};
            raycast_detail::worldRayFromEntity(registry, e, rc, o, axis);
            const float maxT = rc.maxDistance;

            Vec3 dirs[RaycastComponent::kMaxConeDebugRays];
            int nDirs = 1;
            dirs[0] = axis;
            if (rc.useCone) {
                nDirs = raycast_detail::buildConeRayDirections(
                    axis,
                    rc.coneHalfAngleDeg,
                    rc.coneRings,
                    rc.coneSegments,
                    dirs,
                    RaycastComponent::kMaxConeDebugRays);
            }

            rc.lastWorldOrigin = o;
            rc.lastWorldDirection = axis;
            rc.hasHit = false;
            rc.hitEntity = INVALID_ENTITY;
            rc.hitDistance = 0.f;
            rc.debugRayCount = 0;

            candidates.clear();
            for (int i = 0; i < nDirs; ++i) {
                const Vec3& dn = dirs[i];
                const Vec3 segEnd{o.x + dn.x * maxT, o.y + dn.y * maxT, o.z + dn.z * maxT};
                grid.querySegment(o, segEnd, broadTmp);
                for (Entity t : broadTmp)
                    candidates.insert(t);
            }

            RaycastHitData best{};
            best.t = maxT + 1.f;
            bool found = false;

            for (Entity tgt : candidates) {
                if (tgt == e)
                    continue;
                if (tgt == rc.ignoreEntity)
                    continue;
                const uint32_t cat = raycast_detail::colliderCategory(registry, tgt);
                if (!raycastLayerTest(cat, rc.layerMask))
                    continue;

                for (int i = 0; i < nDirs; ++i) {
                    const Vec3& dn = dirs[i];
                    RaycastHitData h{};
                    h.entity = tgt;
                    if (!raycast_detail::testRayAgainstEntity(registry, tgt, o, dn, maxT, h))
                        continue;
                    if (h.t < best.t && h.t >= 0.f) {
                        best = h;
                        best.entity = tgt;
                        found = true;
                    }
                }
            }

            for (int i = 0; i < nDirs && i < RaycastComponent::kMaxConeDebugRays; ++i) {
                const Vec3& dn = dirs[i];
                rc.debugRayEnd[rc.debugRayCount++] = {
                    o.x + dn.x * maxT,
                    o.y + dn.y * maxT,
                    o.z + dn.z * maxT};
            }

            const Vec3 segCentral{o.x + axis.x * maxT, o.y + axis.y * maxT, o.z + axis.z * maxT};
            if (found) {
                rc.hasHit = true;
                rc.hitDistance = best.t;
                rc.hitPoint = best.point;
                rc.hitNormal = best.normal;
                rc.hitEntity = best.entity;
                rc.lastRayEnd = {o.x + axis.x * best.t, o.y + axis.y * best.t, o.z + axis.z * best.t};
            } else {
                rc.lastRayEnd = segCentral;
            }
        }
    }
};
