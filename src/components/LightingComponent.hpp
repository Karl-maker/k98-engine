#pragma once

#include "../math/Vec3.hpp"
#include <cstdint>

// -----------------------------------------------------------------------------
// LightingComponent — evaluated in OpenGLRenderSystem (fragment shader). Entity
// should have WorldTransformComponent (socket attach, hierarchy, or root transform).
//
// Light types:
//   Directional — infinite; axis from world matrix +Z column (or worldDirectionOverride).
//   Point       — position from world matrix; inverse-square attenuation + range falloff.
//   Spot        — point + cone along forward axis; inner/outer cone in degrees.
//   Ambient     — no direction; adds to scene ambient (hemisphere-style fill).
//
// Register: registry.registerComponent<LightingComponent>();
// -----------------------------------------------------------------------------

enum class LightType : std::uint8_t {
    Directional = 0,
    Point       = 1,
    Spot        = 2,
    Ambient     = 3,
};

struct LightingComponent {
    bool enabled = true;

    LightType type = LightType::Point;

    Vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.5f;

    /// If length² > 1e-8, normalized and used as light **forward** (direction the beam travels) for
    /// Directional / Spot when useEntityAxis is false. Ignored when useEntityAxis is true.
    Vec3 worldDirectionOverride = {0.0f, 0.0f, 0.0f};

    /// If true, forward axis = normalized world matrix column 2 (indices 8–10) — typical glTF +Z.
    bool useEntityAxis = true;

    /// Max distance for point/spot (smooth falloff to zero at edge).
    float range = 12.0f;

    float attenConstant = 1.0f;
    float attenLinear = 0.35f;
    float attenQuadratic = 0.44f;

    float spotInnerDegrees = 28.0f;
    float spotOuterDegrees = 45.0f;

    /// Blinn–Phong specular (mesh materials are roughness-based; spec scales with (1−roughness) in shader).
    float specularPower = 48.0f;
    float specularIntensity = 0.4f;

    /// >0.5: half-Lambert diffuse wrap; else standard Lambert.
    bool useHalfLambert = true;

    float rimIntensity = 0.0f;
    float rimPower = 4.0f;
};
