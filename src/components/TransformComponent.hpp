#pragma once
#include "../ecs/Entity.hpp"
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"

// -----------------------------------------------------------------------------
// TransformComponent — local transform for an entity. Optional `parent` links
// to a parent entity for hierarchy (combined with AttachmentSystem / sockets).
// WorldTransformComponent caches derived world matrices (see TransformSystem).
//
// Register: registry.registerComponent<TransformComponent>();
// -----------------------------------------------------------------------------

struct TransformComponent {
    Vec3 position;
    Quat rotation;
    Vec3 scale{1,1,1};

    Entity parent = static_cast<Entity>(-1); // no parent
};