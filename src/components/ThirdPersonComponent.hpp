#pragma once

#include "../math/Vec3.hpp"

/// Orbit camera + planar locomotion basis for third-person control. Use with `CameraComponent`;
/// first-person rigs omit this component.
struct ThirdPersonComponent
{
    float orbitYaw = 3.14159265f;
    float orbitPitch = 0.22f;
    float orbitDistance = 4.35f;
    /// Height of orbit pivot above the followed entity's origin.
    float orbitPivotHeight = 0.9f;
    float mouseSensitivity = 0.0025f;

    /// World-space XZ basis from the current view (flattened eye→target). Filled before movement controllers run.
    Vec3 planarMoveForward{0.f, 0.f, -1.f};
    Vec3 planarMoveRight{1.f, 0.f, 0.f};
};
