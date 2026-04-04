#pragma once

#include "../ecs/Entity.hpp"

// -----------------------------------------------------------------------------
// BoneInstanceComponent — optional entity representing one bone for gameplay /
// debug draw. `skeletonRoot` points at the entity that has SkeletonPoseComponent;
// `boneIndex` indexes into that skeleton.
//
// Register: registry.registerComponent<BoneInstanceComponent>();
// Setup: add WorldTransformComponent on the bone entity; run BoneSyncSystem after AnimationSystem.
// -----------------------------------------------------------------------------

struct BoneInstanceComponent {
    Entity skeletonRoot = INVALID_ENTITY;
    int boneIndex = -1;
};
