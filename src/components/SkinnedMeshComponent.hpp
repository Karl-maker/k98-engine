#pragma once

#include "../ecs/Entity.hpp"

// -----------------------------------------------------------------------------
// SkinnedMeshComponent — ECS binding between a **skinned mesh** (subset of a
// ModelAsset) and the **skeleton root** entity that owns animation data.
//
// Data flow for rendering (GPU skinning):
//   - **Joint indices & weights** are stored per-vertex on the mesh:
//       `skeletonRoot` → SkeletonInstanceComponent::model →
//       `model->meshes[meshIndex].vertices[]` → each `Vertex` has `boneIndex[4]`,
//       `boneWeight[4]` (see `math/Vertex.hpp`), filled at import (e.g. glTF JOINTS_0 / WEIGHTS_0).
//   - **Inverse bind matrices** live on the skeleton:
//       `model->skeleton.bones[j].inverseBind`.
//   - **Animated bone matrices** (skeleton space) come from AnimationSystem into
//       SkeletonPoseComponent::globalPoseByBoneIndex on `skeletonRoot`; multiply
//       global * inverseBind in the shader per bone index referenced by vertices.
//
// This component does **not** duplicate weights — it only identifies which mesh
// submesh to draw and which skeleton drives it. Optional child `TransformComponent`
// with `parent = skeletonRoot` follows the rig (see SkeletonSpawn).
//
// Registration:
//   registry.registerComponent<SkinnedMeshComponent>();
//
// Example (after loading ModelAsset on skeleton root):
//   registry.addComponent(meshEntity, SkinnedMeshComponent{ .skeletonRoot = root, .meshIndex = 0, .materialIndex = 0 });
//
// Backward-compatible alias:
// -----------------------------------------------------------------------------
struct SkinnedMeshComponent {
    /// Entity with SkeletonInstanceComponent + SkeletonPoseComponent (same ModelAsset as mesh).
    Entity skeletonRoot = INVALID_ENTITY;
    /// Index into `model->meshes` (vertices contain bone indices/weights).
    int meshIndex = 0;
    int materialIndex = 0;
};

using MeshRenderProxyComponent = SkinnedMeshComponent;
