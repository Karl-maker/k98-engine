#pragma once

#include <cstdint>

// =============================================================================
// SystemUpdateGroups — logical ordering for game simulation. The main thread
// runs these phases in order (see ThirdPersonCameraDemo::onUpdate). Registry
// access is NOT thread-safe; physics/collision stay on the main thread unless
// you add external synchronization.
//
// Environment  — streaming, caches, world prep that does not depend on latest physics.
// Simulation   — animation, transforms, attachments, camera (gameplay motion).
// Physics      — spatial grid, collision, rays (queries after transforms are final).
//
// Optional: ThreadService::parallelRange* can subdivide **read-only** or **per-chunk
// isolated** work only — do not mutate Registry from multiple threads without locks.
// =============================================================================

enum class SystemUpdateGroup : std::uint8_t {
    Environment = 0,
    Simulation  = 1,
    Physics     = 2,
};
