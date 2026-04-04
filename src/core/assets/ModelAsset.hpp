#pragma once
#include "IAsset.hpp"
#include "../math/Vertex.hpp"
#include <vector>

class ModelAsset : public IAsset {
public:
    std::vector<Vertex> vertices;
};