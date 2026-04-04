#pragma once

#include "../ecs/Entity.hpp"

/// Points at `ModelAsset::meshes[meshIndex]` on the model attached to `skeletonRoot` (skinning
/// weights live on those vertices; no CPU mesh deformation here).
struct MeshRenderProxyComponent {
    Entity skeletonRoot = INVALID_ENTITY;
    int meshIndex = 0;
    int materialIndex = 0;
};
