#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

struct SocketComponent {
    Entity parentEntity;
    Vec3 localOffset;
    Quat localRotation;

    Mat4 worldTransform;
};