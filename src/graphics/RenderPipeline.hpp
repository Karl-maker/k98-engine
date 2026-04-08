#pragma once

// =============================================================================
// Graphics / render pipeline (SOLID-oriented)
// -----------------------------------------------------------------------------
// Single responsibility
//   - IRenderPass: one layer (terrain, static/skinned meshes, debug fallback…).
//   - IGraphicsRenderer: window, GPU resources, ordered pass list (swap for Vulkan/Metal/etc.).
//   - Components (PbrMaterialPresetComponent, StaticMeshComponent, …): data only.
//
// Open / closed
//   - Add IRenderPass implementations without editing GameLoop; register via
//     IGraphicsRenderer::registerRenderPass().
//
// Liskov
//   - Passes consume RenderContext (registry + matrices + IGraphicsRenderer*).
//
// Interface segregation
//   - Passes call narrow entry points (executeTerrainPass, executeStaticSkinnedMeshesPass, …)
//     implemented on the concrete backend.
//
// Dependency inversion
//   - Game code and passes depend on IGraphicsRenderer + IRenderPass, not OpenGL.
//
// Extension points
//   - clearRenderPasses() + registerRenderPass() for custom pipelines.
//   - PbrMaterialPresetComponent (+ PbrTextureSetComponent) for tiled PBR surfaces.
//   - HdriEnvironmentComponent for IBL (consumed by OpenGLVer2Renderer).
// =============================================================================
