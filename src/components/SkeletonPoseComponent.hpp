#pragma once

#include "../math/Mat4.hpp"
#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"
#include <unordered_map>
#include <vector>

/// Sparse world matrices for synced bones only. `poseCache*` supports skipping work when
/// animation times and skeleton root transform are unchanged (e.g. speeds set to 0).
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
