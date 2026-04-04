#pragma once

#include "../core/assets/ModelAsset.hpp"
#include <memory>
#include <vector>

// -----------------------------------------------------------------------------
// SkeletonInstanceComponent — holds shared skeleton + clip data (`ModelAsset`)
// for one entity (skeleton root). `syncBoneIndices` limits CPU pose work: only
// those bones (and ancestors) are sampled; use for gameplay bones / IK hooks.
//
// Register: registry.registerComponent<SkeletonInstanceComponent>();
// Pair: SkeletonPoseComponent + AnimationPlaybackComponent + TransformComponent.
// Order: AnimationSystem reads this; BoneSyncSystem consumes pose for bone entities.
// -----------------------------------------------------------------------------

struct SkeletonInstanceComponent {
    std::shared_ptr<const ModelAsset> model;
    /// CPU world matrices are computed only for these indices. Local animation is evaluated only on
    /// root→bone paths to these leaves (sibling subtrees are skipped). Full joint list stays in `ModelAsset` for GPU.
    std::vector<int> syncBoneIndices;
};
