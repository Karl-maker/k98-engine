#pragma once

#include "GraphicsTypes.hpp"
#include "IRenderPass.hpp"
#include "RenderContext.hpp"

#include <memory>
#include <string>

struct GLFWwindow;
class ModelAsset;
class Registry;
class AssetManager;

// -----------------------------------------------------------------------------
// Graphics backend abstraction — swap OpenGL for Vulkan/Metal/Direct3D without
// changing game code. Implementations own window context, GPU caches, passes.
// -----------------------------------------------------------------------------

class IGraphicsRenderer {
public:
    virtual ~IGraphicsRenderer() = default;

    virtual bool init(int width, int height, const char* title, const GraphicsInitOptions& options = {}) = 0;
    virtual void shutdown() = 0;

    virtual void pollFramebufferSize(int& outW, int& outH) const = 0;
    virtual GLFWwindow* window() const = 0;

    /// Upload mesh/material data already loaded as ModelAsset (textures + VAO/VBO/EBO).
    virtual bool uploadStaticModel(const ModelAsset& model, const std::string& assetCacheKey) = 0;

    /// Load glTF via AssetManager, upload to GPU, optionally drop CPU vertex/index arrays (skeleton + clips stay in ModelAsset).
    virtual bool uploadStaticModelFromPath(
        AssetManager& assets,
        const std::string& path,
        const std::string& assetCacheKey,
        bool releaseCpuMeshAfterUpload = true) = 0;

    virtual void registerRenderPass(std::unique_ptr<IRenderPass> pass) = 0;
    virtual void clearRenderPasses() = 0;
    virtual void installDefaultRenderPasses() = 0;

    virtual void executeTerrainPass(RenderContext& ctx) = 0;
    virtual void executeStaticSkinnedMeshesPass(RenderContext& ctx) = 0;
    virtual void executeDebugPlayerFallbackPass(RenderContext& ctx) = 0;

    virtual void renderFrame(Registry& registry) = 0;

    virtual void setDebugHudSnapshot(OpenGLDebugHudSnapshot snapshot) = 0;

    virtual void uploadPbrMaterialPresets(Registry& registry) = 0;

    virtual bool shouldClose() const = 0;
};
