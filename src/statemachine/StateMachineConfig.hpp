#pragma once

#include "StateMachineTypes.hpp"

#include <string>
#include <unordered_map>

struct StateMachineConfig
{
    std::string initialState;
    std::unordered_map<std::string, StateDefinition> states;
};