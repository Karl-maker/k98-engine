#pragma once

#include "../ecs/Entity.hpp"

/// Sparse bone entity: driven by dense pose on skeletonRoot (see BoneSyncSystem).
struct BoneInstanceComponent {
    Entity skeletonRoot = INVALID_ENTITY;
    int boneIndex = -1;
};
