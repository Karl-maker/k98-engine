#pragma once

// -----------------------------------------------------------------------------
// Filled by Control from raw input (camera-relative). Gameplay reads this in
// update and applies Velocity, gravity, jump eligibility (terrain, etc.).
// Control does not write Velocity or sample the heightfield.
// -----------------------------------------------------------------------------

struct PlayerMovementIntentComponent {
    float horizontalVelX = 0.0f;
    float horizontalVelZ = 0.0f;
    bool  jumpPressed    = false;
};
