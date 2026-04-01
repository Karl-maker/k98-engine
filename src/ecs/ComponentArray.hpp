#pragma once
#include <vector>
#include <unordered_map>
#include <cassert>
#include "Entity.hpp"

template<typename T>
class ComponentArray
{
public:
    ComponentArray()
    {
        components.reserve(MAX_ENTITIES); // prevent reallocations
    }

    void insertData(Entity entity, const T& component)
    {
        assert(entityToIndex.find(entity) == entityToIndex.end() && "Component added twice");

        size_t newIndex = components.size();

        entityToIndex[entity] = newIndex;
        indexToEntity[newIndex] = entity;

        components.push_back(component);
    }

    void removeData(Entity entity)
    {
        assert(entityToIndex.find(entity) != entityToIndex.end() && "Removing non-existent component");

        size_t indexOfRemoved = entityToIndex[entity];
        size_t indexOfLast = components.size() - 1;

        // Move last element into removed spot
        components[indexOfRemoved] = components[indexOfLast];

        Entity lastEntity = indexToEntity[indexOfLast];
        entityToIndex[lastEntity] = indexOfRemoved;
        indexToEntity[indexOfRemoved] = lastEntity;

        // Remove last
        components.pop_back();

        entityToIndex.erase(entity);
        indexToEntity.erase(indexOfLast);
    }

    T& getData(Entity entity)
    {
        assert(entityToIndex.find(entity) != entityToIndex.end() && "Component not found");
        return components[entityToIndex[entity]];
    }

    bool hasData(Entity entity) const
    {
        return entityToIndex.find(entity) != entityToIndex.end();
    }

private:
    std::vector<T> components;
    std::unordered_map<Entity, size_t> entityToIndex;
    std::unordered_map<size_t, Entity> indexToEntity;
};