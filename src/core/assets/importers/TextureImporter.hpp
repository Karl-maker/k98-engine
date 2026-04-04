#pragma once
#include "IAssetImporter.hpp"
#include "../TextureAsset.hpp"
#include <iostream>

class TextureImporter : public IAssetImporter {
public:
    std::shared_ptr<IAsset> import(const std::string& path) override {
        auto texture = std::make_shared<TextureAsset>();

        // Example (replace with stb_image)
        std::cout << "Loading texture: " << path << std::endl;

        texture->width = 512;
        texture->height = 512;
        texture->channels = 4;

        return texture;
    }
};