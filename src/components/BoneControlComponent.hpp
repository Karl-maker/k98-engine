#pragma once

#include <unordered_map>
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

struct BoneControlComponent
{
    std::unordered_map<int, BoneTransform> overrides;

    bool additive = false;
};