#pragma once

#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"
#include <string>

// -----------------------------------------------------------------------------
// StaticMeshComponent — references a glTF that OpenGLRenderSystem uploaded under
// `assetCacheKey` (canonical path string). The renderer draws entities that have
// this + Transform/WorldTransform using `modelSpaceRotation` (applied before
// uniformScale) so imported models can be stood upright independent of world pose.
//
// Register once: registry.registerComponent<StaticMeshComponent>();
// After ModelAsset load + uploadStaticModel(asset, key), add:
//   StaticMeshComponent{ .assetCacheKey = key, .gpuRegistered = true, ... }
// -----------------------------------------------------------------------------

struct StaticMeshComponent {
    /// Must match the key passed to OpenGLRenderSystem::uploadStaticModel.
    std::string assetCacheKey;

    /// Applied in model space after uniform scale: final = world * R * S
    Quat modelSpaceRotation = Quat::Identity();

    float uniformScale = 0.14f;

    /// Extra translation in model space after rotation and scale (e.g. sink feet to match ground).
    Vec3 modelSpaceTranslation{0.0f, 0.0f, 0.0f};

    bool gpuRegistered = false;
};
