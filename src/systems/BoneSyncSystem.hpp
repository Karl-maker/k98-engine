#pragma once

#include "../ecs/Registry.hpp"
#include "../components/BoneInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/WorldTransformComponent.hpp"

/// After AnimationSystem: copy dense pose matrices to sparse bone entities for sockets/collision.
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

            const auto& pose = registry.getComponent<SkeletonPoseComponent>(bone.skeletonRoot);
            auto it = pose.globalPoseByBoneIndex.find(bone.boneIndex);
            if (it == pose.globalPoseByBoneIndex.end())
                continue;

            auto& world = registry.getComponent<WorldTransformComponent>(e);
            world.world = it->second;
            world.dirty = true;
        }
    }
};
