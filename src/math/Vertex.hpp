#pragma once
#include <cmath>
#include <algorithm>

struct Vertex {
    float x = 0, y = 0, z = 0;
    float nx = 0, ny = 0, nz = 1;
    float u = 0, v = 0;
    /// glTF TANGENT (xyz + handedness w); default is a valid fallback when absent.
    float tx = 1, ty = 0, tz = 0, tw = 1;
    /// GPU skinning: joint indices + weights (see SkinnedMeshComponent + ModelAsset::meshes).
    int boneIndex[4] = {-1, -1, -1, -1};
    float boneWeight[4] = {0, 0, 0, 0};

    void addBoneInfluence(int index, float w) {
        if (w <= 1e-8f || index < 0)
            return;
        for (int i = 0; i < 4; ++i) {
            if (boneIndex[i] < 0) {
                boneIndex[i] = index;
                boneWeight[i] = w;
                return;
            }
        }
        int minSlot = 0;
        for (int i = 1; i < 4; ++i) {
            if (boneWeight[i] < boneWeight[minSlot])
                minSlot = i;
        }
        if (w > boneWeight[minSlot]) {
            boneIndex[minSlot] = index;
            boneWeight[minSlot] = w;
        }
    }

    void normalizeWeights() {
        float s = 0;
        for (int i = 0; i < 4; ++i)
            s += boneWeight[i];
        if (s > 1e-8f) {
            float inv = 1.0f / s;
            for (int i = 0; i < 4; ++i)
                boneWeight[i] *= inv;
        }
    }
};
