#pragma once

#include "../math/Vec3.hpp"
#include "../physics/CollisionLayers.hpp"

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

    // --- ThirdPersonCameraCollisionSystem (pull-in + terrain height) ---
    bool cameraCollisionEnabled = true;
    float cameraCollisionRadius = 0.2f;
    float cameraMinOrbitDistance = 0.42f;
    float cameraGroundClearance = 0.38f;
    float cameraSideProbeOffset = 0.1f;
    /// Extra meters added when pulling in (keeps orbit longer / less “zoom” on soft hits).
    float cameraObstructionRelaxMeters = 0.55f;
    /// When false, rigid bodies with invMass > 0 are ignored (camera passes through dynamic props).
    bool cameraBlockDynamicColliders = true;
    /// Default ignores `CollisionLayer::Player` so NPCs / other characters do not steal the orbit.
    uint32_t cameraObstructionLayerMask = CollisionLayer::MaskAllButPlayer;
};
