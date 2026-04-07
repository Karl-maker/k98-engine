#pragma once

#include "../math/Vec3.hpp"

// -----------------------------------------------------------------------------
// World-space horizontal move intent (m/s) after LocomotorIntentSystem combines
// control axes with camera. Locomotion apply systems scale by walk/sprint/air.
// -----------------------------------------------------------------------------

struct MovementComponent {
    /// Desired horizontal velocity in world space (y should stay 0).
    Vec3 desiredWorldVelocityXZ{0.0f, 0.0f, 0.0f};

    float walkSpeed = 5.0f;
    float sprintMultiplier = 1.65f;
    /// Horizontal scale while airborne (0 = no air control, 1 = full).
    float airControl = 0.65f;

    float jumpImpulse = 11.0f;
    /// Extra vertical when moving forward (forward axis > 0 scales this).
    float forwardJumpBonus = 2.5f;
};
