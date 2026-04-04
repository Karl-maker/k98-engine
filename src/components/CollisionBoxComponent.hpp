#pragma once
#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"
#include <vector>

struct CollisionBoxComponent {
    Vec3 halfSize;

    // Cached bounds
    Vec3 min;
    Vec3 max;

    // Movement cache
    Vec3 lastPosition;

    // Current + previous collisions
    std::vector<Entity> touching;
    std::vector<Entity> previousTouching;

    bool isStatic{false};
};

enum class CollisionEventType {
    Enter,
    Stay,
    Exit
};

struct CollisionEvent {
    Entity a;
    Entity b;
    CollisionEventType type;
};