#pragma once

#include "../ecs/Entity.hpp"

/// Sparse bone entity: `AnimationSystem` fills `SkeletonPoseComponent` on `skeletonRoot`;
/// `BoneSyncSystem` writes world = root TRS × sampled bone matrix so the bone follows the character.
struct BoneInstanceComponent {
    Entity skeletonRoot = INVALID_ENTITY;
    int boneIndex = -1;
};
