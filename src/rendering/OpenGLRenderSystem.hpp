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
    void drawTexturedModel(const Mat4& vp, const Mat4& model, const std::string& assetCacheKey);
    void drawTexturedSkinnedModel(
        const Mat4& vp,
        const Mat4& model,
        const std::string& assetCacheKey,
        const std::vector<Mat4>& jointSkinMatrices);

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
};
