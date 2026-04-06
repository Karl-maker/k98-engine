#pragma once
#include "../ecs/Entity.hpp"
#include <functional>

struct SequenceAction {
    float time;
    bool executed{false};

    // optional target
    Entity target = static_cast<Entity>(-1);

    // the action
    std::function<void(Entity)> action;
};