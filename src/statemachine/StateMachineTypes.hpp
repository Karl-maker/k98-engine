#pragma once

#include "../ecs/Registry.hpp"

#include <functional>
#include <string>
#include <vector>

enum class StateEventType
{
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Stop,
    UserDetected
};

// Guard = optional condition
using StateGuard = std::function<bool(Entity)>;

// Actions = behavior hooks
using StateEnterAction = std::function<void(Entity)>;
using StateUpdateAction = std::function<void(Entity, double)>;
using StateExitAction = std::function<void(Entity)>;

// 🔥 Event-based transition
struct EventTransition
{
    StateEventType event;
    std::string toState;
    StateGuard guard;
};

// State definition
struct StateDefinition
{
    std::string name;

    StateEnterAction onEnter;
    StateUpdateAction onUpdate;
    StateExitAction onExit;

    std::vector<EventTransition> eventTransitions;
};