#pragma once
#include "EntityManager.hpp"
#include "ComponentManager.hpp"

class Registry
{
public:
    Entity createEntity()
    {
        return entityManager.createEntity();
    }

    void destroyEntity(Entity entity)
    {
        entityManager.destroyEntity(entity);
    }

    template<typename T>
    void registerComponent()
    {
        componentManager.registerComponent<T>();
    }

    template<typename T>
    void addComponent(Entity entity, const T& component)
    {
        componentManager.addComponent<T>(entity, component);

        auto signature = entityManager.getSignature(entity);
        signature.set(componentManager.getComponentType<T>(), true);
        entityManager.setSignature(entity, signature);
    }

    template<typename T>
    void removeComponent(Entity entity)
    {
        componentManager.removeComponent<T>(entity);

        auto signature = entityManager.getSignature(entity);
        signature.set(componentManager.getComponentType<T>(), false);
        entityManager.setSignature(entity, signature);
    }

    template<typename T>
    T& getComponent(Entity entity)
    {
        return componentManager.getComponent<T>(entity);
    }

private:
    EntityManager entityManager;
    ComponentManager componentManager;
};