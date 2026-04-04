#pragma once

#include <cstdint>
#include "../../math/Vec3.hpp"
#include "../../math/Quat.hpp"
#include <string>
#include <vector>

enum class AnimChannelPath : std::uint8_t {
    Translation,
    Rotation,
    Scale
};

struct Vec3Keyframe {
    float timeSec = 0.f;
    Vec3 value{};
};

struct QuatKeyframe {
    float timeSec = 0.f;
    Quat value{0, 0, 0, 1};
};

/// One glTF-style channel: affects one bone, one path (T/R/S).
struct ClipBoneChannel {
    int boneIndex = -1;
    AnimChannelPath path = AnimChannelPath::Translation;
    std::vector<Vec3Keyframe> vecKeys;
    std::vector<QuatKeyframe> quatKeys;
};

struct AnimationClipData {
    std::string name;
    float durationSec = 0.f;
    std::vector<ClipBoneChannel> channels;
};
