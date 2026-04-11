#pragma once

#include "ISpawnArchetypeFactory.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace spawn {

class SpawnFactoryRegistry {
public:
    void registerArchetype(const std::string& name, std::unique_ptr<ISpawnArchetypeFactory> factory)
    {
        if (!factory)
            return;
        m_factories[name] = std::move(factory);
    }

    ISpawnArchetypeFactory* tryGet(const std::string& name)
    {
        auto it = m_factories.find(name);
        if (it == m_factories.end())
            return nullptr;
        return it->second.get();
    }

private:
    std::unordered_map<std::string, std::unique_ptr<ISpawnArchetypeFactory>> m_factories;
};

} // namespace spawn
