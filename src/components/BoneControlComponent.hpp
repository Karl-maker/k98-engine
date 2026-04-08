#pragma once

#include <unordered_map>
#include "PoseComponent.hpp"

struct BoneOverrideEntry {
    BoneTransform value{};
    /// 0 = keep animated pose, 1 = full `value` (for replace mode) or full additive delta weight.
    float blendWeight = 1.f;
};

struct BoneControlComponent {
    std::unordered_map<int, BoneOverrideEntry> overrides;

    bool additive = false;
};
