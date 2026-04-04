#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

// -----------------------------------------------------------------------------
// AttachComponent — snaps an entity’s Transform to a socket’s world pose.
// `targetEntity` is the articulated parent (e.g. player); `socketEntity` must
// have SocketComponent (updated by SocketSystem). `inheritPosition` / `inheritRotation`
// gate whether world position and/or rotation are copied each frame.
//
// Register: registry.registerComponent<AttachComponent>();
// Requires: TransformComponent on this entity; SocketComponent on socketEntity.
// Order: SocketSystem → AttachmentSystem (before FacingRaySystem for ray origins).
// -----------------------------------------------------------------------------

struct AttachComponent {
    Entity targetEntity;
    Entity socketEntity;

    Vec3 offset;
    Quat rotationOffset;

    bool inheritPosition{true};
    bool inheritRotation{true};
};