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
    float followLerp{10.0f};         // smoothing strength

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

    float orbitSensitivity{2.5f};
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
};