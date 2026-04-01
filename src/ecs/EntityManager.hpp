#pragma once
#include <queue>
#include <array>
#include <bitset>
#include <cassert>
#include "Entity.hpp"

using Signature = std::bitset<MAX_COMPONENTS>;

class EntityManager
{
public:
    EntityManager()
    {
        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
        {
            availableEntities.push(entity);
        }
    }

    Entity createEntity()
    {
        assert(livingEntityCount < MAX_ENTITIES && "Too many entities");

        Entity id = availableEntities.front();
        availableEntities.pop();
        ++livingEntityCount;

        return id;
    }

    void destroyEntity(Entity entity)
    {
        assert(entity < MAX_ENTITIES && "Invalid entity");

        signatures[entity].reset();
        availableEntities.push(entity);
        --livingEntityCount;
    }

    void setSignature(Entity entity, Signature signature)
    {
        signatures[entity] = signature;
    }

    Signature getSignature(Entity entity)
    {
        return signatures[entity];
    }

private:
    std::queue<Entity> availableEntities{};
    std::array<Signature, MAX_ENTITIES> signatures{};
    uint32_t livingEntityCount = 0;
};