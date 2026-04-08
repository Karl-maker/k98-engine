#pragma once

#include "../ecs/Registry.hpp"
#include "../components/AnimationComponent.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../animation/ClipDataSampling.hpp"
#include <cmath>

class AnimationSystem
{
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

            const AnimationClipData& clip = anim.clips[static_cast<size_t>(anim.currentClip)];
            anim.currentTime += dt;
            float t = anim.currentTime;
            if (clip.durationSec > 1e-6f && anim.looping)
                t = std::fmod(t, clip.durationSec);
            if (clip.durationSec > 1e-6f && !anim.looping)
                t = std::min(t, clip.durationSec);

            sampleAnimationClipData(clip, t, skeleton, pose);
            pose.dirty = true;
        }
    }
};
