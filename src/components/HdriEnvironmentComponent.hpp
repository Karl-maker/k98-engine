#pragma once

#include <string>

struct HdriEnvironmentComponent {
    std::string hdriAssetPath;
    bool enabled = true;
    float intensity = 1.f;
    float rotationY = 0.f;
    float diffuseEnvironmentWeight = 1.f;
    float specularEnvironmentWeight = 1.f;
};
