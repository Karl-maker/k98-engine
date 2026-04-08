#pragma once

#include "../ecs/Registry.hpp"
#include "../components/BoneAttachmentComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../math/Mat4.hpp"

class BoneAttachmentSystem
{
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
            const Mat4 combined = mat4Mul(boneW, localA);
            transform.position = {combined.m[12], combined.m[13], combined.m[14]};
            transform.rotation = {0.f, 0.f, 0.f, 1.f};
            (void)attach.inheritScale;
        }
    }
};
