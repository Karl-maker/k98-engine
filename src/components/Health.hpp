#pragma once

// -----------------------------------------------------------------------------
// Health — integer hit points. Gameplay / UI read `hp`; no system in this
// project auto-applies damage (set from your combat logic).
//
// Register: registry.registerComponent<Health>();
// -----------------------------------------------------------------------------

struct Health
{
    int hp = 100;
};
