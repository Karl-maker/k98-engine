#pragma once

#include <cstdint>

/// Optional. If missing, physics and raycasts treat the collider as matching all layers.
struct ColliderFilterComponent
{
    /// Which layer bits this collider belongs to (often a single bit).
    uint32_t categoryBits = 0xFFFFFFFFu;
    /// Which category bits this collider responds to in physics (pair test: A vs B).
    uint32_t collideMask = 0xFFFFFFFFu;
};
