#pragma once

#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"

#include <cstdint>

struct GLFWwindow;

// =============================================================================
// OpenGLRenderSystem — minimal forward-lit debug draw. Queries the Registry each
// frame (active camera, PlayerTag, EnemyTag, BoneInstance) so callers pass only
// `registry`; add tags / components as your game grows without changing the API.
// =============================================================================
class OpenGLRenderSystem {
public:
    bool init(int width, int height, const char* title);
    void shutdown();

    void pollFramebufferSize(int& outW, int& outH) const;
    GLFWwindow* window() const { return m_window; }

    /// Reads `registry` only: first active `CameraComponent`, tagged player/enemies, bone instances.
    void renderFrame(Registry& registry);

    bool shouldClose() const;

private:
    void buildPyramidMesh();
    void drawPyramid(const Mat4& mvp, const Mat4& model, const float color[3]);

    GLFWwindow* m_window = nullptr;
    int m_fbW = 1280;
    int m_fbH = 720;

    unsigned int m_program = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_vertexCount = 0;
};
