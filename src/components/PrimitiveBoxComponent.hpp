#pragma once

#include "../math/Vec3.hpp"

/// Unlit solid box drawn with `OpenGLVer2Renderer` (unit cube scaled by `halfExtents * 2`).
struct PrimitiveBoxComponent {
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    float color[3]{0.42f, 0.4f, 0.38f};
};
