#pragma once
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <cassert>
#include "ComponentArray.hpp"

class ComponentManager
{
public:
    template<typename T>
    void registerComponent()
    {
        std::type_index typeName = typeid(T);

        assert(componentTypes.find(typeName) == componentTypes.end() && "Component already registered");

        componentTypes[typeName] = nextComponentType;
        componentArrays[typeName] = std::make_shared<ComponentArray<T>>();

        ++nextComponentType;
    }

    template<typename T>
    size_t getComponentType()
    {
        std::type_index typeName = typeid(T);

        assert(componentTypes.find(typeName) != componentTypes.end() && "Component not registered");

        return componentTypes[typeName];
    }

    template<typename T>
    void addComponent(Entity entity, const T& component)
    {
        getArray<T>()->insertData(entity, component);
    }

    template<typename T>
    void removeComponent(Entity entity)
    {
        getArray<T>()->removeData(entity);
    }

    template<typename T>
    T& getComponent(Entity entity)
    {
        return getArray<T>()->getData(entity);
    }

private:
    std::unordered_map<std::type_index, size_t> componentTypes;
    std::unordered_map<std::type_index, std::shared_ptr<void>> componentArrays;
    size_t nextComponentType = 0;

    template<typename T>
    std::shared_ptr<ComponentArray<T>> getArray()
    {
        std::type_index typeName = typeid(T);

        assert(componentArrays.find(typeName) != componentArrays.end() && "Component not registered");

        return std::static_pointer_cast<ComponentArray<T>>(componentArrays[typeName]);
    }
};