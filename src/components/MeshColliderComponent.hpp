#pragma once
#include <vector>
#include "math/Vec3.hpp"
#include "math/Vertex.hpp"

struct MeshColliderComponent
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    bool isStatic = true;   // important for optimization
};