#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"

/// Attaches this entity's transform to another entity (e.g. camera on player, passenger in vehicle).
/// Updated by `EntityAttachmentSystem` before `WorldTransformSyncSystem`.
struct EntityAttachmentComponent {
    Entity parent = INVALID_ENTITY;

    /// Offset from the parent's origin. Interpretation depends on `rotateOffsetWithParent`.
    Vec3 localOffset{};

    /// Extra local rotation applied when `inheritParentOrientation` is true: child = parent * localRotation.
    Quat localRotation{0.f, 0.f, 0.f, 1.f};

    /// When true, `localOffset` is in the parent's local axes (rotated by the parent's orientation).
    /// When false, `localOffset` is added in world space (handy for “follow position only” with a fixed world bias).
    bool rotateOffsetWithParent = true;

    /// When true, the child's rotation is set from the parent (times `localRotation`). When false, rotation is left
    /// untouched (e.g. third-person camera where `updateCamera` / look-at drives facing).
    bool inheritParentOrientation = false;

    /// 1 = snap to computed attach transform; lower values blend from the child's current transform toward it.
    float followBlend = 1.f;
};

