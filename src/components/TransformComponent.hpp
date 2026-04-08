#pragma once
#include "math/Vec3.hpp"
#include "math/Quat.hpp"

struct TransformComponent
{
    Vec3 position {0.f, 0.f, 0.f};
    Quat rotation {0.f, 0.f, 0.f, 1.f};
    Vec3 scale {1.f, 1.f, 1.f};
};