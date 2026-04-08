#pragma once

#include "../core/assets/AnimationClipData.hpp"
#include <cstddef>
#include <vector>

struct AnimationComponent {
    std::vector<AnimationClipData> clips;
    int currentClip = -1;
    float currentTime = 0.f;
    bool looping = true;

    /// When `crossFadeFromClip >= 0`, blend from that clip into `currentClip` (`crossFadeAlpha` 0→1).
    int crossFadeFromClip = -1;
    float crossFadeFromTime = 0.f;
    float crossFadeAlpha = 1.f;
    float crossFadeDurationSec = 0.25f;

    /// Smooth transition from the playing clip to `newClipIndex` over `durationSec`.
    void requestCrossFadeToClip(int newClipIndex, float durationSec = 0.25f);
};

inline void AnimationComponent::requestCrossFadeToClip(int newClipIndex, float durationSec)
{
    if (clips.empty() || newClipIndex < 0 || static_cast<size_t>(newClipIndex) >= clips.size())
        return;
    if (newClipIndex == currentClip)
        return;

    crossFadeDurationSec = durationSec > 1e-6f ? durationSec : 0.25f;

    if (currentClip >= 0 && static_cast<size_t>(currentClip) < clips.size()) {
        crossFadeFromClip = currentClip;
        crossFadeFromTime = currentTime;
        crossFadeAlpha = 0.f;
    } else {
        crossFadeFromClip = -1;
        crossFadeAlpha = 1.f;
    }

    currentClip = newClipIndex;
    currentTime = 0.f;
}
