#pragma once

#include <vector>
#include <string>
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

struct Keyframe
{
    float time;
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};

struct BoneAnimationTrack
{
    std::string boneName;
    std::vector<Keyframe> keyframes;
};

struct AnimationClip
{
    std::string name;
    float duration;
    std::vector<BoneAnimationTrack> tracks;
};