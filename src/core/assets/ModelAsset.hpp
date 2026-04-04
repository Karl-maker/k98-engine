#pragma once
#include "IAsset.hpp"
#include "../math/Vertex.hpp"
#include "../math/Mat4.hpp"
#include <vector>
#include <string>
#include <unordered_map>

struct Material {
    std::string albedoTexture;
    std::string normalTexture;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    int materialIndex;
};

struct Bone {
    std::string name;
    int parentIndex;
    Mat4 inverseBind;
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

    // animations etc
};