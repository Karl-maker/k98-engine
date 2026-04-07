#pragma once

#include "../../ecs/Registry.hpp"
#include "../../ecs/Entity.hpp"
#include "../../components/CollisionBoxComponent.hpp"
#include "../../components/StaticMeshComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/WorldTransformComponent.hpp"
#include "../../math/Quat.hpp"
#include "../../math/Vec3.hpp"

#include <string>

namespace game::factories {

/// Static world prop: textured glTF draw + immovable collision box (must upload mesh once with matching `assetCacheKey`).
struct StaticMeshActorSpawnDesc {
    Vec3 position{0.0f, 0.0f, 0.0f};
    /// Full AABB size (not half-extents).
    Vec3 boxSize{2.0f, 1.0f, 2.0f};
    std::string assetCacheKey;
    float uniformScale       = 1.0f;
    Quat modelSpaceRotation  = Quat::Identity();
    uint32_t layer           = 1u << 2;
    uint32_t collidesWithMask = (1u << 0);
    bool gpuRegistered       = true;
};

struct StaticMeshActorFactory {
    static Entity spawn(Registry& registry, const StaticMeshActorSpawnDesc& desc)
    {
        Entity e = registry.createEntity();
        registry.addComponent(e, TransformComponent{});
        registry.addComponent(e, WorldTransformComponent{});

        auto& t = registry.getComponent<TransformComponent>(e);
        t.position = desc.position;

        CollisionBoxComponent box{};
        box.setBoxSize(desc.boxSize.x, desc.boxSize.y, desc.boxSize.z);
        box.lastPosition     = desc.position;
        box.layer            = desc.layer;
        box.collidesWithMask = desc.collidesWithMask;
        box.isStatic         = true;
        box.blocksMovement   = true;
        registry.addComponent(e, box);

        StaticMeshComponent sm{};
        sm.assetCacheKey      = desc.assetCacheKey;
        sm.modelSpaceRotation = desc.modelSpaceRotation;
        sm.uniformScale       = desc.uniformScale;
        sm.gpuRegistered      = desc.gpuRegistered;
        registry.addComponent(e, std::move(sm));

        return e;
    }
};

} // namespace game::factories
