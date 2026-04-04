#pragma once
#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"

struct CameraComponent {
    // =========================
    // EXISTING (UNCHANGED)
    // =========================
    float fov{60.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};

    Mat4 viewMatrix{};
    Mat4 projectionMatrix{};

    bool active{true};

    // =========================
    // NEW (OPTIONAL FEATURES)
    // =========================

    // ---- FOLLOW ----
    bool enableFollow{false};        // default OFF (backwards compatible)
    float followLerp{10.0f};         // orbit yaw/pitch smoothing (see CameraSystem)
    float followPositionLerp{10.0f}; // exponential position smoothing (when spring off)

    // Spring follow (optional): slight overshoot when dampingRatio < 1
    bool followPositionSpring{false};
    float followSpringFrequency{3.5f};    // Hz-ish responsiveness (higher = tighter)
    float followSpringDampingRatio{1.0f}; // 1 = critical; <1 = bouncy overshoot; >1 = heavy
    Vec3 followPositionVelocity{};        // internal

    // ---- LOOK AT ----
    bool enableLookAt{false};        // default OFF
    Entity lookAtTarget{INVALID_ENTITY};
    Vec3 lookAtOffset{0.0f, 0.0f, 0.0f};

    // ---- INTERNAL / DEBUG ----
    bool debug{false};

    // =========================
    // ORBIT CONTROLS
    // =========================
    bool enableOrbit{false};

    float orbitYaw{0.0f};
    float orbitPitch{0.3f}; // slight downward angle

    float orbitSensitivity{1.0f};
    float orbitDistance{5.0f};

    // clamp pitch (avoid flipping)
    float minPitch{-1.2f};
    float maxPitch{1.2f};

    // =========================
    // LOCK ON SYSTEM
    // =========================
    bool enableLockOn{false};
    Entity lockOnTarget{INVALID_ENTITY};

    // =========================
    // INPUT (to be set externally)
    // =========================
    float inputDeltaX{0.0f};
    float inputDeltaY{0.0f};

    float currentYaw{0.0f};
    float currentPitch{0.0f};

    float inputVelocityX{0.0f};
    float inputVelocityY{0.0f};
};