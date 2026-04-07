#pragma once

#include "RenderContext.hpp"

#include <memory>

// -----------------------------------------------------------------------------
// Single Responsibility: one pass type (terrain, static meshes, debug, UI…).
// Register instances on OpenGLRenderSystem; renderFrame invokes them in sortKey order.
// -----------------------------------------------------------------------------

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    /// Lower runs first (background → foreground).
    virtual int sortKey() const { return 0; }

    virtual void render(RenderContext& ctx) = 0;
};
