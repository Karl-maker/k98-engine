#pragma once

#include "../ecs/Registry.hpp"

#include <string>

struct StateChangedEvent
{
    Entity entity = 0;
    std::string fromState;
    std::string toState;
};

struct StateEnteredEvent
{
    Entity entity = 0;
    std::string state;
};

struct StateExitedEvent
{
    Entity entity = 0;
    std::string state;
};