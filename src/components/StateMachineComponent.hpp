#pragma once

#include "../statemachine/StateMachine.hpp"

// -----------------------------------------------------------------------------
// StateMachineComponent — wraps `StateMachine` (states, transitions, events).
// Initialize with `machine.initialize(entity, config)` from game setup code.
//
// Register: registry.registerComponent<StateMachineComponent>();
// See StateMachine.hpp / StateMachineTypes.hpp for transition tables and callbacks.
// -----------------------------------------------------------------------------

struct StateMachineComponent
{
    StateMachine machine;
};