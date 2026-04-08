#pragma once

#include "../core/assets/AnimationClipData.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../math/MathOps.hpp"
#include <cmath>

inline Vec3 sampleVec3Channel(const std::vector<Vec3Keyframe>& keys, float tSec)
{
    if (keys.empty())
        return {};
    if (keys.size() == 1)
        return keys[0].value;
    if (tSec <= keys.front().timeSec)
        return keys.front().value;
    if (tSec >= keys.back().timeSec)
        return keys.back().value;
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (tSec >= keys[i].timeSec && tSec <= keys[i + 1].timeSec) {
            const float t0 = keys[i].timeSec;
            const float t1 = keys[i + 1].timeSec;
            const float u = (t1 > t0) ? (tSec - t0) / (t1 - t0) : 0.f;
            const Vec3& a = keys[i].value;
            const Vec3& b = keys[i + 1].value;
            return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
        }
    }
    return keys.back().value;
}

inline Quat sampleQuatChannel(const std::vector<QuatKeyframe>& keys, float tSec)
{
    if (keys.empty())
        return {0.f, 0.f, 0.f, 1.f};
    if (keys.size() == 1)
        return quatNormalize(keys[0].value);
    if (tSec <= keys.front().timeSec)
        return quatNormalize(keys.front().value);
    if (tSec >= keys.back().timeSec)
        return quatNormalize(keys.back().value);
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (tSec >= keys[i].timeSec && tSec <= keys[i + 1].timeSec) {
            const float t0 = keys[i].timeSec;
            const float t1 = keys[i + 1].timeSec;
            const float u = (t1 > t0) ? (tSec - t0) / (t1 - t0) : 0.f;
            const Quat& a = quatNormalize(keys[i].value);
            const Quat& b = quatNormalize(keys[i + 1].value);
            const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
            const Quat b2 = (dot < 0.f) ? Quat{-b.x, -b.y, -b.z, -b.w} : b;
            return quatNormalize(Quat{
                a.x + (b2.x - a.x) * u,
                a.y + (b2.y - a.y) * u,
                a.z + (b2.z - a.z) * u,
                a.w + (b2.w - a.w) * u});
        }
    }
    return quatNormalize(keys.back().value);
}

inline void sampleAnimationClipData(
    const AnimationClipData& clip,
    float timeSec,
    const SkeletonComponent& skeleton,
    PoseComponent& pose)
{
    (void)skeleton;
    for (const auto& ch : clip.channels) {
        if (ch.boneIndex < 0 || static_cast<size_t>(ch.boneIndex) >= pose.localPose.size())
            continue;
        auto& lp = pose.localPose[static_cast<size_t>(ch.boneIndex)];
        switch (ch.path) {
        case AnimChannelPath::Translation:
            lp.position = sampleVec3Channel(ch.vecKeys, timeSec);
            break;
        case AnimChannelPath::Rotation:
            lp.rotation = sampleQuatChannel(ch.quatKeys, timeSec);
            break;
        case AnimChannelPath::Scale:
            lp.scale = sampleVec3Channel(ch.vecKeys, timeSec);
            break;
        }
    }
}
