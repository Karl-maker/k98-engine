#pragma once
#include <memory>
#include <string>
#include "../Asset.hpp"

class IAssetImporter {
public:
    virtual ~IAssetImporter() = default;

    virtual std::shared_ptr<IAsset> import(const std::string& path) = 0;
};