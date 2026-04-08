#pragma once

#include "Vec2.hpp"
#include "Vec3.hpp"

struct Vertex {
    Vec3 position{};
    Vec3 normal{0.f, 0.f, 1.f};
    Vec2 uv{};
    /// glTF TANGENT xyz; handedness lives in `tangentW`.
    Vec3 tangent{1.f, 0.f, 0.f};
    float tangentW = 1.f;
};
