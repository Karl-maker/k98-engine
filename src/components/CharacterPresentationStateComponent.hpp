#pragma once

/// Procedural / driven presentation on a character (IK cycles, clip alternation).
struct CharacterPresentationStateComponent
{
    /// Seconds; drives right-hand rest → point → rest cycle.
    float rightHandAimCycleTime = 0.f;
    /// Loops idle (clip 0) ↔ secondary clip (clip 1) with cross-fade.
    float animClipCrossFadeTimer = 0.f;
    int animClipCrossFadeSegment = 0;
};
