#pragma once

// =============================================================================
// BoneSyncSystem — copies sparse bone global matrices from SkeletonPoseComponent
// (on skeleton root) into BoneInstance entities’ WorldTransformComponent, multiplied
// by the root’s world TRS so bones follow the character.
//
// Registration:
//   BoneInstanceComponent, SkeletonPoseComponent, TransformComponent (root),
//   WorldTransformComponent (bone entities)
//
// Order: after AnimationSystem.
// =============================================================================

#include "../ecs/Registry.hpp"
#include "../components/BoneInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Mat4.hpp"

class BoneSyncSystem {
public:
    void update(Registry& registry) {
        auto entities = registry.getEntitiesWith<BoneInstanceComponent, WorldTransformComponent>();

        for (Entity e : entities) {
            auto& bone = registry.getComponent<BoneInstanceComponent>(e);
            if (bone.skeletonRoot == INVALID_ENTITY || bone.boneIndex < 0)
                continue;
            if (!registry.hasComponent<SkeletonPoseComponent>(bone.skeletonRoot))
                continue;
            if (!registry.hasComponent<TransformComponent>(bone.skeletonRoot))
                continue;

            const auto& pose = registry.getComponent<SkeletonPoseComponent>(bone.skeletonRoot);
            auto it = pose.globalPoseByBoneIndex.find(bone.boneIndex);
            if (it == pose.globalPoseByBoneIndex.end())
                continue;

            const auto& rootTf = registry.getComponent<TransformComponent>(bone.skeletonRoot);
            const Mat4 rootWorld = Mat4::FromTRS(rootTf.position, rootTf.rotation, rootTf.scale);

            auto& world = registry.getComponent<WorldTransformComponent>(e);
            world.world = Mat4::mat4Mul(rootWorld, it->second);
            world.dirty = true;
        }
    }
};
