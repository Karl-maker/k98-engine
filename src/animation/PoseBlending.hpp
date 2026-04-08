#pragma once

#include "../components/PoseComponent.hpp"
#include "../math/MathOps.hpp"

/// Per-bone blend: `t=0` → `a`, `t=1` → `b`.
inline void blendLocalPoses(
    PoseComponent& out,
    const PoseComponent& a,
    const PoseComponent& b,
    float t,
    size_t boneCount)
{
    if (t < 0.f)
        t = 0.f;
    else if (t > 1.f)
        t = 1.f;
    if (out.localPose.size() < boneCount)
        out.localPose.resize(boneCount);
    for (size_t i = 0; i < boneCount; ++i) {
        const BoneTransform& A = a.localPose[i];
        const BoneTransform& B = b.localPose[i];
        out.localPose[i].position = vec3Lerp(A.position, B.position, t);
        out.localPose[i].rotation = quatSlerp(A.rotation, B.rotation, t);
        out.localPose[i].scale = vec3Lerp(A.scale, B.scale, t);
    }
}
