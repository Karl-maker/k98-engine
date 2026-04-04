#pragma once

struct AnimationPlaybackComponent {
    int primaryClip = 0;
    int secondaryClip = 0;
    float timePrimary = 0.f;
    float timeSecondary = 0.f;
    /// 0 = full primary, 1 = full secondary (linear blend in local space before hierarchy).
    float blendAlpha = 0.f;
    float speedPrimary = 1.f;
    float speedSecondary = 1.f;
    bool loopPrimary = true;
    bool loopSecondary = true;
    /// Set true when gameplay needs a fresh pose (teleport, external bone push, clip swap, etc.).
    bool invalidatePoseCache = false;
};
