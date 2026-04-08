#pragma once

#include "MovementTutorialSettings.hpp"

#include "../core/IGame.hpp"
#include "../ecs/Registry.hpp"
#include "../game/StateEventType.hpp"
#include "../statemachine/StateMachine.hpp"
#include "../statemachine/StateMachineTypes.hpp"

#include "../components/CameraComponent.hpp"
#include "../components/CollisionBoxComponent.hpp"
#include "../components/GroundingStateComponent.hpp"
#include "../components/HdriEnvironmentComponent.hpp"
#include "../components/LightingComponent.hpp"
#include "../components/LocomotionRuntimeComponent.hpp"
#include "../components/LocomotorControlComponent.hpp"
#include "../components/MassComponent.hpp"
#include "../components/MovementComponent.hpp"
#include "../components/PlayerMovementIntentComponent.hpp"
#include "../components/Position.hpp"
#include "../components/ShaderPipelineComponent.hpp"
#include "../components/StaticMeshComponent.hpp"
#include "../components/GpuSkinPaletteComponent.hpp"
#include "../components/StateMachineComponent.hpp"
#include "../components/TerrainSettingsComponent.hpp"
#include "../components/PbrMaterialPresetComponent.hpp"
#include "../components/TerrainChunkComponent.hpp"
#include "../components/HeightMapComponent.hpp"
#include "../components/Texture2DGlComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/Velocity.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../components/GameplayTags.hpp"
#include "../components/AttachComponent.hpp"
#include "../components/SocketComponent.hpp"

#include "../systems/CameraGroundClampSystem.hpp"
#include "../systems/CameraSystem.hpp"
#include "../systems/CollisionSystem.hpp"
#include "../systems/LocomotorIntentSystem.hpp"
#include "../systems/PlayerLocomotionSystem.hpp"
#include "../systems/PositionToTransformSystem.hpp"
#include "../systems/SolidCollisionResponseSystem.hpp"
#include "../systems/TerrainEnvironmentSystem.hpp"
#include "../systems/TransformSystem.hpp"
#include "../systems/AttachmentSystem.hpp"
#include "../systems/AnimationSystem.hpp"

#include "../core/assets/AssetManager.hpp"
#include "../core/assets/importers/GltfModelImporter.hpp"
#include "../core/assets/ModelAsset.hpp"

#include "factories/SkinnedCharacterActorFactory.hpp"
#include "factories/StaticMeshActorFactory.hpp"

#include "../components/SkeletonInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/AnimationPlaybackComponent.hpp"

#include "../graphics/GraphicsTypes.hpp"
#include "../graphics/IGraphicsRenderer.hpp"
#include "../graphics/opengl/OpenGLVer2Renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "../math/Quat.hpp"
#include "../math/Vec3.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

// =============================================================================
// Movement tutorial: flat terrain, locomotion FSM (Idle / Moving / Sprinting /
// Jumping), LocomotorControl → Movement → PlayerLocomotionSystem, mass+gravity.
// No enemies; render pipeline uses OpenGL default IRenderPass registration.
// =============================================================================

class MovementTutorial final : public IGame {
public:
    explicit MovementTutorial(const MovementTutorialSettings& settings = {})
        : m_settings(settings)
        , m_debugHudEnabled(settings.openglDebugHud)
    {
    }

    void onStart() override
    {
        std::cout << "Movement Tutorial — WASD move, mouse look, Shift sprint, Space jump\n";

        registerComponents();
        m_assetManager.registerImporter("gltf", std::make_shared<GltfModelImporter>());
        m_assetManager.registerImporter("glb", std::make_shared<GltfModelImporter>());

        createTerrainSettingsFlat();
        createPlayerFromFactory();
        createCameraRig();
        createHdriOnlyEnvironment();

        m_spatialGrid.cellSize = 4.0f;

        m_positionToTransformSystem.update(m_registry);

        m_renderer = std::make_unique<OpenGLVer2Renderer>();
        GraphicsInitOptions glOpts;
        glOpts.swapInterval = m_settings.glSwapInterval;
        if (!m_renderer->init(1280, 720, "Movement Tutorial", glOpts)) {
            std::cerr << "OpenGL init failed (headless path not implemented).\n";
            m_shouldClose = true;
            return;
        }

        m_renderer->uploadPbrMaterialPresets(m_registry);

        if (m_renderer->window()) {
            glfwSetInputMode(m_renderer->window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            int ww = 0, wh = 0;
            glfwGetWindowSize(m_renderer->window(), &ww, &wh);
            if (ww > 0 && wh > 0)
                glfwSetCursorPos(m_renderer->window(), static_cast<double>(ww) * 0.5, static_cast<double>(wh) * 0.5);
        }

        if (m_playerAvatarModel) {
            // Slight -Y aligns glTF root with capsule feet (physics uses footOffset = halfHeight).
            game::factories::SkinnedCharacterActorFactory::uploadToGpuAndAttach(
                *m_renderer,
                m_registry,
                m_player,
                m_playerAvatarModel,
                m_playerAvatarPath,
                1.0f,
                Quat::Identity(),
                Vec3{0.0f, -0.07f, 0.0f});

            game::factories::StaticMeshActorSpawnDesc prop{};
            prop.position        = {8.0f, 0.35f, 4.0f};
            prop.boxSize         = {1.4f, 0.7f, 1.4f};
            prop.assetCacheKey   = m_playerAvatarPath;
            prop.uniformScale    = 0.22f;
            prop.layer           = kLayerWorldStatic;
            prop.collidesWithMask = (1u << 0);
            m_staticProp         = game::factories::StaticMeshActorFactory::spawn(m_registry, prop);
        }

        m_terrainEnv.snapGroundedActorsToSurface(m_registry, m_spatialGrid);
        m_transformSystem.update(m_registry);
    }

    void onInput() override
    {
        if (!m_renderer || !m_renderer->window())
            return;

        glfwPollEvents();

        GLFWwindow* w = m_renderer->window();
        auto& ctrl      = m_registry.getComponent<LocomotorControlComponent>(m_player);

        ctrl.forward = 0.0f;
        ctrl.right   = 0.0f;
        if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)
            ctrl.forward += 1.0f;
        if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)
            ctrl.forward -= 1.0f;
        if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)
            ctrl.right -= 1.0f;
        if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)
            ctrl.right += 1.0f;

        ctrl.sprintHeld = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

        const bool spaceDown = glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS;
        ctrl.jumpRequested   = spaceDown && !m_prevSpaceDown;
        m_prevSpaceDown      = spaceDown;

        const bool lDown = glfwGetKey(w, GLFW_KEY_L) == GLFW_PRESS;
        if (lDown && !m_prevDebugToggleDown)
            m_debugHudEnabled = !m_debugHudEnabled;
        m_prevDebugToggleDown = lDown;

        if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_shouldClose = true;
        }

        int ww = 0, wh = 0;
        glfwGetWindowSize(w, &ww, &wh);
        if (ww > 0 && wh > 0) {
            const double cx = static_cast<double>(ww) * 0.5;
            const double cy = static_cast<double>(wh) * 0.5;
            double mx = 0.0, my = 0.0;
            glfwGetCursorPos(w, &mx, &my);
            const float dx = static_cast<float>(mx - cx) * m_mouseSensitivity;
            const float dy = static_cast<float>(my - cy) * m_mouseSensitivity;
            if (m_registry.hasComponent<CameraComponent>(m_camera)) {
                auto& cam = m_registry.getComponent<CameraComponent>(m_camera);
                cam.inputDeltaX = dx;
                cam.inputDeltaY = dy;
            }
            glfwSetCursorPos(w, cx, cy);
        }

        if (m_renderer->shouldClose())
            m_shouldClose = true;
    }

    void onUpdate(double dt) override
    {
        for (Entity pe : m_registry.getEntitiesWith<Position>())
            m_registry.getComponent<Position>(pe).syncPreviousFromCurrent();

        Vec3 viewer{0.0f, 0.0f, 0.0f};
        if (m_registry.hasComponent<Position>(m_player))
            viewer = m_registry.getComponent<Position>(m_player);
        m_terrainEnv.updateStreaming(m_registry, viewer);

        LocomotorIntentSystem::update(m_registry, m_player, m_camera);

        m_playerLocomotion.update(
            m_registry,
            m_spatialGrid,
            &m_terrainEnv.heightField(),
            m_gravity,
            m_floorY,
            dt);

        m_positionToTransformSystem.update(m_registry);

        m_transformSystem.update(m_registry);
        m_attachmentSystem.update(m_registry);

        const float fdt = static_cast<float>(dt);
        m_cameraSystem.update(m_registry, fdt);
        updatePlayerFacingFromCamera();
        m_cameraGroundClampSystem.update(
            m_registry,
            m_spatialGrid,
            &m_terrainEnv.heightField(),
            m_floorY,
            kLayerWorldStatic,
            fdt);

        m_animationSystem.update(m_registry, fdt);
        game::factories::SkinnedCharacterActorFactory::updateGpuSkinPalette(m_registry, m_player);

        m_collisionSystem.update(m_registry, m_spatialGrid);
        m_solidCollisionResponse.update(m_registry);
    }

    void onRender(double) override
    {
        if (!m_renderer)
            return;
        using clock = std::chrono::steady_clock;
        const auto now = clock::now();
        if (m_haveLastFrameTime) {
            const double dt = std::chrono::duration<double>(now - m_lastFrameTime).count();
            if (dt > 1e-9) {
                const float inst = static_cast<float>(1.0 / dt);
                m_fpsSmooth = m_fpsSmooth * 0.92f + inst * 0.08f;
            }
        } else {
            m_haveLastFrameTime = true;
        }
        m_lastFrameTime = now;

        OpenGLDebugHudSnapshot hud;
        hud.enabled         = m_debugHudEnabled;
        hud.fps             = m_fpsSmooth;
        hud.entityCount     = static_cast<int>(m_registry.getAliveEntityCount());
        hud.targetFpsPreset = m_settings.targetFpsPreset;
        if (m_player != INVALID_ENTITY && m_registry.hasComponent<StateMachineComponent>(m_player))
            hud.locomotionState =
                m_registry.getComponent<StateMachineComponent>(m_player).machine.getCurrentState();
        m_renderer->setDebugHudSnapshot(std::move(hud));

        m_renderer->renderFrame(m_registry);

        if (m_debugHudEnabled && m_renderer->window()) {
            char title[512];
            const char* st = "-";
            if (m_player != INVALID_ENTITY && m_registry.hasComponent<StateMachineComponent>(m_player))
                st = m_registry.getComponent<StateMachineComponent>(m_player).machine.getCurrentState().c_str();
            std::snprintf(
                title,
                sizeof(title),
                "Movement Tutorial | %.0f fps | %s | %d entities",
                static_cast<double>(m_fpsSmooth),
                st,
                static_cast<int>(m_registry.getAliveEntityCount()));
            glfwSetWindowTitle(m_renderer->window(), title);
        } else if (m_renderer->window()) {
            glfwSetWindowTitle(m_renderer->window(), "Movement Tutorial");
        }
    }

    void onStop() override
    {
        if (m_renderer && m_renderer->window())
            glfwSetInputMode(m_renderer->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (m_renderer)
            m_renderer->shutdown();
        std::cout << "Movement Tutorial stopped.\n";
    }

    bool shouldClose() const override { return m_shouldClose; }

private:
    static constexpr uint32_t kLayerWorldStatic = 1u << 2;

    void registerComponents()
    {
        m_registry.registerComponent<Position>();
        m_registry.registerComponent<Velocity>();
        m_registry.registerComponent<TransformComponent>();
        m_registry.registerComponent<WorldTransformComponent>();
        m_registry.registerComponent<CameraComponent>();
        m_registry.registerComponent<CollisionBoxComponent>();
        m_registry.registerComponent<StateMachineComponent>();
        m_registry.registerComponent<PlayerTagComponent>();
        m_registry.registerComponent<MassComponent>();
        m_registry.registerComponent<PlayerMovementIntentComponent>();
        m_registry.registerComponent<TerrainSettingsComponent>();
        m_registry.registerComponent<HeightMapComponent>();
        m_registry.registerComponent<TerrainChunkComponent>();
        m_registry.registerComponent<LocomotorControlComponent>();
        m_registry.registerComponent<MovementComponent>();
        m_registry.registerComponent<LocomotionRuntimeComponent>();
        m_registry.registerComponent<GroundingStateComponent>();
        m_registry.registerComponent<LightingComponent>();
        m_registry.registerComponent<HdriEnvironmentComponent>();
        m_registry.registerComponent<Texture2DGlComponent>();
        m_registry.registerComponent<ShaderPipelineComponent>();
        /// Required by renderer pass queries even when no mesh entities exist.
        m_registry.registerComponent<StaticMeshComponent>();
        m_registry.registerComponent<GpuSkinPaletteComponent>();
        m_registry.registerComponent<SocketComponent>();
        m_registry.registerComponent<AttachComponent>();
        m_registry.registerComponent<SkeletonInstanceComponent>();
        m_registry.registerComponent<SkeletonPoseComponent>();
        m_registry.registerComponent<AnimationPlaybackComponent>();
        m_registry.registerComponent<PbrMaterialPresetComponent>();
    }

    void createTerrainSettingsFlat()
    {
        Entity e              = m_registry.createEntity();
        TerrainSettingsComponent ts{};
        ts.chunkSize          = 32;
        ts.scale              = 1.0f;
        ts.renderRadius       = 3;
        ts.flatTerrain        = true;
        ts.flatTerrainHeight  = 0.0f;
        m_registry.addComponent(e, ts);

        PbrMaterialPresetComponent surf{};
        surf.presetRootRelative = "assets/textures/Hex-Tile";
        surf.surfaceUvRepeats   = 4.0f;
        m_registry.addComponent(e, surf);
    }

    static bool wantsMoveGuard(Registry& reg, Entity o)
    {
        const auto& c = reg.getComponent<LocomotorControlComponent>(o);
        return std::abs(c.forward) + std::abs(c.right) > 0.02f;
    }

    static bool sprintHeldGuard(Registry& reg, Entity o)
    {
        return reg.getComponent<LocomotorControlComponent>(o).sprintHeld;
    }

    /// Horizontal facing matches camera look (same basis as LocomotorIntentSystem) when locomoting.
    void updatePlayerFacingFromCamera()
    {
        if (m_player == INVALID_ENTITY || m_camera == INVALID_ENTITY)
            return;
        if (!m_registry.hasComponent<StateMachineComponent>(m_player) ||
            !m_registry.hasComponent<TransformComponent>(m_player))
            return;

        const std::string& st = m_registry.getComponent<StateMachineComponent>(m_player).machine.getCurrentState();
        if (st == "Idle")
            return;

        if (!m_registry.hasComponent<TransformComponent>(m_camera) || !m_registry.hasComponent<CameraComponent>(m_camera))
            return;

        auto& camComp = m_registry.getComponent<CameraComponent>(m_camera);
        auto& camTf   = m_registry.getComponent<TransformComponent>(m_camera);
        Entity lookTarget = m_player;
        if (camComp.enableLookAt && camComp.lookAtTarget != INVALID_ENTITY)
            lookTarget = camComp.lookAtTarget;

        float yaw = camComp.currentYaw;
        if (lookTarget != INVALID_ENTITY && m_registry.hasComponent<TransformComponent>(lookTarget)) {
            const auto& tgtTf = m_registry.getComponent<TransformComponent>(lookTarget);
            const float dx =
                (tgtTf.position.x + camComp.lookAtOffset.x) - camTf.position.x;
            const float dz =
                (tgtTf.position.z + camComp.lookAtOffset.z) - camTf.position.z;
            const float hDist = std::sqrt(dx * dx + dz * dz);
            if (hDist > 1.0e-5f)
                yaw = std::atan2(dx, dz);
        }

        auto& playerTf    = m_registry.getComponent<TransformComponent>(m_player);
        playerTf.rotation = quatFromAxisAngleRad({0.0f, 1.0f, 0.0f}, yaw);
    }

    void setupPlayerStateMachine(Entity e)
    {
        StateMachineConfig config;
        config.initialState = "Idle";

        auto jumpImpulse = [this](Entity o) {
            auto& v  = m_registry.getComponent<Velocity>(o);
            auto& mv = m_registry.getComponent<MovementComponent>(o);
            auto& c  = m_registry.getComponent<LocomotorControlComponent>(o);
            const float bonus = std::max(0.0f, c.forward) * mv.forwardJumpBonus;
            v.y               = mv.jumpImpulse + bonus;
        };

        config.states["Idle"] = {
            "Idle",
            nullptr,
            nullptr,
            nullptr,
            {
                {StateEventType::MoveUp, "Moving", nullptr},
                {StateEventType::Jump, "Jumping", nullptr},
            }};

        config.states["Moving"] = {
            "Moving",
            nullptr,
            nullptr,
            nullptr,
            {
                {StateEventType::Stop, "Idle", nullptr},
                {StateEventType::SprintPress, "Sprinting", nullptr},
                {StateEventType::Jump, "Jumping", nullptr},
            }};

        config.states["Sprinting"] = {
            "Sprinting",
            nullptr,
            nullptr,
            nullptr,
            {
                {StateEventType::Stop, "Idle", nullptr},
                {
                    StateEventType::SprintRelease,
                    "Moving",
                    [this](Entity o) { return wantsMoveGuard(m_registry, o); },
                },
                {
                    StateEventType::SprintRelease,
                    "Idle",
                    [this](Entity o) { return !wantsMoveGuard(m_registry, o); },
                },
                {StateEventType::Jump, "Jumping", nullptr},
            }};

        config.states["Jumping"] = {
            "Jumping",
            jumpImpulse,
            nullptr,
            nullptr,
            {
                {
                    StateEventType::Landed,
                    "Sprinting",
                    [this](Entity o) {
                        return sprintHeldGuard(m_registry, o) && wantsMoveGuard(m_registry, o);
                    },
                },
                {
                    StateEventType::Landed,
                    "Moving",
                    [this](Entity o) {
                        return wantsMoveGuard(m_registry, o) && !sprintHeldGuard(m_registry, o);
                    },
                },
                {
                    StateEventType::Landed,
                    "Idle",
                    [this](Entity o) { return !wantsMoveGuard(m_registry, o); },
                },
            }};

        m_registry.addComponent(e, StateMachineComponent{});
        m_registry.getComponent<StateMachineComponent>(e).machine.initialize(e, config);
    }

    void createPlayerFromFactory()
    {
        m_playerAvatarModel = game::factories::SkinnedCharacterActorFactory::tryLoadGltf(
            m_assetManager,
            "",
            m_playerAvatarPath);

        game::factories::SkinnedCharacterSpawnDesc desc{};
        desc.position         = {0.0f, 1.2f, 0.0f};
        desc.uniformScale     = 1.0f;
        /// Capsule matches third-person demo; footOffset = halfY places feet on ground.
        desc.collisionHalfX   = 0.4f;
        desc.collisionHalfY   = 0.55f;
        desc.collisionHalfZ   = 0.4f;
        desc.physicsLayer     = 1u << 0;
        desc.collidesWithMask = kLayerWorldStatic;
        desc.mass.massKg           = 1.0f;
        desc.mass.gravityScale     = 1.0f;
        desc.mass.footOffset       = 0.55f;
        desc.mass.fallbackGroundY  = m_floorY;
        desc.mass.solidGroundMask  = kLayerWorldStatic;

        m_player = game::factories::SkinnedCharacterActorFactory::spawn(m_registry, desc, m_playerAvatarModel);

        m_registry.addComponent(m_player, PlayerTagComponent{});
        m_registry.addComponent(m_player, LocomotorControlComponent{});
        m_registry.addComponent(m_player, MovementComponent{});
        m_registry.addComponent(m_player, LocomotionRuntimeComponent{});
        m_registry.addComponent(m_player, GroundingStateComponent{});
        m_registry.addComponent(m_player, PlayerMovementIntentComponent{});
        m_registry.addComponent(m_player, ShaderPipelineComponent{});

        setupPlayerStateMachine(m_player);

        if (m_playerAvatarModel)
            std::cout << "Loaded business character mesh + skeleton; idle clip index "
                      << game::factories::SkinnedCharacterActorFactory::findIdleClipIndex(*m_playerAvatarModel)
                      << "\n";
        else
            std::cerr << "Playing as capsule fallback (glTF load failed).\n";
    }

    void createCameraRig()
    {
        m_cameraSocket = m_registry.createEntity();
        m_registry.addComponent(m_cameraSocket, SocketComponent{
            m_player,
            {0.0f, 1.5f, 0.85f},
            {},
            {}});

        m_camera = m_registry.createEntity();
        m_registry.addComponent(m_camera, TransformComponent{});
        m_registry.addComponent(m_camera, WorldTransformComponent{});

        CameraComponent cam{};
        cam.active                   = true;
        cam.enableFollow             = true;
        cam.followLerp               = 2.2f;
        cam.followPositionSpring     = true;
        cam.followSpringFrequency    = 3.4f;
        cam.followSpringDampingRatio = 0.72f;
        cam.enableLookAt             = true;
        cam.lookAtTarget             = m_player;
        cam.lookAtOffset             = {0.0f, 1.05f, 0.0f};
        cam.enableOrbit              = true;
        // Orbit offset uses (sin(yaw), cos(yaw)) on XZ; yaw=0 puts the camera on +Z. π flips to −Z (behind typical forward/+Z character).
        cam.orbitYaw                 = 3.14159265f;
        cam.orbitPitch               = 0.35f;
        cam.orbitDistance            = 6.0f;
        cam.orbitSensitivity         = 0.58f;
        cam.minPitch                 = -0.6f;
        cam.maxPitch                 = 1.0f;
        cam.enableLockOn             = false;
        cam.enableGroundHeightClamp  = true;
        cam.groundClearance          = 0.5f;
        m_registry.addComponent(m_camera, cam);

        {
            auto& playerPos    = m_registry.getComponent<Position>(m_player);
            auto& camTransform = m_registry.getComponent<TransformComponent>(m_camera);
            const float yaw     = cam.orbitYaw;
            const float pitch   = cam.orbitPitch;
            const float dist    = cam.orbitDistance;
            const float cP      = std::cos(pitch);
            const float sP      = std::sin(pitch);
            const float cY      = std::cos(yaw);
            const float sY      = std::sin(yaw);
            camTransform.position.x = playerPos.x + dist * cP * sY;
            camTransform.position.y = playerPos.y + dist * sP;
            camTransform.position.z = playerPos.z + dist * cP * cY;
        }

        m_registry.addComponent(m_camera, AttachComponent{
            m_player,
            m_cameraSocket,
            {0.0f, 0.0f, 0.0f},
            {},
            false,
            true});
    }

    /// Image-based lighting only: no directional sun (see OpenGL ambient zero when HDRI enabled).
    void createHdriOnlyEnvironment()
    {
        Entity h = m_registry.createEntity();
        HdriEnvironmentComponent comp{};
        comp.enabled = true;
#ifdef GAME_ENGINE_PROJECT_ROOT
        comp.hdriAssetPath = std::string(GAME_ENGINE_PROJECT_ROOT) + "/assets/lighting/hdri.webp";
#else
        comp.hdriAssetPath = "/Users/family/Documents/k98/game-engine/assets/lighting/hdri.webp";
#endif
        comp.intensity                  = 1.15f;
        comp.rotationY                  = 0.0f;
        comp.diffuseEnvironmentWeight   = 0.92f;
        comp.specularEnvironmentWeight  = 0.55f;
        m_registry.addComponent(h, comp);
    }

    Registry m_registry;
    AssetManager m_assetManager;
    Entity m_player = INVALID_ENTITY;
    Entity m_staticProp = INVALID_ENTITY;
    Entity m_camera = INVALID_ENTITY;
    Entity m_cameraSocket = INVALID_ENTITY;

    std::shared_ptr<ModelAsset> m_playerAvatarModel;
    std::string m_playerAvatarPath;

    std::unique_ptr<IGraphicsRenderer> m_renderer;
    AnimationSystem m_animationSystem;
    TerrainEnvironmentSystem m_terrainEnv;
    SpatialGridSystem m_spatialGrid;
    PositionToTransformSystem m_positionToTransformSystem;
    TransformSystem m_transformSystem;
    AttachmentSystem m_attachmentSystem;
    CameraSystem m_cameraSystem;
    CameraGroundClampSystem m_cameraGroundClampSystem;
    CollisionSystem m_collisionSystem;
    SolidCollisionResponseSystem m_solidCollisionResponse;
    PlayerLocomotionSystem m_playerLocomotion;

    float m_floorY   = 0.0f;
    float m_gravity  = -28.0f;
    float m_mouseSensitivity = 0.0025f;
    bool m_shouldClose       = false;
    bool m_prevSpaceDown     = false;

    MovementTutorialSettings m_settings;
    bool m_debugHudEnabled = false;
    bool m_prevDebugToggleDown = false;
    std::chrono::steady_clock::time_point m_lastFrameTime{};
    bool m_haveLastFrameTime = false;
    float m_fpsSmooth        = 60.f;
};
