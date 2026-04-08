#pragma once

#include "../core/ThreadService.hpp"
#include "../core/IGame.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

struct SuperHeroSettings {
    bool openglDebugHud = false;
    int glSwapInterval = 1;
    int targetFpsPreset = 60;
};

class SuperHero final : public IGame {
public:
    explicit SuperHero(const SuperHeroSettings& settings = {})
        : m_settings(settings)
        , m_debugHudEnabled(settings.openglDebugHud)
    {
        (void)m_debugHudEnabled;
    }

    void onStart() override
    {
        m_threadService.configure({});
        m_threadService.start();

        m_registry.registerComponent<TransformComponent>();
        m_registry.registerComponent<MovementIntentComponent>();
        m_registry.registerComponent<KinematicComponent>();

        Entity e = m_registry.createEntity();
        TransformComponent tc{};
        m_registry.addComponent(e, tc);
    

        if (!glfwInit()) {
            std::cerr << "glfwInit failed\n";
            m_shouldClose = true;
            return;
        }
        m_glfwInitialized = true;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        m_window = glfwCreateWindow(960, 540, "SuperHero", nullptr, nullptr);
        if (!m_window) {
            std::cerr << "glfwCreateWindow failed\n";
            m_shouldClose = true;
            return;
        }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(m_settings.glSwapInterval);

        (void)m_assetManager;
    }

    void onInput() override
    {
    }

    void onUpdate(double dt) override
    {
        if (m_shouldClose)
            return;

        const float fdt = static_cast<float>(dt);
    }

    void onRender(double) override
    {
        if (m_window)
            glfwSwapBuffers(m_window);
    }

    void onStop() override
    {
        m_threadService.shutdown();
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        if (m_glfwInitialized) {
            glfwTerminate();
            m_glfwInitialized = false;
        }
    }

    bool shouldClose() const override { return m_shouldClose; }

private:
    SuperHeroSettings m_settings;
    bool m_debugHudEnabled = false;
    bool m_shouldClose = false;
    bool m_glfwInitialized = false;

    GLFWwindow* m_window = nullptr;

    Registry m_registry;
    AssetManager m_assetManager;
    ThreadService m_threadService;

};
