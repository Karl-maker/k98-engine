#pragma once

#include "../math/Vec3.hpp"

enum class LightType : int {
    Directional = 0,
    Point = 1,
    Spot = 2,
    Ambient = 3,
};

struct LightingComponent {
    bool enabled = true;
    LightType type = LightType::Directional;
    Vec3 color{1.f, 1.f, 1.f};
    float intensity = 1.f;
    float range = 100.f;
    Vec3 worldDirectionOverride{0.f, 0.f, 0.f};
    bool useEntityAxis = true;
    float spotInnerDegrees = 30.f;
    float spotOuterDegrees = 45.f;
    float attenConstant = 1.f;
    float attenLinear = 0.f;
    float attenQuadratic = 0.f;
    float specularPower = 32.f;
    float specularIntensity = 1.f;
    bool useHalfLambert = true;
    float rimPower = 4.f;
    float rimIntensity = 0.f;
};


