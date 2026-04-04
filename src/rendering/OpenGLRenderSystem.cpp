#include "OpenGLRenderSystem.hpp"

#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Vec3.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

static const char* kVertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uMVP;
out vec3 vNormal;
void main() {
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFragSrc = R"(
#version 330 core
in vec3 vNormal;
uniform vec3 uColor;
uniform vec3 uLightDir;
uniform vec3 uAmbient;
out vec4 FragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float ndl = max(dot(N, L), 0.0);
    vec3 c = uColor * (uAmbient + vec3(0.55) * ndl);
    FragColor = vec4(c, 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool OpenGLRenderSystem::init(int width, int height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

#if !defined(__APPLE__)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "glewInit failed\n";
        shutdown();
        return false;
    }
#endif

    glfwGetFramebufferSize(m_window, &m_fbW, &m_fbH);
    glViewport(0, 0, m_fbW, m_fbH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
    if (!vs || !fs)
        return false;

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    GLint linked = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        std::cerr << "Program link: " << log << "\n";
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    buildPyramidMesh();
    return true;
}

void OpenGLRenderSystem::buildPyramidMesh() {
    struct V {
        float px, py, pz, nx, ny, nz;
    };
    std::vector<V> v;
    // Unit pyramid: apex +Y, square base in plane y = -0.5 (centered at origin).
    const Vec3 apex{0.0f, 0.5f, 0.0f};
    const Vec3 b0{-0.5f, -0.5f, -0.5f};
    const Vec3 b1{0.5f, -0.5f, -0.5f};
    const Vec3 b2{0.5f, -0.5f, 0.5f};
    const Vec3 b3{-0.5f, -0.5f, 0.5f};

    auto addTri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
        Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        Vec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 n = cross(e1, e2);
        n = normalize(n);
        v.push_back({a.x, a.y, a.z, n.x, n.y, n.z});
        v.push_back({b.x, b.y, b.z, n.x, n.y, n.z});
        v.push_back({c.x, c.y, c.z, n.x, n.y, n.z});
    };

    // Base (outward normal -Y, viewed from below)
    addTri(b0, b1, b2);
    addTri(b0, b2, b3);
    // Four sides (CCW when viewed from outside)
    addTri(apex, b1, b0);
    addTri(apex, b2, b1);
    addTri(apex, b3, b2);
    addTri(apex, b0, b3);

    m_vertexCount = static_cast<unsigned int>(v.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(V), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void OpenGLRenderSystem::shutdown() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

void OpenGLRenderSystem::pollFramebufferSize(int& outW, int& outH) const {
    if (m_window)
        glfwGetFramebufferSize(m_window, &outW, &outH);
    else {
        outW = m_fbW;
        outH = m_fbH;
    }
}

bool OpenGLRenderSystem::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void OpenGLRenderSystem::drawPyramid(const Mat4& mvp, const Mat4& model, const float color[3]) {
    GLint uM = glGetUniformLocation(m_program, "uModel");
    GLint uMvp = glGetUniformLocation(m_program, "uMVP");
    GLint uC = glGetUniformLocation(m_program, "uColor");
    GLint uL = glGetUniformLocation(m_program, "uLightDir");
    GLint uA = glGetUniformLocation(m_program, "uAmbient");
    glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);
    glUniform3fv(uC, 1, color);
    float lightDir[3] = {0.35f, 0.85f, 0.35f};
    float len = std::sqrt(lightDir[0] * lightDir[0] + lightDir[1] * lightDir[1] + lightDir[2] * lightDir[2]);
    if (len > 1e-5f) {
        lightDir[0] /= len;
        lightDir[1] /= len;
        lightDir[2] /= len;
    }
    glUniform3fv(uL, 1, lightDir);
    float amb[3] = {0.22f, 0.22f, 0.25f};
    glUniform3fv(uA, 1, amb);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertexCount));
    glBindVertexArray(0);
}

void OpenGLRenderSystem::renderFrame(
    Registry& registry,
    Entity cameraEntity,
    Entity playerEntity,
    const std::vector<Entity>& enemies,
    Entity boneEntity) {
    if (!m_window || !m_program)
        return;

    glfwGetFramebufferSize(m_window, &m_fbW, &m_fbH);
    if (m_fbW <= 0 || m_fbH <= 0)
        return;

    glViewport(0, 0, m_fbW, m_fbH);
    glClearColor(0.12f, 0.12f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!registry.hasComponent<CameraComponent>(cameraEntity) ||
        !registry.hasComponent<TransformComponent>(cameraEntity))
        return;

    auto& cam = registry.getComponent<CameraComponent>(cameraEntity);
    if (!cam.active)
        return;

    auto worldTranslation = [&](Entity e) -> Vec3 {
        if (registry.hasComponent<WorldTransformComponent>(e)) {
            const Mat4& w = registry.getComponent<WorldTransformComponent>(e).world;
            return {w.m[12], w.m[13], w.m[14]};
        }
        if (registry.hasComponent<TransformComponent>(e))
            return registry.getComponent<TransformComponent>(e).position;
        return {0.0f, 0.0f, 0.0f};
    };

    const Vec3 eye = worldTranslation(cameraEntity);

    Entity lookTarget = INVALID_ENTITY;
    if (cam.enableLockOn && cam.lockOnTarget != INVALID_ENTITY)
        lookTarget = cam.lockOnTarget;
    else if (cam.enableLookAt && cam.lookAtTarget != INVALID_ENTITY)
        lookTarget = cam.lookAtTarget;

    Mat4 view;
    if (lookTarget != INVALID_ENTITY && registry.hasComponent<TransformComponent>(lookTarget)) {
        const auto& t = registry.getComponent<TransformComponent>(lookTarget);
        Vec3 center{
            t.position.x + cam.lookAtOffset.x,
            t.position.y + cam.lookAtOffset.y,
            t.position.z + cam.lookAtOffset.z};
        view = Mat4::LookAt(eye, center, {0.0f, 1.0f, 0.0f});
    } else {
        // Fallback: match `CameraSystem` (stores camera *world* matrix in `viewMatrix`).
        view = Mat4::inverse(cam.viewMatrix);
    }

    float aspect = static_cast<float>(m_fbW) / static_cast<float>(m_fbH);
    Mat4 proj = Mat4::Perspective(cam.fov, aspect, cam.nearPlane, cam.farPlane);
    Mat4 vp = mat4Mul(proj, view);

    glUseProgram(m_program);

    auto drawAt = [&](const Vec3& pos, float scale, const float col[3]) {
        Mat4 model = mat4Mul(Mat4::FromTranslation(pos), Mat4::FromScale({scale, scale, scale}));
        Mat4 mvp = mat4Mul(vp, model);
        drawPyramid(mvp, model, col);
    };

    const float colPlayer[3] = {0.2f, 0.75f, 0.35f};
    const float colEnemy[3] = {0.85f, 0.35f, 0.2f};
    const float colBone[3] = {0.9f, 0.85f, 0.2f};

    drawAt(worldTranslation(playerEntity), 1.15f, colPlayer);

    for (Entity e : enemies)
        drawAt(worldTranslation(e), 1.05f, colEnemy);

    // Full bone pose matrix so rotation from AnimationSystem is visible (translation-only hid Y-axis swing).
    if (boneEntity != INVALID_ENTITY && registry.hasComponent<WorldTransformComponent>(boneEntity)) {
        const Mat4& w = registry.getComponent<WorldTransformComponent>(boneEntity).world;
        const float s = 0.45f;
        Mat4 model = mat4Mul(w, Mat4::FromScale({s, s, s}));
        Mat4 mvp = mat4Mul(vp, model);
        drawPyramid(mvp, model, colBone);
    }

    glfwSwapBuffers(m_window);
}
