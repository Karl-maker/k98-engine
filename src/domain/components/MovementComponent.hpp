#pragma once

#include "../../math/Vec3.hpp"
#include "../states/MovementStateEnums.hpp"

struct MovementComponent
{
    // INPUT / INTENT
    Vec3 desiredDirection{0,0,0}; // normalized
    bool wantsToJump = false;

    // STATE
    MovementState state = MovementState::Idle;

    // SPEEDS
    float walkSpeed   = 2.0f;
    float runSpeed    = 4.0f;
    float sprintSpeed = 6.5f;
    float crouchSpeed = 1.5f;
    float crawlSpeed  = 1.0f;

    // PHYSICS TUNING
    float acceleration = 20.0f;
    float airControl   = 0.3f;
    float jumpForce    = 5.0f;

    // RUNTIME
    float currentSpeed = 0.0f;
};