#pragma once

#include "../ecs/Registry.hpp"
#include "../components/SkeletonInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/AnimationPlaybackComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../animation/AnimationSampling.hpp"

class AnimationSystem {
public:
    void update(Registry& registry, float dt) {
        auto entities = registry.getEntitiesWith<
            SkeletonInstanceComponent,
            SkeletonPoseComponent,
            AnimationPlaybackComponent,
            TransformComponent>();

        for (Entity e : entities) {
            auto& skel = registry.getComponent<SkeletonInstanceComponent>(e);
            auto& pose = registry.getComponent<SkeletonPoseComponent>(e);
            auto& play = registry.getComponent<AnimationPlaybackComponent>(e);
            auto& rootT = registry.getComponent<TransformComponent>(e);

            if (!skel.model || skel.model->skeleton.bones.empty())
                continue;

            if (play.speedPrimary != 0.f)
                play.timePrimary += dt * play.speedPrimary;
            if (play.speedSecondary != 0.f)
                play.timeSecondary += dt * play.speedSecondary;

            const bool forced = play.invalidatePoseCache;
            if (play.invalidatePoseCache)
                play.invalidatePoseCache = false;

            const bool cacheOk = pose.poseCacheValid && !forced &&
                floatApproxEqual(play.timePrimary, pose.cachedTimePrimary) &&
                floatApproxEqual(play.timeSecondary, pose.cachedTimeSecondary) &&
                floatApproxEqual(play.blendAlpha, pose.cachedBlendAlpha) &&
                play.primaryClip == pose.cachedClipPrimary &&
                play.secondaryClip == pose.cachedClipSecondary &&
                vec3ApproxEqual(rootT.position, pose.cachedRootPosition) &&
                quatApproxEqualOrientation(rootT.rotation, pose.cachedRootRotation) &&
                sameSortedSyncIndices(skel.syncBoneIndices, pose.cachedSyncBoneIndices);

            if (cacheOk)
                continue;

            sampleAnimationBlended(
                *skel.model,
                play.primaryClip,
                play.timePrimary,
                play.secondaryClip,
                play.timeSecondary,
                play.blendAlpha,
                play.loopPrimary,
                play.loopSecondary,
                skel.syncBoneIndices,
                pose.globalPoseByBoneIndex);

            pose.cachedTimePrimary = play.timePrimary;
            pose.cachedTimeSecondary = play.timeSecondary;
            pose.cachedBlendAlpha = play.blendAlpha;
            pose.cachedClipPrimary = play.primaryClip;
            pose.cachedClipSecondary = play.secondaryClip;
            pose.cachedRootPosition = rootT.position;
            pose.cachedRootRotation = rootT.rotation;
            pose.cachedSyncBoneIndices = skel.syncBoneIndices;
            pose.poseCacheValid = true;
        }
    }
};
