#pragma once

// -----------------------------------------------------------------------------
// Raw locomotion axes from input (-1..1). Systems combine this with camera yaw
// to fill MovementComponent. Jump is edge-triggered via jumpRequested.
// -----------------------------------------------------------------------------

struct LocomotorControlComponent {
    /// Forward / back (game forward is typically -Z when aligned with camera).
    float forward = 0.0f;
    /// Strafe right positive, left negative.
    float right = 0.0f;

    bool sprintHeld = false;

    /// Set true for one frame when jump key pressed; cleared when consumed.
    bool jumpRequested = false;
};
