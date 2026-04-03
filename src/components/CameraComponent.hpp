#pragma once
#include "../math/Mat4.hpp"

struct CameraComponent {
    float fov{60.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};

    Mat4 viewMatrix;
    Mat4 projectionMatrix;

    bool active{true};
};