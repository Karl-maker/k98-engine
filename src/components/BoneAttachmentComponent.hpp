#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

struct BoneAttachmentComponent
{
    /// Entity that owns `PoseComponent` / skeleton (defaults to self if `INVALID_ENTITY`).
    Entity skeletonEntity = INVALID_ENTITY;
    int boneIndex = -1;
    Vec3 localOffset{};
    Quat localRotation{0.f, 0.f, 0.f, 1.f};
    bool inheritScale = true;
};
