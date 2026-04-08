#pragma once

// OpenGL 3.3 implementation of IGraphicsRenderer — GPU mesh cache (VAO/VBO/EBO),
// PBR + skinning + terrain + HDRI. Pass list: IRenderPass (Open/Closed).

#include "../IGraphicsRenderer.hpp"

#include "../../ecs/Entity.hpp"
#include "../../ecs/Registry.hpp"
#include "../../math/Mat4.hpp"
#include "../../math/Vec3.hpp"
#include "../IRenderPass.hpp"
#include "../RenderContext.hpp"

#include "../../components/PbrTextureSetComponent.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;
class ModelAsset;
class AssetManager;

class OpenGLVer2Renderer final : public IGraphicsRenderer {
public:
    bool init(int width, int height, const char* title, const GraphicsInitOptions& options = {}) override;
    void shutdown() override;

    void pollFramebufferSize(int& outW, int& outH) const override;
    GLFWwindow* window() const override { return m_window; }

    bool uploadStaticModel(const ModelAsset& model, const std::string& assetCacheKey) override;

    bool uploadStaticModelFromPath(
        AssetManager& assets,
        const std::string& path,
        const std::string& assetCacheKey,
        bool releaseCpuMeshAfterUpload = true) override;

    void registerRenderPass(std::unique_ptr<IRenderPass> pass) override;
    void clearRenderPasses() override;
    void installDefaultRenderPasses() override;

    void executeTerrainPass(RenderContext& ctx) override;
    void executeStaticSkinnedMeshesPass(RenderContext& ctx) override;
    void executeDebugPlayerFallbackPass(RenderContext& ctx) override;

    void renderFrame(Registry& registry) override;

    void setDebugHudSnapshot(OpenGLDebugHudSnapshot snapshot) override;

    void uploadPbrMaterialPresets(Registry& registry) override;

    bool shouldClose() const override;

private:
    void buildPyramidMesh();
    void buildBoxMesh();
    void buildDebugLineMesh();
    void drawDebugRaycasts(RenderContext& ctx);
    void buildTexturedShaderPipeline();
    void buildSkinnedTexturedShaderPipeline();
    void releaseStaticModel();
    void releaseGpuMeshesForKey(const std::string& assetCacheKey);
    void drawPyramid(const Mat4& mvp, const Mat4& model, const float color[3]);
    void drawBox(const Mat4& mvp, const Mat4& model, const float color[3]);
    void drawTexturedModel(
        Registry& registry,
        const Mat4& pvShifted,
        const Mat4& tmO,
        const Mat4& model,
        const std::string& assetCacheKey,
        const Vec3& cameraWorld);
    void drawTexturedSkinnedModel(
        Registry& registry,
        const Mat4& pvShifted,
        const Mat4& tmO,
        const Mat4& model,
        const std::string& assetCacheKey,
        const std::vector<Mat4>& jointSkinMatrices,
        const Vec3& cameraWorld);

    void applyTexturedSceneLighting(unsigned int program, Registry& registry, const Vec3& cameraWorld);
    void applyHdriUniforms(unsigned int program, Registry& registry);

    void bindPbrTextureMaps(unsigned int program, const PbrTextureSetComponent& maps, bool useDisplacementMap);

    struct TerrainChunkGpuMesh {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int indexCount = 0;
        int floatsPerVertex = 6;
    };

    void syncTerrainMeshes(Registry& registry);
    void drawTerrainMeshes(Registry& registry, const Mat4& pvShifted, const Mat4& tmO, const Vec3& cameraWorld);
    void releaseTerrainMeshes();

    void getFloatingOriginMatrices(
        const Mat4& proj,
        const Mat4& view,
        const Vec3& snappedOrigin,
        Mat4& outPvShifted,
        Mat4& outTmO);

    void buildDebugHudPipeline();
    void drawDebugHudOverlay();

    struct StaticMeshPart {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int indexCount = 0;
        unsigned int albedo = 0;
        unsigned int normalMap = 0;
        unsigned int occlusionMap = 0;
        unsigned int metallicRoughnessMap = 0;
        bool hasNormalMap = false;
        bool hasOcclusion = false;
        bool hasMetallicRoughness = false;
        bool skinned = false;
    };

    GLFWwindow* m_window = nullptr;
    int m_fbW = 1280;
    int m_fbH = 720;

    unsigned int m_program = 0;
    unsigned int m_texProgram = 0;
    unsigned int m_skinTexProgram = 0;
    unsigned int m_skinPaletteUbo = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_vertexCount = 0;
    unsigned int m_boxVao = 0;
    unsigned int m_boxVbo = 0;
    unsigned int m_boxVertexCount = 0;
    unsigned int m_lineVao = 0;
    unsigned int m_lineVbo = 0;

    std::unordered_map<std::string, std::vector<StaticMeshPart>> m_gpuMeshByAssetKey;

    unsigned int m_hdriTexture = 0;
    std::string m_hdriLoadedPath;
    bool m_hdriWarnedMultiple = false;

    Mat4 m_floatOriginPvShifted{};
    Mat4 m_floatOriginTmO{};
    Vec3 m_floatOriginCachedO{};
    float m_floatOriginCacheView[16]{};
    float m_floatOriginCacheProj[16]{};
    int m_floatOriginCacheFbW = 0;
    int m_floatOriginCacheFbH = 0;
    bool m_floatOriginCacheValid = false;

    std::unordered_map<Entity, TerrainChunkGpuMesh> m_terrainMeshes;

    unsigned int m_texWhite1x1 = 0;

    std::vector<std::unique_ptr<IRenderPass>> m_renderPasses;

    OpenGLDebugHudSnapshot m_debugHud{};
    unsigned int m_debugHudProgram = 0;
    unsigned int m_debugHudVao = 0;
    unsigned int m_debugHudVbo = 0;
    int m_debugHudLocFbSize = -1;
};
