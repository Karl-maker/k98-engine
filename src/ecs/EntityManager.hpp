#pragma once
#include <queue>
#include <array>
#include <bitset>
#include <cassert>
#include <vector>
#include <algorithm>

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

    // -------------------------
    // CREATE
    // -------------------------
    Entity createEntity()
    {
        assert(livingEntityCount < MAX_ENTITIES && "Too many entities");

        Entity id = availableEntities.front();
        availableEntities.pop();

        livingEntities.push_back(id);
        ++livingEntityCount;

        return id;
    }

    // -------------------------
    // DESTROY
    // -------------------------
    void destroyEntity(Entity entity)
    {
        assert(entity < MAX_ENTITIES && "Invalid entity");

        signatures[entity].reset();

        auto it = std::find(livingEntities.begin(), livingEntities.end(), entity);
        if (it != livingEntities.end())
        {
            std::swap(*it, livingEntities.back());
            livingEntities.pop_back();
        }

        availableEntities.push(entity);
        --livingEntityCount;
    }

    // -------------------------
    // SIGNATURE (FIXED)
    // -------------------------

    void setSignature(Entity entity, const Signature& signature)
    {
        signatures[entity] = signature;
    }

    const Signature& getSignature(Entity entity) const
    {
        return signatures[entity];
    }

    Signature& getSignature(Entity entity)
    {
        return signatures[entity];
    }

    // -------------------------
    // ACTIVE ENTITIES
    // -------------------------
    const std::vector<Entity>& getAliveEntities() const
    {
        return livingEntities;
    }

private:
    std::queue<Entity> availableEntities{};
    std::array<Signature, MAX_ENTITIES> signatures{};
    std::vector<Entity> livingEntities;

    uint32_t livingEntityCount = 0;
};