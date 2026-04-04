#pragma once

#include "../core/assets/ModelAsset.hpp"
#include <memory>
#include <vector>

struct SkeletonInstanceComponent {
    std::shared_ptr<const ModelAsset> model;
    /// CPU world matrices are computed only for these indices. Local animation is evaluated only on
    /// root→bone paths to these leaves (sibling subtrees are skipped). Full joint list stays in `ModelAsset` for GPU.
    std::vector<int> syncBoneIndices;
};
