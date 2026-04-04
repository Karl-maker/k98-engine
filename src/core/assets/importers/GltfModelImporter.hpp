#pragma once

#include "IAssetImporter.hpp"

class GltfModelImporter : public IAssetImporter {
public:
    std::shared_ptr<IAsset> import(const std::string& path) override;
};
