#pragma once

#include "../math/Vec3.hpp"
#include <cstdint>

// -----------------------------------------------------------------------------
// Simple procedural solid for SolidMeshRenderFeature (no glTF). Local cube is
// axis-aligned ±1 in model space, then scaled by halfExtent and translated by
// localOffset before applying WorldTransformComponent.world.
//
// Register: registry.registerComponent<SolidMeshComponent>();
// -----------------------------------------------------------------------------

struct SolidMeshComponent {
    enum class Shape : std::uint8_t { Cube = 0 };

    Shape shape = Shape::Cube;

    /// Half-size of the cube in model space (final AABB is [-h,h] per axis before world matrix).
    float halfExtent = 0.35f;

    /// Extra translation in **local** space after scale (e.g. lower body under camera).
    Vec3 localOffset{0.0f, -0.85f, 0.0f};

    Vec3 color{0.25f, 0.55f, 0.95f};
};
