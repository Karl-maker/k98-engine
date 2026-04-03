#pragma once
#include <vector>
#include <array>
#include <cassert>
#include "Entity.hpp"

template<typename T>
class ComponentArray
{
public:
    ComponentArray()
    {
        components.reserve(MAX_ENTITIES);
        entityToIndex.fill(INVALID_INDEX);
        indexToEntity.fill(INVALID_ENTITY);
    }

    void insertData(Entity entity, const T& component)
    {
        assert(entityToIndex[entity] == INVALID_INDEX && "Component added twice");

        size_t newIndex = size;

        entityToIndex[entity] = newIndex;
        indexToEntity[newIndex] = entity;

        if (newIndex >= components.size())
            components.push_back(component);
        else
            components[newIndex] = component;

        size++;
    }

    void removeData(Entity entity)
    {
        assert(entityToIndex[entity] != INVALID_INDEX && "Removing non-existent component");

        size_t removedIndex = entityToIndex[entity];
        size_t lastIndex = size - 1;

        components[removedIndex] = components[lastIndex];

        Entity lastEntity = indexToEntity[lastIndex];

        entityToIndex[lastEntity] = removedIndex;
        indexToEntity[removedIndex] = lastEntity;

        entityToIndex[entity] = INVALID_INDEX;
        indexToEntity[lastIndex] = INVALID_ENTITY;

        size--;
    }

    T& getData(Entity entity)
    {
        assert(entityToIndex[entity] != INVALID_INDEX && "Component not found");
        return components[entityToIndex[entity]];
    }

    bool hasData(Entity entity) const
    {
        return entityToIndex[entity] != INVALID_INDEX;
    }

private:
    static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

    std::vector<T> components;

    std::array<size_t, MAX_ENTITIES> entityToIndex;
    std::array<Entity, MAX_ENTITIES> indexToEntity;

    size_t size = 0;
};