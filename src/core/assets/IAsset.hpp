#pragma once
#include <string>

class IAsset {
public:
    virtual ~IAsset() = default;

    std::string id;   // unique identifier (path or hash)
};