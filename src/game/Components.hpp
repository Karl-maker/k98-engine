#pragma once

#include "../statemachine/StateMachine.hpp"

struct Position
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Velocity
{
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
};

struct Health
{
    int hp = 100;
};
