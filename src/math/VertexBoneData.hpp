#pragma once

#include <cmath>

/// Up to four joint indices + weights per vertex (glTF JOINTS_0 / WEIGHTS_0). Plain arrays for
/// `offsetof` / GPU interleaving with `MeshVertexStream`.
struct VertexBoneData {
    int boneIndices[4] = {-1, -1, -1, -1};
    float weights[4] = {0.f, 0.f, 0.f, 0.f};

    void addBoneInfluence(int index, float w)
    {
        if (w <= 1e-8f || index < 0)
            return;
        for (int i = 0; i < 4; ++i) {
            if (boneIndices[i] < 0) {
                boneIndices[i] = index;
                weights[i] = w;
                return;
            }
        }
        int minSlot = 0;
        for (int i = 1; i < 4; ++i) {
            if (weights[i] < weights[minSlot])
                minSlot = i;
        }
        if (w > weights[minSlot]) {
            boneIndices[minSlot] = index;
            weights[minSlot] = w;
        }
    }

    void normalizeWeights()
    {
        float s = 0.f;
        for (int i = 0; i < 4; ++i)
            s += weights[i];
        if (s > 1e-8f) {
            float inv = 1.0f / s;
            for (int i = 0; i < 4; ++i)
                weights[i] *= inv;
        }
    }
};
