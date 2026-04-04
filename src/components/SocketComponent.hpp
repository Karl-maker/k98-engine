#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

// -----------------------------------------------------------------------------
// SocketComponent — attachment point on a rig. `parentEntity` must have
// WorldTransformComponent; SocketSystem computes `worldTransform` each frame:
//   world = parentWorld * TRS(localOffset, localRotation, scale 1).
// Attach child entities via AttachComponent pointing at this socket entity.
//
// Register: registry.registerComponent<SocketComponent>();
// Order: TransformSystem (parent world) → SocketSystem → AttachmentSystem.
// -----------------------------------------------------------------------------

struct SocketComponent {
    Entity parentEntity;
    Vec3 localOffset;
    Quat localRotation;

    Mat4 worldTransform;
};