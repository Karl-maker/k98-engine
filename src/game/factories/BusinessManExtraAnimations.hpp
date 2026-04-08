#pragma once

#include "../ModelAssetMapper.hpp"
#include "../../components/AnimationComponent.hpp"
#include "../../components/SkeletonComponent.hpp"
#include "../../core/assets/AnimationClipData.hpp"
#include "../../math/MathOps.hpp"

namespace game::factories {

/// Appends a second looping clip: subtle spine/neck/left-arm motion (distinct from imported idle).
inline void appendBusinessManSecondaryClip(const SkeletonComponent& sk, AnimationComponent& anim)
{
    AnimationClipData clip;
    clip.name = "BusinessMan_Stretch";
    clip.durationSec = 3.f;

    auto addRotLoop = [&](const char* nameSubstr, Vec3 axisLocal, float amplitudeRad) {
        const int bi = findBoneIndexByNameSubstring(sk, nameSubstr);
        if (bi < 0 || static_cast<size_t>(bi) >= sk.bones.size())
            return;
        const Quat r0 = sk.bones[static_cast<size_t>(bi)].localRotation;
        const Quat rMid = quatNormalize(quatMul(r0, quatAxisAngle(axisLocal, amplitudeRad)));

        ClipBoneChannel ch;
        ch.boneIndex = bi;
        ch.path = AnimChannelPath::Rotation;
        ch.quatKeys.push_back({0.f, r0});
        ch.quatKeys.push_back({1.5f, rMid});
        ch.quatKeys.push_back({3.f, r0});
        clip.channels.push_back(std::move(ch));
    };

    addRotLoop("Spine2", {1.f, 0.f, 0.f}, 0.14f);
    addRotLoop("Neck", {0.f, 1.f, 0.f}, 0.1f);
    addRotLoop("LeftArm", {1.f, 0.f, 0.f}, 0.22f);

    if (!clip.channels.empty())
        anim.clips.push_back(std::move(clip));
}

} // namespace game::factories
