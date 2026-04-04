#pragma once

#include "../ecs/Entity.hpp"
#include <cstdint>

// -----------------------------------------------------------------------------
// FacingRayDriverComponent — paired with RayComponent + TransformComponent on the
// same entity. Drives a ray used for aim / line-of-sight: origin follows the
// entity transform (use AttachComponent + SocketComponent on a parent for
// chest-height offset); horizontal direction is computed from `cameraEntity`’s
// CameraComponent (lock-on / look-at) and transforms.
//
// Setup (typical):
//   1. Socket on player (offset). 2. Ray entity: Transform, Attach(parent, socket),
//      RayComponent, RaycastHitComponent, FacingRayDriverComponent{ .cameraEntity = cam, ... }.
//   3. Each frame: SocketSystem → AttachmentSystem → FacingRaySystem → RaycastSystem.
//
// Registration:
//   registry.registerComponent<FacingRayDriverComponent>();
// -----------------------------------------------------------------------------

struct FacingRayDriverComponent {
    /// Camera used for lock-on / look-at aim (must have CameraComponent + TransformComponent).
    Entity cameraEntity = INVALID_ENTITY;
    /// Copy Transform.rotation from this entity each tick (e.g. player body yaw).
    Entity rotationAlignEntity = INVALID_ENTITY;
    /// Passed to RayComponent::ignoreEntity.
    Entity ignoreEntity = INVALID_ENTITY;

    float    maxDistance = 100.0f;
    uint32_t layerMask   = 0xFFFFFFFFu;
    float    sweepRadius = 0.0f;
};
