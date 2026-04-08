#pragma once

#include "../components/ColliderFilterComponent.hpp"
#include "../ecs/Entity.hpp"
#include "../ecs/Registry.hpp"
#include <cstdint>

inline bool collisionLayersInteract(uint32_t aCategory, uint32_t aCollideMask, uint32_t bCategory, uint32_t bCollideMask)
{
    return (aCategory & bCollideMask) != 0 && (bCategory & aCollideMask) != 0;
}

inline bool raycastLayerTest(uint32_t colliderCategoryBits, uint32_t rayLayerMask)
{
    return (colliderCategoryBits & rayLayerMask) != 0;
}

inline bool entitiesCollideByLayer(Registry& registry, Entity a, Entity b)
{
    uint32_t ac = 0xFFFFFFFFu;
    uint32_t am = 0xFFFFFFFFu;
    uint32_t bc = 0xFFFFFFFFu;
    uint32_t bm = 0xFFFFFFFFu;
    if (registry.hasComponent<ColliderFilterComponent>(a)) {
        const auto& f = registry.getComponent<ColliderFilterComponent>(a);
        ac = f.categoryBits;
        am = f.collideMask;
    }
    if (registry.hasComponent<ColliderFilterComponent>(b)) {
        const auto& f = registry.getComponent<ColliderFilterComponent>(b);
        bc = f.categoryBits;
        bm = f.collideMask;
    }
    return collisionLayersInteract(ac, am, bc, bm);
}
