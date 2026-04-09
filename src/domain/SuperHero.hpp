#pragma once

#include "../core/IGame.hpp"
#include "../ecs/Entity.hpp"
#include "../systems/PhysicsSystem.hpp"
#include "../systems/RaycastSystem.hpp"
#include "../systems/TerrainChunkSystem.hpp"
#include "../utils/TerrainHeightField.hpp"
#include "terrain/TerrainWorldMap.hpp"
#include "systems/AiControllerSystem.hpp"
#include "systems/CollisionSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/PlayerControllerSystem.hpp"

#include <chrono>
#include <string>

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
    void updateThirdPersonCamera(float dt);
    void updateRightArmAim();
    void updateAnimClipCrossFade(float dt);

    Settings m_settings;
    bool m_debugHudEnabled = false;
    std::string m_debugDetailText;
    float m_fpsSmooth = 0.f;
    bool m_haveFrameTime = false;
    std::chrono::steady_clock::time_point m_lastFrameTime{};
    /// Seconds; drives right-hand rest → point → rest cycle.
    float m_handAnimTime = 0.f;
    /// Loops idle (clip 0) ↔ secondary clip (clip 1) with cross-fade.
    float m_animClipTimer = 0.f;
    int m_animClipSegment = 0;
    bool m_shouldClose = false;
    /// Edge-detect GLFW_KEY_L for debug HUD toggle.
    bool m_debugHudKeyLHeld = false;
    /// Mouse deltas for orbit (cursor mode uses absolute position).
    double m_lastCamMouseX = 0.0;
    double m_lastCamMouseY = 0.0;
    bool m_cameraMouseInitialized = false;

    Registry* m_registry = nullptr;
    AssetManager* m_assetManager = nullptr;
    ThreadService* m_threadService = nullptr;
    OpenGLVer2Renderer* m_renderer = nullptr;

    Entity m_character = INVALID_ENTITY;
    Entity m_hat = INVALID_ENTITY;
    Entity m_camera = INVALID_ENTITY;
    /// Small dynamic box spawned above the player; rests on the capsule via physics.
    Entity m_headBox = INVALID_ENTITY;
    Entity m_headBox2 = INVALID_ENTITY;
    /// Bone-attached line-of-sight ray (e.g. head forward).
    Entity m_headRay = INVALID_ENTITY;
    Entity m_aiChaser = INVALID_ENTITY;

    PlayerControllerSystem m_playerController{};
    AIControllerSystem m_aiController{};
    MovementSystem m_movementSystem{};
    CollisionSystem m_collisionSystem{};
    PhysicsSystem m_physicsSys{};
    RaycastSystem m_raycastSys{};

    TerrainChunkSystem m_terrainChunks{};
    TerrainHeightField m_terrainHeights{};
    TerrainWorldMap m_terrainMap{};
};
