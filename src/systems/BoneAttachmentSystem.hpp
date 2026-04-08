#pragma once

#include "../components/BoneAttachmentComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/RenderableMeshComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"
#include "../math/MathOps.hpp"
#include "../math/Vec3.hpp"

namespace {

/// Same root matrix as skinned mesh draw: entity TRS + optional mesh model-space TRS.
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

} // namespace

class BoneAttachmentSystem {
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<BoneAttachmentComponent, TransformComponent>();

        for (auto e : entities) {
            auto& attach = registry.getComponent<BoneAttachmentComponent>(e);
            auto& transform = registry.getComponent<TransformComponent>(e);

            const Entity skelE = (attach.skeletonEntity != INVALID_ENTITY) ? attach.skeletonEntity : e;
            if (!registry.hasComponent<PoseComponent>(skelE))
                continue;
            auto& pose = registry.getComponent<PoseComponent>(skelE);

            if (attach.boneIndex < 0 || static_cast<size_t>(attach.boneIndex) >= pose.worldMatrix.size())
                continue;

            const Mat4& boneW = pose.worldMatrix[static_cast<size_t>(attach.boneIndex)];
            const Mat4 localA = Mat4::FromTRS(attach.localOffset, attach.localRotation, {1.f, 1.f, 1.f});
            const Mat4 model = skinnedEntityModelMatrix(registry, skelE);
            const Mat4 combined = mat4Mul(model, mat4Mul(boneW, localA));
            transform.position = {combined.m[12], combined.m[13], combined.m[14]};
            transform.rotation = {0.f, 0.f, 0.f, 1.f};
            (void)attach.inheritScale;
        }
    }
};
