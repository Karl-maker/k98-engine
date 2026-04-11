#pragma once

#include "SpawnTypes.hpp"

#include "../../ecs/Entity.hpp"

#include <string>
#include <vector>

namespace spawn {

class ISpawnListener {
public:
    virtual ~ISpawnListener() = default;
    virtual void onSpawn(const std::string& entryId, const SpawnEntryDesc& desc, const std::vector<Entity>& entities)
    {
        (void)entryId;
        (void)desc;
        (void)entities;
    }
    virtual void onDespawn(const std::string& entryId) { (void)entryId; }
};

class SpawnEventBus {
public:
    void addListener(ISpawnListener* listener)
    {
        if (listener)
            m_listeners.push_back(listener);
    }

    void notifySpawn(const std::string& entryId, const SpawnEntryDesc& desc, const std::vector<Entity>& entities)
    {
        for (ISpawnListener* l : m_listeners)
            l->onSpawn(entryId, desc, entities);
    }

    void notifyDespawn(const std::string& entryId)
    {
        for (ISpawnListener* l : m_listeners)
            l->onDespawn(entryId);
    }

private:
    std::vector<ISpawnListener*> m_listeners;
};

} // namespace spawn
