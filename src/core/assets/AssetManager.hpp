#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <iostream>
#include "IAsset.hpp"
#include "ImporterRegistry.hpp"

class AssetManager {
private:
    std::unordered_map<std::string, std::shared_ptr<IAsset>> cache;
    ImporterRegistry registry;

public:
    void registerImporter(const std::string& ext,
                          std::shared_ptr<IAssetImporter> importer) {
        registry.registerImporter(ext, importer);
    }

    std::shared_ptr<IAsset> load(const std::string& path) {
        // cache check
        if (cache.count(path)) {
            return cache[path];
        }

        // get extension
        auto dot = path.find_last_of('.');
        std::string ext = path.substr(dot + 1);

        auto importer = registry.getImporter(ext);
        if (!importer) {
            std::cerr << "No importer for ." << ext << std::endl;
            return nullptr;
        }

        auto asset = importer->import(path);
        asset->id = path;

        cache[path] = asset;
        return asset;
    }
};