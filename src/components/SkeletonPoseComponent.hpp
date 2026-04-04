#pragma once

#include "../math/Mat4.hpp"
#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"
#include <unordered_map>
#include <vector>

// -----------------------------------------------------------------------------
// SkeletonPoseComponent — output of AnimationSystem: global bone matrices in
// skeleton space (root-relative), keyed by bone index. `globalPoseByBoneIndex`
// only contains entries for `syncBoneIndices` on SkeletonInstanceComponent.
// Cache fields avoid resampling when time/root match; set `invalidatePoseCache`
// on AnimationPlaybackComponent to force.
//
// Register: registry.registerComponent<SkeletonPoseComponent>();
// Same entity as SkeletonInstanceComponent (skeleton root).
// -----------------------------------------------------------------------------

struct SkeletonPoseComponent {
    std::unordered_map<int, Mat4> globalPoseByBoneIndex;

    float cachedTimePrimary = -1.0e30f;
    float cachedTimeSecondary = -1.0e30f;
    float cachedBlendAlpha = -1.0e30f;
    int cachedClipPrimary = -999999;
    int cachedClipSecondary = -999999;
    Vec3 cachedRootPosition{};
    Quat cachedRootRotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::vector<int> cachedSyncBoneIndices;
    bool poseCacheValid = false;
};
