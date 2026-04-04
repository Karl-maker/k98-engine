#pragma once

#include "../ecs/Registry.hpp"
#include "../ecs/Entity.hpp"
#include "../math/Mat4.hpp"

#include <cstdint>
#include <vector>

struct GLFWwindow;

/// Forward+ambient lit pyramids at world positions; view matches camera eye → look target (see `CameraSystem`).
class OpenGLRenderSystem {
public:
    bool init(int width, int height, const char* title);
    void shutdown();

    void pollFramebufferSize(int& outW, int& outH) const;
    GLFWwindow* window() const { return m_window; }

    void renderFrame(
        Registry& registry,
        Entity cameraEntity,
        Entity playerEntity,
        const std::vector<Entity>& enemies,
        Entity boneEntity);

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
