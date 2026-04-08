#pragma once

#include "../components/SkeletonComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../components/GpuSkinPaletteComponent.hpp"
#include "../math/Mat4.hpp"

inline void computeJointSkinMatrices(
    const SkeletonComponent& skeleton,
    const PoseComponent& pose,
    GpuSkinPaletteComponent& out)
{
    const size_t n = skeleton.bones.size();
    out.jointSkinMatrices.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const Mat4& g = (i < pose.worldMatrix.size()) ? pose.worldMatrix[i] : Mat4::Identity();
        out.jointSkinMatrices[i] = mat4Mul(g, skeleton.bones[i].inverseBind);
    }
}
