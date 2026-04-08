#pragma once

#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"

struct CameraComponent {
    Mat4 viewMatrix = Mat4::Identity();
    float fov = 60.f;
    float nearPlane = 0.1f;
    float farPlane = 500.f;
    bool active = true;

    bool enableLookAt = false;
    Entity lookAtTarget = INVALID_ENTITY;
    Vec3 lookAtOffset{0.f, 0.f, 0.f};

    bool enableLockOn = false;
    Entity lockOnTarget = INVALID_ENTITY;
};
