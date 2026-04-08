#pragma once

#include "../core/assets/AnimationClipData.hpp"
#include <vector>

struct AnimationComponent {
    std::vector<AnimationClipData> clips;
    int currentClip = -1;
    float currentTime = 0.f;
    bool looping = true;
};
