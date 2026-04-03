#pragma once
#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

struct TransformComponent {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1,1,1};

    Entity parent = static_cast<Entity>(-1); // no parent
};