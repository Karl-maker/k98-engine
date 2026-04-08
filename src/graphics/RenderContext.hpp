#pragma once

#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"

class IGraphicsRenderer;

// -----------------------------------------------------------------------------
// Per-frame snapshot for IRenderPass (registry + camera matrices + renderer abstraction).
// Dependency inversion: passes depend on IGraphicsRenderer*, not concrete OpenGL types.
// -----------------------------------------------------------------------------

struct RenderContext {
    Registry* registry = nullptr;
    IGraphicsRenderer* renderer = nullptr;
    Mat4 pvShifted{};
    Mat4 tmO{};
    Vec3 cameraWorld{};
};
