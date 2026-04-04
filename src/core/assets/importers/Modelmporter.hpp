#pragma once
#include "IAssetImporter.hpp"
#include "../ModelAsset.hpp"
#include <iostream>

class ModelImporter : public IAssetImporter {
public:
    std::shared_ptr<IAsset> import(const std::string& path) override {
        auto model = std::make_shared<ModelAsset>();

        std::cout << "Loading model: " << path << std::endl;

        // Example (replace with Assimp)
        model->vertices.push_back({0,0,0});
        model->vertices.push_back({1,0,0});

        // for (auto& mat : model->materials) {
        //     assetManager->loadAsync<TextureAsset>(mat.albedoTexture);
        //     assetManager->loadAsync<TextureAsset>(mat.normalTexture);
        // }

        return model;
    }
};