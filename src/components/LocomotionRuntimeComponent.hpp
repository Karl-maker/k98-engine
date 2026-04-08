#pragma once

// -----------------------------------------------------------------------------
// Per-frame locomotion bookkeeping (ground transitions for FSM).
// -----------------------------------------------------------------------------

struct LocomotionRuntimeComponent {
    bool wasGroundedLastFrame = true;
    bool sprintWasHeldLastFrame = false;
};
