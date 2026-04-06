#pragma once

#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"
#include "../math/Vec3.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;
class ModelAsset;

// =============================================================================
// OpenGLRenderSystem — debug primitives + static glTF draws driven by ECS.
// Textured meshes: entities with StaticMeshComponent + Transform (see renderFrame).
// Also: active camera, EnemyTag pyramids, BoneInstance pyramids, PlayerTag pyramid fallback.
// =============================================================================
class OpenGLRenderSystem {
public:
    bool init(int width, int height, const char* title);
    void shutdown();

    void pollFramebufferSize(int& outW, int& outH) const;
    GLFWwindow* window() const { return m_window; }

    /// Upload meshes/materials; `assetCacheKey` must match StaticMeshComponent::assetCacheKey when drawing.
    bool uploadStaticModel(const ModelAsset& model, const std::string& assetCacheKey);

    /// Reads `registry` only: first active `CameraComponent`, tagged player/enemies, bone instances.
    void renderFrame(Registry& registry);

    bool shouldClose() const;

private:
    void buildPyramidMesh();
    void buildTexturedShaderPipeline();
    void buildSkinnedTexturedShaderPipeline();
    void releaseStaticModel();
    void releaseGpuMeshesForKey(const std::string& assetCacheKey);
    void drawPyramid(const Mat4& mvp, const Mat4& model, const float color[3]);
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

    /// Fills P*V*T(O) and T(-O) with O = snappedOrigin. Reuses cached matrices when
    /// framebuffer, proj, view, and O match the last frame (avoids redundant mat4 work).
    void getFloatingOriginMatrices(
        const Mat4& proj,
        const Mat4& view,
        const Vec3& snappedOrigin,
        Mat4& outPvShifted,
        Mat4& outTmO);

    struct StaticMeshPart {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int          indexCount = 0;
        unsigned int albedo = 0;
        unsigned int normalMap = 0;
        unsigned int occlusionMap = 0;
        unsigned int metallicRoughnessMap = 0;
        bool         hasNormalMap = false;
        bool         hasOcclusion = false;
        bool         hasMetallicRoughness = false;
        bool         skinned = false;
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

    std::unordered_map<std::string, std::vector<StaticMeshPart>> m_gpuMeshByAssetKey;

    unsigned int m_hdriTexture = 0;
    std::string m_hdriLoadedPath;
    bool m_hdriWarnedMultiple = false;

    // Floating origin (see tryGetCachedFloatingOriginMatrices).
    Mat4 m_floatOriginPvShifted{};
    Mat4 m_floatOriginTmO{};
    Vec3 m_floatOriginCachedO{};
    float m_floatOriginCacheView[16]{};
    float m_floatOriginCacheProj[16]{};
    int m_floatOriginCacheFbW = 0;
    int m_floatOriginCacheFbH = 0;
    bool m_floatOriginCacheValid = false;
};
