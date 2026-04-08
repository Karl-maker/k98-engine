#pragma once

#include "IAsset.hpp"
#include "AnimationClipData.hpp"
#include "../../math/Vertex.hpp"
#include "../../math/VertexBoneData.hpp"
#include "../../math/Mat4.hpp"
#include "../../math/Quat.hpp"
#include <vector>
#include <string>
#include <unordered_map>

struct Material {
    /// Resolved filesystem paths (directory of .gltf + image uri).
    std::string albedoTexture;
    std::string normalTexture;
    std::string occlusionTexture;
    std::string metallicRoughnessTexture;
};

struct Mesh {
    std::vector<Vertex> vertices;
    /// Parallel to `vertices` (same length after load); joint indices + weights for GPU skinning.
    std::vector<VertexBoneData> boneData;
    std::vector<int> indices;
    int materialIndex = 0;
};

struct Bone {
    std::string name;
    /// Parent bone index into `Skeleton::bones`, or -1 for root. Hierarchy is defined only by this
    /// field — bones may appear in any order in the vector (e.g. glTF joint order).
    int parentIndex = -1;
    /// From glTF; used on GPU with animated globals — not multiplied on CPU for skinning.
    Mat4 inverseBind = Mat4::Identity();
    Vec3 restTranslation{};
    Quat restRotation{0, 0, 0, 1};
    Vec3 restScale{1, 1, 1};
};

struct Skeleton {
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> boneMap;
};

class ModelAsset : public IAsset {
public:
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    Skeleton skeleton;
    std::vector<AnimationClipData> clips;

    /// Drop CPU vertex/index arrays after GPU upload; keeps materials + skeleton + clips for animation.
    void releaseMeshGeometry()
    {
        for (Mesh& m : meshes) {
            m.vertices.clear();
            m.vertices.shrink_to_fit();
            m.boneData.clear();
            m.boneData.shrink_to_fit();
            m.indices.clear();
            m.indices.shrink_to_fit();
        }
    }
};
