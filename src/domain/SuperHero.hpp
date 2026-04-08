#pragma once

#include "../core/IGame.hpp"
#include "../ecs/Entity.hpp"

class Registry;
class AssetManager;
class ThreadService;
class OpenGLVer2Renderer;

class SuperHero final : public IGame {
public:
    struct Settings {
        bool openglDebugHud = false;
        int glSwapInterval = 1;
        int targetFpsPreset = 60;
    };

    explicit SuperHero(const Settings& settings);
    ~SuperHero() override;

    void onStart() override;
    void onInput() override;
    void onUpdate(double dt) override;
    void onRender(double alpha) override;
    void onStop() override;

    bool shouldClose() const override;

private:
    void registerComponents();
    void updateCamera();

    Settings m_settings;
    bool m_debugHudEnabled = false;
    bool m_shouldClose = false;

    Registry* m_registry = nullptr;
    AssetManager* m_assetManager = nullptr;
    ThreadService* m_threadService = nullptr;
    OpenGLVer2Renderer* m_renderer = nullptr;

    Entity m_character = INVALID_ENTITY;
    Entity m_hat = INVALID_ENTITY;
    Entity m_camera = INVALID_ENTITY;
};
