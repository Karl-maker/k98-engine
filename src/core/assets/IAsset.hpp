#pragma once
#include <memory>
#include <string>

class IAsset {
public:
    virtual ~IAsset() = default;

    std::string id;   // unique identifier (path or hash)
};

template<typename T>
struct AssetHandle {
    std::shared_ptr<T> asset;
};