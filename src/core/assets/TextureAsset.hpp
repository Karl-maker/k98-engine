#pragma once
#include "IAsset.hpp"
#include <vector>

class TextureAsset : public IAsset {
public:
    int width;
    int height;
    int channels;
    std::vector<unsigned char> data;
};