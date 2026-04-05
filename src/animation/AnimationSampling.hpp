#pragma once

#include "../core/assets/ModelAsset.hpp"
#include "../core/assets/AnimationClipData.hpp"
#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct BoneLocalTRS {
    Vec3 translation{};
    Quat rotation{0, 0, 0, 1};
    Vec3 scale{1, 1, 1};
};

inline float animationTimeClamp(float t, float durationSec, bool loop) {
    if (durationSec <= 1e-8f)
        return 0.f;
    if (loop) {
        t = std::fmod(t, durationSec);
        if (t < 0.f)
            t += durationSec;
        return t;
    }
    return std::clamp(t, 0.f, durationSec);
}

inline Vec3 sampleVec3Channel(const std::vector<Vec3Keyframe>& keys, float t) {
    if (keys.empty())
        return Vec3{};
    if (keys.size() == 1)
        return keys[0].value;
    if (t <= keys.front().timeSec)
        return keys.front().value;
    if (t >= keys.back().timeSec)
        return keys.back().value;
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].timeSec && t <= keys[i + 1].timeSec) {
            float t0 = keys[i].timeSec;
            float t1 = keys[i + 1].timeSec;
            float u = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.f;
            return lerp(keys[i].value, keys[i + 1].value, u);
        }
    }
    return keys.back().value;
}

inline Quat sampleQuatChannel(const std::vector<QuatKeyframe>& keys, float t) {
    if (keys.empty())
        return Quat::Identity();
    if (keys.size() == 1)
        return quatNormalize(keys[0].value);
    if (t <= keys.front().timeSec)
        return quatNormalize(keys.front().value);
    if (t >= keys.back().timeSec)
        return quatNormalize(keys.back().value);
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].timeSec && t <= keys[i + 1].timeSec) {
            float t0 = keys[i].timeSec;
            float t1 = keys[i + 1].timeSec;
            float u = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.f;
            return quatSlerp(quatNormalize(keys[i].value), quatNormalize(keys[i + 1].value), u);
        }
    }
    return quatNormalize(keys.back().value);
}

inline void initLocalsFromRest(const Skeleton& sk, std::vector<BoneLocalTRS>& out) {
    out.resize(sk.bones.size());
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        out[i].translation = sk.bones[i].restTranslation;
        out[i].rotation = quatNormalize(sk.bones[i].restRotation);
        out[i].scale = sk.bones[i].restScale;
    }
}

/// Walks from `boneIndex` toward the root using only `Bone::parentIndex` links (each value is an
/// index into `sk.bones`). Vector storage order does **not** define hierarchy — glTF joint order
/// can be arbitrary as long as `parentIndex` forms a tree (or chain) back to roots (`parentIndex < 0`).
/// Returns a chain ordered **root → … → boneIndex** for matrix accumulation.
inline bool boneChainRootFirst(
    const Skeleton& sk,
    int boneIndex,
    std::vector<int>& outRootToBone) {
    outRootToBone.clear();
    if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= sk.bones.size())
        return false;

    std::vector<int> leafToRoot;
    std::unordered_set<int> seenAlongWalk;
    int i = boneIndex;
    int guard = 0;
    const int maxSteps = static_cast<int>(sk.bones.size()) + 2;

    while (i >= 0 && guard < maxSteps) {
        if (static_cast<size_t>(i) >= sk.bones.size())
            return false;
        if (seenAlongWalk.count(i))
            return false;
        seenAlongWalk.insert(i);
        leafToRoot.push_back(i);

        int p = sk.bones[static_cast<size_t>(i)].parentIndex;
        if (p == i)
            break;
        i = p;
        ++guard;
    }

    outRootToBone.assign(leafToRoot.rbegin(), leafToRoot.rend());
    return true;
}

/// Union of root→bone paths for each synced leaf. Sibling subtrees (e.g. indices 6–7 when only 5 is
/// synced) are excluded — we only follow `parentIndex` chains toward roots.
inline std::unordered_set<int> bonesNeededForSyncIndices(
    const Skeleton& sk,
    const std::vector<int>& syncBoneIndices) {
    std::unordered_set<int> needed;
    std::vector<int> chain;
    for (int bi : syncBoneIndices) {
        if (!boneChainRootFirst(sk, bi, chain))
            continue;
        for (int idx : chain)
            needed.insert(idx);
    }
    return needed;
}

inline void initLocalsFromRestPartial(
    const Skeleton& sk,
    const std::unordered_set<int>& needed,
    std::vector<BoneLocalTRS>& out) {
    out.assign(sk.bones.size(), BoneLocalTRS{});
    for (int i : needed) {
        if (i < 0 || static_cast<size_t>(i) >= sk.bones.size())
            continue;
        size_t u = static_cast<size_t>(i);
        out[u].translation = sk.bones[u].restTranslation;
        out[u].rotation = quatNormalize(sk.bones[u].restRotation);
        out[u].scale = sk.bones[u].restScale;
    }
}

inline void applyClipChannels(const AnimationClipData& clip, float timeSec, std::vector<BoneLocalTRS>& io) {
    for (const ClipBoneChannel& ch : clip.channels) {
        if (ch.boneIndex < 0 || static_cast<size_t>(ch.boneIndex) >= io.size())
            continue;
        BoneLocalTRS& b = io[static_cast<size_t>(ch.boneIndex)];
        switch (ch.path) {
        case AnimChannelPath::Translation:
            if (!ch.vecKeys.empty())
                b.translation = sampleVec3Channel(ch.vecKeys, timeSec);
            break;
        case AnimChannelPath::Rotation:
            if (!ch.quatKeys.empty())
                b.rotation = sampleQuatChannel(ch.quatKeys, timeSec);
            break;
        case AnimChannelPath::Scale:
            if (!ch.vecKeys.empty())
                b.scale = sampleVec3Channel(ch.vecKeys, timeSec);
            break;
        }
    }
}

/// Only applies channels targeting bones in `needed` (skips unrelated branches).
inline void applyClipChannelsForBones(
    const AnimationClipData& clip,
    float timeSec,
    std::vector<BoneLocalTRS>& io,
    const std::unordered_set<int>& needed) {
    for (const ClipBoneChannel& ch : clip.channels) {
        if (needed.count(ch.boneIndex) == 0)
            continue;
        if (ch.boneIndex < 0 || static_cast<size_t>(ch.boneIndex) >= io.size())
            continue;
        BoneLocalTRS& b = io[static_cast<size_t>(ch.boneIndex)];
        switch (ch.path) {
        case AnimChannelPath::Translation:
            if (!ch.vecKeys.empty())
                b.translation = sampleVec3Channel(ch.vecKeys, timeSec);
            break;
        case AnimChannelPath::Rotation:
            if (!ch.quatKeys.empty())
                b.rotation = sampleQuatChannel(ch.quatKeys, timeSec);
            break;
        case AnimChannelPath::Scale:
            if (!ch.vecKeys.empty())
                b.scale = sampleVec3Channel(ch.vecKeys, timeSec);
            break;
        }
    }
}

inline void blendLocalPoses(
    const std::vector<BoneLocalTRS>& a,
    const std::vector<BoneLocalTRS>& b,
    float alpha,
    std::vector<BoneLocalTRS>& out) {
    out.resize(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        out[i].translation = lerp(a[i].translation, b[i].translation, alpha);
        out[i].rotation = quatSlerp(a[i].rotation, b[i].rotation, alpha);
        out[i].scale = lerp(a[i].scale, b[i].scale, alpha);
    }
}

inline void blendLocalPosesPartial(
    const std::vector<BoneLocalTRS>& a,
    const std::vector<BoneLocalTRS>& b,
    float alpha,
    const std::unordered_set<int>& needed,
    std::vector<BoneLocalTRS>& out) {
    out.resize(a.size());
    for (int i : needed) {
        if (i < 0 || static_cast<size_t>(i) >= a.size())
            continue;
        size_t u = static_cast<size_t>(i);
        out[u].translation = lerp(a[u].translation, b[u].translation, alpha);
        out[u].rotation = quatSlerp(a[u].rotation, b[u].rotation, alpha);
        out[u].scale = lerp(a[u].scale, b[u].scale, alpha);
    }
}

inline void localsToGlobalMatrices(const Skeleton& sk, const std::vector<BoneLocalTRS>& locals, std::vector<Mat4>& global) {
    global.resize(sk.bones.size());
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        Mat4 local = Mat4::FromTRS(locals[i].translation, locals[i].rotation, locals[i].scale);
        int p = sk.bones[i].parentIndex;
        if (p < 0 || static_cast<size_t>(p) >= sk.bones.size())
            global[i] = local;
        else
            global[i] = mat4Mul(global[static_cast<size_t>(p)], local);
    }
}

/// World matrix for one bone: locals multiplied along the same root→bone chain as `boneChainRootFirst`.
inline Mat4 globalMatrixAlongBonePath(
    const Skeleton& sk,
    const std::vector<BoneLocalTRS>& locals,
    int boneIndex) {
    std::vector<int> path;
    if (!boneChainRootFirst(sk, boneIndex, path))
        return Mat4::Identity();
    Mat4 acc = Mat4::Identity();
    for (int idx : path) {
        Mat4 L = Mat4::FromTRS(
            locals[static_cast<size_t>(idx)].translation,
            locals[static_cast<size_t>(idx)].rotation,
            locals[static_cast<size_t>(idx)].scale);
        acc = mat4Mul(acc, L);
    }
    return acc;
}

/// Skinning (global * inverseBind) and vertex deformation are intentionally not done here:
/// the render path uploads global bone transforms and inverse bind matrices from the asset;
/// the GPU applies weights from imported vertex data.

/// Samples rest + clips only for bones on paths to `syncBoneIndices` (ancestors + leaves).
/// Writes world matrices only for synced indices. Sibling subtrees (e.g. bones 6–7 if only 5 is synced) are skipped.
inline void sampleAnimationBlended(
    const ModelAsset& model,
    int clipA,
    float timeA,
    int clipB,
    float timeB,
    float blendAlpha,
    bool loopA,
    bool loopB,
    const std::vector<int>& syncBoneIndices,
    std::unordered_map<int, Mat4>& outGlobalPoseByBoneIndex) {
    const Skeleton& sk = model.skeleton;
    if (sk.bones.empty())
        return;

    std::unordered_set<int> needed = bonesNeededForSyncIndices(sk, syncBoneIndices);
    if (needed.empty()) {
        outGlobalPoseByBoneIndex.clear();
        return;
    }

    std::vector<BoneLocalTRS> localA, localB, localM;
    initLocalsFromRestPartial(sk, needed, localA);
    initLocalsFromRestPartial(sk, needed, localB);

    if (clipA >= 0 && clipA < static_cast<int>(model.clips.size())) {
        const AnimationClipData& c = model.clips[static_cast<size_t>(clipA)];
        float t = animationTimeClamp(timeA, c.durationSec, loopA);
        applyClipChannelsForBones(c, t, localA, needed);
    }
    if (clipB >= 0 && clipB < static_cast<int>(model.clips.size())) {
        const AnimationClipData& c = model.clips[static_cast<size_t>(clipB)];
        float t = animationTimeClamp(timeB, c.durationSec, loopB);
        applyClipChannelsForBones(c, t, localB, needed);
    }

    blendLocalPosesPartial(localA, localB, blendAlpha, needed, localM);

    outGlobalPoseByBoneIndex.clear();
    for (int bi : syncBoneIndices) {
        if (bi < 0 || static_cast<size_t>(bi) >= sk.bones.size())
            continue;
        outGlobalPoseByBoneIndex[bi] = globalMatrixAlongBonePath(sk, localM, bi);
    }
}

/// Full skeleton sample: every bone gets a global matrix (skeleton-local). Use for GPU skinning palettes.
inline void sampleAnimationBlendedFull(
    const ModelAsset& model,
    int clipA,
    float timeA,
    int clipB,
    float timeB,
    float blendAlpha,
    bool loopA,
    bool loopB,
    std::vector<Mat4>& outGlobal) {
    const Skeleton& sk = model.skeleton;
    if (sk.bones.empty())
        return;

    std::vector<BoneLocalTRS> localA, localB, localM;
    initLocalsFromRest(sk, localA);
    initLocalsFromRest(sk, localB);

    if (clipA >= 0 && clipA < static_cast<int>(model.clips.size())) {
        const AnimationClipData& c = model.clips[static_cast<size_t>(clipA)];
        float t = animationTimeClamp(timeA, c.durationSec, loopA);
        applyClipChannels(c, t, localA);
    }
    if (clipB >= 0 && clipB < static_cast<int>(model.clips.size())) {
        const AnimationClipData& c = model.clips[static_cast<size_t>(clipB)];
        float t = animationTimeClamp(timeB, c.durationSec, loopB);
        applyClipChannels(c, t, localB);
    }

    blendLocalPoses(localA, localB, blendAlpha, localM);
    localsToGlobalMatrices(sk, localM, outGlobal);
}

/// Per-joint skin matrix for glTF-style GPU skinning: `globalBone * inverseBind`.
inline void computeSkinMatrices(
    const Skeleton& sk,
    const std::vector<Mat4>& global,
    std::vector<Mat4>& outSkin) {
    outSkin.resize(sk.bones.size());
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        const Mat4& g = (i < global.size()) ? global[i] : Mat4::Identity();
        outSkin[i] = mat4Mul(g, sk.bones[i].inverseBind);
    }
}

inline bool sameSortedSyncIndices(const std::vector<int>& a, const std::vector<int>& b) {
    if (a.size() != b.size())
        return false;
    std::vector<int> sa = a;
    std::vector<int> sb = b;
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    return sa == sb;
}

inline bool floatApproxEqual(float x, float y, float eps = 1e-4f) {
    return std::abs(x - y) < eps;
}

inline bool vec3ApproxEqual(const Vec3& a, const Vec3& b, float eps = 1e-5f) {
    Vec3 d = a - b;
    return lengthSquared(d) < eps * eps;
}

inline bool quatApproxEqualOrientation(const Quat& a, const Quat& b) {
    float d = std::abs(quatDot(quatNormalize(a), quatNormalize(b)));
    return d > 0.99999f;
}
