#pragma once

#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

struct AttachComponent {
    Entity targetEntity;
    Entity socketEntity;

    Vec3 offset;
    Quat rotationOffset;

    bool inheritPosition{true};
    bool inheritRotation{true};
};