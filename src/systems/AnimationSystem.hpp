#pragma once

#include "../animation/ClipDataSampling.hpp"
#include "../animation/PoseBlending.hpp"
#include "../components/AnimationComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../game/ModelAssetMapper.hpp"
#include <cmath>

class AnimationSystem {
public:
    void update(Registry& registry, float dt)
    {
        auto entities = registry.getEntitiesWith<AnimationComponent, SkeletonComponent, PoseComponent>();

        for (auto e : entities) {
            auto& anim = registry.getComponent<AnimationComponent>(e);
            auto& skeleton = registry.getComponent<SkeletonComponent>(e);
            auto& pose = registry.getComponent<PoseComponent>(e);

            if (anim.currentClip < 0 || static_cast<size_t>(anim.currentClip) >= anim.clips.size())
                continue;

            const size_t nBones = skeleton.bones.size();
            if (nBones == 0)
                continue;

            anim.currentTime += dt;
            const AnimationClipData& clipTo = anim.clips[static_cast<size_t>(anim.currentClip)];
            float tTo = wrapClipTime(anim.currentTime, clipTo.durationSec, anim.looping);

            const bool crossFading = anim.crossFadeFromClip >= 0 && static_cast<size_t>(anim.crossFadeFromClip) < anim.clips.size() &&
                                     anim.crossFadeAlpha < 1.f - 1e-5f;

            if (crossFading) {
                const int fromIdx = anim.crossFadeFromClip;
                anim.crossFadeFromTime += dt;
                anim.crossFadeAlpha += dt / anim.crossFadeDurationSec;
                bool fadeDone = false;
                if (anim.crossFadeAlpha >= 1.f) {
                    anim.crossFadeAlpha = 1.f;
                    fadeDone = true;
                }

                const AnimationClipData& clipFrom = anim.clips[static_cast<size_t>(fromIdx)];
                float tFrom = wrapClipTime(anim.crossFadeFromTime, clipFrom.durationSec, anim.looping);

                thread_local PoseComponent scratchA;
                thread_local PoseComponent scratchB;
                initRestPoseFromSkeleton(skeleton, scratchA);
                initRestPoseFromSkeleton(skeleton, scratchB);
                sampleAnimationClipData(clipFrom, tFrom, skeleton, scratchA);
                sampleAnimationClipData(clipTo, tTo, skeleton, scratchB);

                blendLocalPoses(pose, scratchA, scratchB, anim.crossFadeAlpha, nBones);
                if (fadeDone)
                    anim.crossFadeFromClip = -1;
            } else {
                initRestPoseFromSkeleton(skeleton, pose);
                sampleAnimationClipData(clipTo, tTo, skeleton, pose);
            }

            pose.dirty = true;
        }
    }

private:
    static float wrapClipTime(float t, float durationSec, bool looping)
    {
        if (durationSec <= 1e-6f)
            return 0.f;
        if (looping)
            return std::fmod(t, durationSec);
        return std::min(t, durationSec);
    }
};
