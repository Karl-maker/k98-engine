#pragma once
#include "../math/Mat4.hpp"

struct WorldTransformComponent {
    Mat4 world;
    bool dirty{true};
};