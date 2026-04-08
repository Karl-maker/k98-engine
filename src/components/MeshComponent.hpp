#pragma once

#include <vector>
#include "math/Vertex.hpp"
#include "math/VertexBoneData.hpp"

struct MeshComponent
{
    std::vector<Vertex> vertices;
    std::vector<VertexBoneData> boneData;
    std::vector<uint32_t> indices;
};
