#pragma once

#include "../../ecs/Entity.hpp"

/// Chase target on XZ; `AIControllerSystem` reads the target transform each frame.
struct ChaseComponent
{
    Entity chaseTarget = INVALID_ENTITY;
};
