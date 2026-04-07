#pragma once

#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"

class OpenGLRenderSystem;

// -----------------------------------------------------------------------------
// Frame snapshot passed to each IRenderPass (registry + camera matrices).
// -----------------------------------------------------------------------------

struct RenderContext {
    Registry* registry = nullptr;
    OpenGLRenderSystem* renderer = nullptr;
    Mat4 pvShifted{};
    Mat4 tmO{};
    Vec3 cameraWorld{};
};
