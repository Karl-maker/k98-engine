#pragma once

#include "../math/Vec3.hpp"
#include "../ecs/Entity.hpp"

/// World-space reference for proximity streaming (e.g. follows transform each frame).
struct StreamingAnchorComponent {
    Vec3 worldPosition{};
    float loadRadius = 80.f;
    float unloadRadius = 120.f;
    Entity viewerEntity = INVALID_ENTITY;
};
