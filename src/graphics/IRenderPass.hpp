#pragma once

#include "RenderContext.hpp"

#include <memory>

// -----------------------------------------------------------------------------
// Single responsibility: one visual layer (terrain, meshes, debug, UI…).
// Open/closed: add new pass types without modifying existing ones.
// Register on IGraphicsRenderer::registerRenderPass (see OpenGLVer2Renderer).
// -----------------------------------------------------------------------------

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    /// Lower runs first (background → foreground).
    virtual int sortKey() const { return 0; }

    virtual void render(RenderContext& ctx) = 0;
};
