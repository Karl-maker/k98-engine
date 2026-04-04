#pragma once

// -----------------------------------------------------------------------------
// AnimationPlaybackComponent — clip selection and playback for AnimationSystem.
// Clips are indexed into `ModelAsset::clips` on the same entity as SkeletonInstanceComponent.
//
// ------------------------------------------------------------------------------
// HOW TO SWITCH ANIMATIONS
// ------------------------------------------------------------------------------
// The system always supports **two clips** (primary / secondary) and blends
// between their **local bone poses** using `blendAlpha` (0 = primary only, 1 =
// secondary only). Hierarchy is built after blending (`sampleAnimationBlended`).
//
// **A) Hard cut (instant switch)**  
//   - Set `primaryClip` to the new clip index.  
//   - Reset `timePrimary` to 0.f (or any start time in seconds).  
//   - Set `blendAlpha = 0.f` so only primary is used (or set `secondaryClip` equal to `primaryClip`).  
//   - Set `invalidatePoseCache = true` so the pose cache does not reuse the old pose.  
//   - Optionally pause the old clip by setting `speedSecondary = 0.f` if you still
//     keep a secondary for debugging.
//
// **B) Crossfade from clip A to clip B**  
//   - Put current animation on `primaryClip`, target on `secondaryClip`.  
//   - Drive `blendAlpha` from 0 → 1 over N seconds (your gameplay code).  
//   - Both `timePrimary` and `timeSecondary` advance by default (`speedPrimary` / `speedSecondary`).  
//   - When fade completes: assign `primaryClip = secondaryClip`, `timePrimary = timeSecondary`
//     (or 0), set `blendAlpha = 0`, set `invalidatePoseCache = true` once.
//
// **C) Single-clip playback**  
//   - Only `primaryClip` matters; keep `blendAlpha = 0` and `secondaryClip` unused or duplicate.
//
// Looping: `loopPrimary` / `loopSecondary` wrap time in the sampler; clamp otherwise.
//
// Register: registry.registerComponent<AnimationPlaybackComponent>();
// -----------------------------------------------------------------------------

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
    /// Set true when gameplay needs a fresh pose (teleport, clip swap, etc.).
    bool invalidatePoseCache = false;
};
