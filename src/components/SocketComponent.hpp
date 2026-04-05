#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

// -----------------------------------------------------------------------------
// SocketComponent — attachment point on a rig. Default: `parentEntity` has
// WorldTransformComponent; SocketSystem sets `worldTransform` =
//   parentWorld * TRS(localOffset, localRotation, scale 1).
// Bone-follow: if `followBoneIndex >= 0`, set `skeletonRoot` to the entity that has
// SkeletonPoseComponent + WorldTransform (same entity as the skinned character);
// then `worldTransform = rootWorld * globalBoneMatrix * localSocketTRS` so sockets
// track animated bones. Attach children with AttachComponent targeting this socket entity.
//
// Register: registry.registerComponent<SocketComponent>();
// Order: TransformSystem (parent world) → SocketSystem → AttachmentSystem.
// -----------------------------------------------------------------------------

struct SocketComponent {
    Entity parentEntity;
    Vec3 localOffset;
    Quat localRotation;

    Mat4 worldTransform;

    /// If >= 0, use animated bone pose from `skeletonRoot` (see file comment). Otherwise use `parentEntity`.
    Entity skeletonRoot = INVALID_ENTITY;
    int followBoneIndex = -1;

    /// When true, `OpenGLRenderSystem` draws a small yellow debug pyramid at this socket.
    bool debugDrawPyramid = false;
};