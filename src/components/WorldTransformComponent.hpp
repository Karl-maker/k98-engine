#pragma once

#include "../math/Mat4.hpp"

struct WorldTransformComponent {
    Mat4 world = Mat4::Identity();
};
