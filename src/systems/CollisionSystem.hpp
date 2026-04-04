#pragma once
#include "../ecs/Registry.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../utils/CollisionUtils.hpp"
#include "../utils/ProximityUtils.hpp"

#include <functional>
#include <vector>

class CollisionSystem {
public:

    using Handler = std::function<void(const CollisionEvent&)>;

    // =========================
    // Register handlers
    // =========================
    void addHandler(Handler handler)
    {
        handlers.push_back(handler);
    }

    void update(Registry& registry, SpatialGridSystem& grid)
    {
        auto entities = registry.getEntitiesWith<CollisionBoxComponent, TransformComponent>();
    
        std::vector<Entity> movedEntities;
    
        // =========================
        // STEP 1: Update bounds + movement
        // =========================
        for (auto e : entities)
        {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);
            auto& transform = registry.getComponent<TransformComponent>(e);
    
            box.min = transform.position - box.halfSize;
            box.max = transform.position + box.halfSize;
    
            if (ProximityUtils::distanceSquared(transform.position, box.lastPosition) > 0.0001f)
            {
                movedEntities.push_back(e);
                box.lastPosition = transform.position;
            }
    
            box.previousTouching = box.touching;
            box.touching.clear();
        }

        grid.update(registry);

        // =========================
        // STEP 2: Spatial collision checks
        // =========================
        for (auto e : entities)
        {
            auto& boxA = registry.getComponent<CollisionBoxComponent>(e);
    
            // 🔥 KEY CHANGE: query using AABB (multi-cell aware)
            auto nearby = grid.getNearby(boxA.min, boxA.max);
    
            for (auto other : nearby)
            {
                // avoid self + duplicate pairs
                if (e >= other) continue;
    
                auto& boxB = registry.getComponent<CollisionBoxComponent>(other);
    
                // skip static-static
                if (boxA.isStatic && boxB.isStatic)
                    continue;
    
                // skip if neither moved
                if (!hasMoved(e, movedEntities) && !hasMoved(other, movedEntities))
                    continue;
    
                // (optional but cheap) broad phase
                if (!broadPhase(boxA, boxB))
                    continue;
    
                // narrow phase
                if (CollisionUtils::aabbAabb(
                        boxA.min, boxA.max,
                        boxB.min, boxB.max))
                {
                    boxA.touching.push_back(other);
                    boxB.touching.push_back(e);
                }
            }
        }
    
        // =========================
        // STEP 3: Emit events
        // =========================
        for (auto e : entities)
        {
            auto& box = registry.getComponent<CollisionBoxComponent>(e);
            processEvents(e, box);
        }
    }

private:
    std::vector<Handler> handlers;

    void emit(const CollisionEvent& event)
    {
        for (auto& h : handlers)
            h(event);
    }

    void processEvents(Entity self, CollisionBoxComponent& box)
    {
        // ENTER
        for (auto other : box.touching)
        {
            if (!contains(box.previousTouching, other))
            {
                emit({self, other, CollisionEventType::Enter});
            }
        }

        // EXIT
        for (auto other : box.previousTouching)
        {
            if (!contains(box.touching, other))
            {
                emit({self, other, CollisionEventType::Exit});
            }
        }

        // STAY
        for (auto other : box.touching)
        {
            if (contains(box.previousTouching, other))
            {
                emit({self, other, CollisionEventType::Stay});
            }
        }
    }

    bool hasMoved(Entity e, const std::vector<Entity>& moved)
    {
        for (auto m : moved)
            if (m == e) return true;
        return false;
    }

    bool contains(const std::vector<Entity>& list, Entity e)
    {
        for (auto x : list)
            if (x == e) return true;
        return false;
    }

    bool broadPhase(const CollisionBoxComponent& a, const CollisionBoxComponent& b)
    {
        Vec3 centerA = (a.min + a.max) * 0.5f;
        Vec3 centerB = (b.min + b.max) * 0.5f;

        float maxDist =
            (a.halfSize.x + b.halfSize.x) +
            (a.halfSize.y + b.halfSize.y) +
            (a.halfSize.z + b.halfSize.z);

        return ProximityUtils::distanceSquared(centerA, centerB) <= (maxDist * maxDist);
    }
};