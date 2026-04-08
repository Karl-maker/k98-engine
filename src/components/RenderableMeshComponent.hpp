#pragma once

#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"
#include <string>

/// GPU cache for a mesh uploaded from `ModelAsset` / importer; `assetCacheKey` matches OpenGLVer2Renderer cache.
struct RenderableMeshComponent {
    std::string assetCacheKey;
    bool gpuRegistered = false;
    float uniformScale = 1.f;
    Quat modelSpaceRotation{0.f, 0.f, 0.f, 1.f};
    Vec3 modelSpaceTranslation{0.f, 0.f, 0.f};
};
