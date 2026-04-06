#pragma once

#include <cstdint>

// -----------------------------------------------------------------------------
// Inertial mass + per-body gravity scale + terrain / solid contact tuning.
// GravitySystem applies vertical acceleration only when `mass > 0` (use `mass <= 0`
// for kinematic / no-gravity bodies). Heavier bodies get the same downward
// acceleration unless you change GravitySystem to use inverse mass.
//
// Register: registry.registerComponent<MassComponent>();
// -----------------------------------------------------------------------------

struct MassComponent {
    /// Kilograms (unitless scale is fine). `<= 0` skips gravity integration.
    float mass = 1.0f;

    /// Multiplies world gravity for this body (per-entity tuning, wind, etc.).
    float gravityScale = 1.0f;

    /// Gameplay Position down to feet for terrain / platform contact.
    float footOffset = 0.02f;

    /// When the heightfield has no sample at this xz.
    float fallbackGroundY = 0.0f;

    uint32_t solidGroundMask = ~0u;

    float penetrationFixEps = 0.02f;
};
