#pragma once
#include "EntityManager.hpp"
#include "ComponentManager.hpp"
#include "System.hpp"
#include <unordered_map>
#include <memory>
#include <typeindex>

class Registry
{
public:
    // -------------------------
    // ENTITY
    // -------------------------
    Entity createEntity()
    {
        return entityManager.createEntity();
    }

    void destroyEntity(Entity entity)
    {
        entityManager.destroyEntity(entity);
    }

    // -------------------------
    // COMPONENT REGISTRATION
    // -------------------------
    template<typename T>
    void registerComponent()
    {
        componentManager.registerComponent<T>();
    }

    // -------------------------
    // ADD / REMOVE
    // -------------------------
    template<typename T>
    void addComponent(Entity entity, const T& component)
    {
        componentManager.addComponent<T>(entity, component);
    
        auto& signature = entityManager.getSignature(entity);
        signature.set(componentManager.getComponentType<T>(), true);
    
        entitySignatureChanged(entity, signature);
    }

    template<typename T>
    void removeComponent(Entity entity)
    {
        componentManager.removeComponent<T>(entity);
    
        auto& signature = entityManager.getSignature(entity);
        signature.set(componentManager.getComponentType<T>(), false);
    
        entitySignatureChanged(entity, signature);
    }

    // -------------------------
    // GET
    // -------------------------
    template<typename T>
    T& getComponent(Entity entity)
    {
        return componentManager.getComponent<T>(entity);
    }

    // -------------------------
    // NEW: HAS COMPONENT (FAST)
    // -------------------------
    template<typename T>
    bool hasComponent(Entity entity)
    {
        auto signature = entityManager.getSignature(entity);
        return signature.test(componentManager.getComponentType<T>());
    }

    // -------------------------
    // NEW: FILTERED QUERY
    // -------------------------
    template<typename... Components>
    std::vector<Entity> getEntitiesWith()
    {
        std::vector<Entity> result;

        for (Entity entity : entityManager.getAliveEntities())
        {
            auto signature = entityManager.getSignature(entity);

            bool hasAll = (... && signature.test(componentManager.getComponentType<Components>()));

            if (hasAll)
            {
                result.push_back(entity);
            }
        }

        return result;
    }

    template<typename T>
    T* tryGetComponent(Entity entity)
    {
        if (!hasComponent<T>(entity)) return nullptr;
        return &getComponent<T>(entity);
    }

    template<typename T>
    std::shared_ptr<T> registerSystem()
    {
        auto type = std::type_index(typeid(T));

        auto system = std::make_shared<T>();
        systems[type] = system;

        return system;
    }

    template<typename T>
    void setSystemSignature(Signature signature)
    {
        auto type = std::type_index(typeid(T));
        systemSignatures[type] = signature;
    }

    void entitySignatureChanged(Entity entity, Signature entitySignature)
    {
        for (auto const& [type, system] : systems)
        {
            auto const& systemSignature = systemSignatures[type];

            if ((entitySignature & systemSignature) == systemSignature)
            {
                system->entities.push_back(entity);
            }
            else
            {
                auto& vec = system->entities;
                vec.erase(std::remove(vec.begin(), vec.end(), entity), vec.end());
            }
        }
    }
private:
    EntityManager entityManager;
    ComponentManager componentManager;
    std::unordered_map<std::type_index, Signature> systemSignatures;
    std::unordered_map<std::type_index, std::shared_ptr<System>> systems;
};