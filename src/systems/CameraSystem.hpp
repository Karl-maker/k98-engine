#pragma once

#include "../ecs/Registry.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../math/Mat4.hpp"

class CameraSystem {
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<CameraComponent, TransformComponent>();

        for (auto e : entities)
        {
            auto& cam = registry.getComponent<CameraComponent>(e);
            if (!cam.active) continue;

            auto& transform = registry.getComponent<TransformComponent>(e);

            cam.viewMatrix = Mat4::FromTR(transform.position, transform.rotation);
        }
    }
};