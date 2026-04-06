#pragma once

// -----------------------------------------------------------------------------
// Written by gameplay after support (terrain + solid tops) is resolved, if you track grounded state.
// Other systems (jump, animation) read grounded without duplicating height queries.
// -----------------------------------------------------------------------------

struct GroundingStateComponent {
    bool grounded = false;
};
