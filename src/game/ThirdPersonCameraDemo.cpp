#pragma once

#include "../core/IGame.hpp"
#include "../ecs/Registry.hpp"
#include "Control.hpp"
#include "../components/StateMachineComponent.hpp"
#include "../game/Components.hpp"
#include "../game/StateEventType.hpp"
#include "../statemachine/StateMachine.hpp"
#include "../statemachine/StateMachineTypes.hpp"

#include "../components/TransformComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../components/SocketComponent.hpp"
#include "../components/AttachComponent.hpp"
#include "../components/CameraComponent.hpp"

#include "../components/CollisionBoxComponent.hpp"
#include "../components/RayComponent.hpp"
#include "../components/RayHitComponent.hpp"
#include "../components/GameplayTags.hpp"
#include "../components/FacingRayDriverComponent.hpp"

#include "../systems/TransformSystem.hpp"
#include "../systems/SocketSystem.hpp"
#include "../systems/AttachmentSystem.hpp"
#include "../systems/CameraSystem.hpp"
#include "../systems/CameraGroundClampSystem.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../systems/CollisionSystem.hpp"
#include "../systems/RaycastSystem.hpp"
#include "../systems/AnimationSystem.hpp"
#include "../systems/BoneSyncSystem.hpp"
#include "../systems/PositionToTransformSystem.hpp"
#include "../systems/CollisionLastPositionSyncSystem.hpp"
#include "../systems/FacingRaySystem.hpp"
#include "../systems/AudioSystem.hpp"
#include "../systems/TerrainEnvironmentSystem.hpp"
#include "../systems/GravitySystem.hpp"
#include "../systems/SolidCollisionResponseSystem.hpp"
#include "../systems/SequenceSystem.hpp"

#include "../components/SequenceComponent.hpp"
#include "../sequence/Sequence.hpp"

#include "../components/TerrainSettingsComponent.hpp"
#include "../components/TerrainChunkComponent.hpp"
#include "../components/HeightMapComponent.hpp"
#include "../components/MassComponent.hpp"
#include "../components/PlayerMovementIntentComponent.hpp"

#include "../core/assets/AssetManager.hpp"
#include "../core/assets/StreamingLoadService.hpp"
#include "../core/assets/StreamingAssetCache.hpp"
#include "../core/SystemUpdateGroups.hpp"
#include "../core/ThreadService.hpp"
#include "../core/assets/importers/GltfModelImporter.hpp"
#include "../core/assets/IAsset.hpp"
#include "../core/assets/ModelAsset.hpp"
#include "../core/assets/AnimationClipData.hpp"
#include "../components/StaticMeshComponent.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

#include "../components/SkeletonInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/AnimationPlaybackComponent.hpp"
#include "../components/BoneInstanceComponent.hpp"
#include "../components/StreamingAnchorComponent.hpp"
#include "../components/StreamableModelComponent.hpp"
#include "../components/SkinnedMeshComponent.hpp"
#include "../components/GpuSkinPaletteComponent.hpp"
#include "../components/LightingComponent.hpp"
#include "../components/HdriEnvironmentComponent.hpp"
#include "../components/AudioComponent.hpp"

#include "../animation/AnimationSampling.hpp"
#include "../graphics/opengl/OpenGLVer2Renderer.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <random>

namespace {

int findIdleClipIndex(const ModelAsset& model)
{
    for (size_t i = 0; i < model.clips.size(); ++i) {
        const std::string& n = model.clips[i].name;
        if (n.find("Idle") != std::string::npos || n.find("idle") != std::string::npos)
            return static_cast<int>(i);
    }
    return model.clips.empty() ? -1 : 0;
}

/// Moving state: continuous twist on `LeftHand_23` (local +Y axis).
int appendLeftHandTwistClip(ModelAsset& model)
{
    auto it = model.skeleton.boneMap.find("LeftHand_23");
    if (it == model.skeleton.boneMap.end())
        return -1;
    const int bi = it->second;
    if (bi < 0 || static_cast<size_t>(bi) >= model.skeleton.bones.size())
        return -1;

    const Quat qRest = quatNormalize(model.skeleton.bones[static_cast<size_t>(bi)].restRotation);
    constexpr float kPi = 3.14159265f;
    const Quat qHalf = quatNormalize(quatMul(quatFromAxisAngleRad({0.f, 1.f, 0.f}, kPi), qRest));

    AnimationClipData clip;
    clip.name = "custom_left_hand_twist";
    clip.durationSec = 3.0f;
    ClipBoneChannel rotCh;
    rotCh.boneIndex = bi;
    rotCh.path = AnimChannelPath::Rotation;
    rotCh.quatKeys.push_back({0.0f, qRest});
    rotCh.quatKeys.push_back({1.5f, qHalf});
    rotCh.quatKeys.push_back({3.0f, qRest});
    clip.channels.push_back(std::move(rotCh));
    model.clips.push_back(std::move(clip));
    return static_cast<int>(model.clips.size() - 1);
}

/// Slowing state: forearm flex / extend on `LeftForeArm_24`.
int appendLeftForeArmBobClip(ModelAsset& model)
{
    auto it = model.skeleton.boneMap.find("LeftForeArm_24");
    if (it == model.skeleton.boneMap.end())
        return -1;
    const int bi = it->second;
    if (bi < 0 || static_cast<size_t>(bi) >= model.skeleton.bones.size())
        return -1;

    const Quat qRest = quatNormalize(model.skeleton.bones[static_cast<size_t>(bi)].restRotation);
    constexpr float kPi = 3.14159265f;
    const float d2r = kPi / 180.f;
    const Quat qUp = quatNormalize(quatMul(quatFromAxisAngleRad({1.f, 0.f, 0.f}, 35.f * d2r), qRest));
    const Quat qDn = quatNormalize(quatMul(quatFromAxisAngleRad({1.f, 0.f, 0.f}, -28.f * d2r), qRest));

    AnimationClipData clip;
    clip.name = "custom_left_forearm_bob";
    clip.durationSec = 2.5f;
    ClipBoneChannel rotCh;
    rotCh.boneIndex = bi;
    rotCh.path = AnimChannelPath::Rotation;
    rotCh.quatKeys.push_back({0.0f, qRest});
    rotCh.quatKeys.push_back({0.6f, qUp});
    rotCh.quatKeys.push_back({1.25f, qRest});
    rotCh.quatKeys.push_back({1.85f, qDn});
    rotCh.quatKeys.push_back({2.5f, qRest});
    clip.channels.push_back(std::move(rotCh));
    model.clips.push_back(std::move(clip));
    return static_cast<int>(model.clips.size() - 1);
}

} // namespace

class ThirdPersonCameraDemo final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Third Person Camera Demo Started (press T for overhead taunt sequence)\n";

        m_threadService.configure({});
        m_threadService.start();

        registerComponents();

        m_assetManager.registerImporter("gltf", std::make_shared<GltfModelImporter>());
        m_assetManager.registerImporter("glb", std::make_shared<GltfModelImporter>());
        m_streamingLoadService.setTrackedBoneNames({"hand"});

        createPlayer();
        loadPlayerAvatarAssetFromDisk();
        attachPlayerSkeletonAnimation();
        createHandSocketAndAttachedEnemy();
        createLightingEntities();
        createHdriEnvironmentEntity();
        createPlayerSlowingSfxEntity();
        createTerrainSettings();
        createEnemies();
        createCameraRig();
        createTauntShowcaseSequence();

        m_spatialGrid.cellSize = 4.0f;

        m_collisionSystem.addHandler(
            [this](const CollisionEvent& ev)
            {
                if (ev.type != CollisionEventType::Enter)
                {
                    return;
                }
                const bool aIsPlayer = (ev.a == m_player);
                const bool bIsPlayer = (ev.b == m_player);
                if (!aIsPlayer && !bIsPlayer)
                {
                    return;
                }
                const Entity other = aIsPlayer ? ev.b : ev.a;
                if (isEnemyEntity(other))
                {
                    ++m_playerEnemyCollisionEnters;
                }
            });

        m_positionToTransformSystem.update(m_registry);
        m_collisionLastPositionSyncSystem.seed(m_registry);

        createPlayerFacingRay();

        m_streamingLoadService.update(m_registry);
        m_animationSystem.update(m_registry, 0.0f);
        m_boneSyncSystem.update(m_registry);
        m_transformSystem.update(m_registry);
        m_socketSystem.update(m_registry);
        m_attachmentSystem.update(m_registry);

        if (!m_gl.init(1280, 720, "Third Person Camera Demo"))
        {
            std::cerr << "OpenGL window init failed.\n";
            m_shouldClose = true;
            m_control = std::make_unique<Control>(m_player, m_camera, 5.0f, true);
            createDemoSolidPlatform(false);
        }
        else
        {
            m_control = std::make_unique<Control>(m_player, m_camera, 5.0f, false);
            if (m_gl.window())
            {
                glfwSetInputMode(m_gl.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                int ww = 0, wh = 0;
                glfwGetWindowSize(m_gl.window(), &ww, &wh);
                if (ww > 0 && wh > 0)
                    glfwSetCursorPos(m_gl.window(), static_cast<double>(ww) * 0.5, static_cast<double>(wh) * 0.5);
            }
            uploadPlayerAvatarToGpu();
            createDemoSolidPlatform(true);
            if (!m_audioSystem.init())
            {
                std::cerr << "Audio engine init failed (continuing without sound).\n";
            }
        }

        m_collisionLastPositionSyncSystem.seed(m_registry);
        m_terrainEnv.snapGroundedActorsToSurface(m_registry, m_spatialGrid);
        m_transformSystem.update(m_registry);
    }

    void onInput() override
    {
        if (m_gl.window())
        {
            glfwPollEvents();

            GLFWwindow* w = m_gl.window();
            InputState st{};
            if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)
                st.moveZ -= 1.0f;
            if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)
                st.moveZ += 1.0f;
            // Flipped strafe vs previous (A = right, D = left).
            if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)
                st.moveX += 1.0f;
            if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)
                st.moveX -= 1.0f;
            if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS)
                st.jumpPressed = true;
            const bool tauntDown = glfwGetKey(w, GLFW_KEY_T) == GLFW_PRESS;
            if (tauntDown && !m_prevTauntKeyDown)
                tryPlayTauntShowcaseSequence();
            m_prevTauntKeyDown = tauntDown;
            if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS)
            {
                glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                st.quit = true;
            }

            int ww = 0, wh = 0;
            glfwGetWindowSize(w, &ww, &wh);
            if (ww > 0 && wh > 0)
            {
                const double cx = static_cast<double>(ww) * 0.5;
                const double cy = static_cast<double>(wh) * 0.5;
                double mx = 0.0;
                double my = 0.0;
                glfwGetCursorPos(w, &mx, &my);
                const float dx = static_cast<float>(mx - cx);
                const float dy = static_cast<float>(my - cy);
                st.mouseDeltaX = dx * m_mouseSensitivity;
                st.mouseDeltaY = dy * m_mouseSensitivity;
                glfwSetCursorPos(w, cx, cy);
            }

            m_control->submitInput(m_registry, st);
            if (m_tauntSequenceCameraOverride && m_registry.hasComponent<CameraComponent>(m_camera))
            {
                auto& cam = m_registry.getComponent<CameraComponent>(m_camera);
                cam.inputDeltaX = 0.0f;
                cam.inputDeltaY = 0.0f;
            }
        }
        else
        {
            m_control->handleInput(m_registry);
            if (m_tauntSequenceCameraOverride && m_registry.hasComponent<CameraComponent>(m_camera))
            {
                auto& cam = m_registry.getComponent<CameraComponent>(m_camera);
                cam.inputDeltaX = 0.0f;
                cam.inputDeltaY = 0.0f;
            }
        }

        if (m_control->shouldClose() || m_gl.shouldClose())
            m_shouldClose = true;
    }

    void onUpdate(double dt) override
    {
        m_elapsedTime += dt;

        // Environment first: terrain chunks + heightfield mirror (see TerrainChunkSystem).
        updateEnvironmentGroup(dt);

        // Scripted sequences (e.g. taunt showcase) before actor FSM / physics integration.
        m_sequenceSystem.update(m_registry, static_cast<float>(dt));

        // Gameplay / state integration (runs before transforms propagate).
        updateActors(dt);
        m_positionToTransformSystem.update(m_registry);

        if (m_registry.hasComponent<TransformComponent>(m_player))
        {
            m_streamingCache.setViewerPosition(m_registry.getComponent<TransformComponent>(m_player).position);
        }

        // Simulation → Physics (SystemUpdateGroups.hpp).
        updateSimulationGroup(dt);
        updatePhysicsGroup(dt);

        syncEnemyAggroFromFacingRay();
    }

    void onRender(double) override
    {
        printTerminalStatusTable();
        m_gl.renderFrame(m_registry);
    }

    void onStop() override
    {
        m_threadService.shutdown();

        m_audioSystem.shutdown();

        if (m_gl.window())
            glfwSetInputMode(m_gl.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_gl.shutdown();
        std::cout << "Third Person Camera Demo Stopped\n";
    }

    bool shouldClose() const override
    {
        return m_shouldClose;
    }

private:
    // Collision layers — CollisionBoxComponent::layer / collidesWithMask; rays use RayComponent::layerMask.
    static constexpr uint32_t kLayerPlayer             = 1u << 0;
    static constexpr uint32_t kLayerEnemyCrowd         = 1u << 1;
    static constexpr uint32_t kLayerWorldStatic        = 1u << 2;
    static constexpr uint32_t kFacingRayEnemyLayerMask = kLayerEnemyCrowd;

    // -------------------------------------------------
    // Setup
    // -------------------------------------------------

    void registerComponents()
    {
        m_registry.registerComponent<Position>();
        m_registry.registerComponent<Velocity>();
        m_registry.registerComponent<Health>();
        m_registry.registerComponent<StateMachineComponent>();

        m_registry.registerComponent<TransformComponent>();
        m_registry.registerComponent<WorldTransformComponent>();
        m_registry.registerComponent<SocketComponent>();
        m_registry.registerComponent<AttachComponent>();
        m_registry.registerComponent<CameraComponent>();
        m_registry.registerComponent<CollisionBoxComponent>();
        m_registry.registerComponent<RayComponent>();
        m_registry.registerComponent<RaycastHitComponent>();

        m_registry.registerComponent<SkeletonInstanceComponent>();
        m_registry.registerComponent<SkeletonPoseComponent>();
        m_registry.registerComponent<AnimationPlaybackComponent>();
        m_registry.registerComponent<BoneInstanceComponent>();
        m_registry.registerComponent<StreamingAnchorComponent>();
        m_registry.registerComponent<StreamableModelComponent>();
        m_registry.registerComponent<SkinnedMeshComponent>();

        m_registry.registerComponent<PlayerTagComponent>();
        m_registry.registerComponent<EnemyTagComponent>();
        m_registry.registerComponent<FacingRayDriverComponent>();
        m_registry.registerComponent<StaticMeshComponent>();
        m_registry.registerComponent<GpuSkinPaletteComponent>();
        m_registry.registerComponent<LightingComponent>();
        m_registry.registerComponent<HdriEnvironmentComponent>();
        m_registry.registerComponent<AudioComponent>();
        m_registry.registerComponent<TerrainSettingsComponent>();
        m_registry.registerComponent<TerrainChunkComponent>();
        m_registry.registerComponent<HeightMapComponent>();
        m_registry.registerComponent<MassComponent>();
        m_registry.registerComponent<PlayerMovementIntentComponent>();
        m_registry.registerComponent<SequenceComponent>();
    }

    void createPlayer()
    {
        m_player = m_registry.createEntity();

        m_registry.addComponent(m_player, Position{0.0f, 0.0f, 0.0f});
        m_registry.addComponent(m_player, Velocity{0.0f, 0.0f, 0.0f});
        m_registry.addComponent(m_player, Health{200});
        m_registry.addComponent(m_player, TransformComponent{});
        m_registry.addComponent(m_player, WorldTransformComponent{});

        {
            CollisionBoxComponent box{};
            box.setBoxSize(
                m_collisionHalfX * 2.0f,
                m_collisionHalfY * 2.0f,
                m_collisionHalfZ * 2.0f);
            box.lastPosition     = {0.0f, 0.0f, 0.0f};
            box.layer            = kLayerPlayer;
            box.collidesWithMask = kLayerWorldStatic | kLayerEnemyCrowd;
            m_registry.addComponent(m_player, box);
        }

        setupPlayerStateMachine(m_player);
        m_registry.addComponent(m_player, PlayerTagComponent{});
        m_registry.addComponent(m_player, PlayerMovementIntentComponent{});
        {
            MassComponent m{};
            m.mass              = 1.0f;
            m.gravityScale      = 1.0f;
            m.footOffset        = 0.02f;
            m.fallbackGroundY   = m_floorY;
            m.solidGroundMask   = kLayerWorldStatic;
            m_registry.addComponent(m_player, m);
        }
    }

    void createEnemies()
    {
        constexpr int kEnemyCount = 100;
        constexpr int kGridSide   = 10;
        constexpr float kSpacing  = 5.0f;
        const float kGridOrigin = -0.5f * static_cast<float>(kGridSide - 1) * kSpacing;

        m_enemyWander.resize(static_cast<std::size_t>(kEnemyCount));

        for (int i = 0; i < kEnemyCount; ++i)
        {
            Entity e = m_registry.createEntity();
            const int row = i / kGridSide;
            const int col = i % kGridSide;
            const float startX = kGridOrigin + static_cast<float>(col) * kSpacing;
            const float startZ = kGridOrigin + static_cast<float>(row) * kSpacing;

            m_registry.addComponent(e, Position{startX, 0.0f, startZ});
            m_registry.addComponent(e, Velocity{0.0f, 0.0f, 0.0f});
            m_registry.addComponent(e, Health{100});
            m_registry.addComponent(e, TransformComponent{});
            m_registry.addComponent(e, WorldTransformComponent{});

            auto& t = m_registry.getComponent<TransformComponent>(e);
            t.position = {startX, 0.0f, startZ};

            {
                CollisionBoxComponent box{};
                box.setBoxSize(
                    m_collisionHalfX * 2.0f,
                    m_collisionHalfY * 2.0f,
                    m_collisionHalfZ * 2.0f);
                box.lastPosition     = {startX, 0.0f, startZ};
                box.layer            = kLayerEnemyCrowd;
                box.collidesWithMask = kLayerWorldStatic | kLayerPlayer;
                m_registry.addComponent(e, box);
            }

            setupEnemyStateMachine(e);
            m_registry.addComponent(e, EnemyTagComponent{static_cast<std::uint32_t>(i)});
            {
                MassComponent m{};
                m.mass              = 1.0f;
                m.gravityScale      = 1.0f;
                m.footOffset        = 0.02f;
                m.fallbackGroundY   = m_floorY;
                m.solidGroundMask   = kLayerWorldStatic;
                m_registry.addComponent(e, m);
            }
            m_enemies.push_back(e);
        }
    }

    /// Global terrain parameters + procedural heightmap (noise biomes).
    void createTerrainSettings()
    {
        m_terrainSettingsEntity = m_registry.createEntity();
        TerrainSettingsComponent ts{};
        ts.chunkSize    = 32;
        ts.scale        = 1.0f;
        ts.renderRadius = 2;
        m_registry.addComponent(m_terrainSettingsEntity, ts);
    }

    /// Static solid AABB + optional textured mesh (same glTF upload as player). Visible only when GPU path runs.
    void createDemoSolidPlatform(bool withStaticMesh)
    {
        Entity e = m_registry.createEntity();
        m_registry.addComponent(e, TransformComponent{});
        auto& t = m_registry.getComponent<TransformComponent>(e);
        t.position = {10.0f, 2.5f, 4.0f};
        m_registry.addComponent(e, WorldTransformComponent{});

        CollisionBoxComponent box{};
        box.setBoxSize(2.8f, 1.1f, 2.8f);
        box.lastPosition     = t.position;
        box.layer            = kLayerWorldStatic;
        box.collidesWithMask = kLayerPlayer | kLayerEnemyCrowd;
        box.isStatic         = true;
        box.blocksMovement   = true;
        m_registry.addComponent(e, box);

        if (withStaticMesh && !m_playerAvatarPath.empty()) {
            StaticMeshComponent sm{};
            sm.assetCacheKey   = m_playerAvatarPath;
            sm.uniformScale    = 0.2f;
            sm.gpuRegistered   = true;
            m_registry.addComponent(e, sm);
        }
    }

    /// CPU load of `assets/business/man/scene.gltf` (rig + embedded idle clip; before GL init).
    bool loadPlayerAvatarAssetFromDisk()
    {
        auto tryLoad = [this](const std::string& path) -> bool {
            if (path.empty())
                return false;
            std::shared_ptr<IAsset> asset = m_assetManager.load(path);
            if (!asset)
                return false;
            auto model = std::dynamic_pointer_cast<ModelAsset>(asset);
            if (!model || model->meshes.empty())
                return false;
            m_playerAvatarModel = model;
            m_playerAvatarPath = path;
            std::cout << "Loaded player avatar (CPU): " << path << " (" << model->meshes.size() << " mesh parts, "
                      << model->skeleton.bones.size() << " bones, " << model->clips.size() << " clips)\n";
            return true;
        };

#ifdef GAME_ENGINE_PROJECT_ROOT
        {
            const std::string root = GAME_ENGINE_PROJECT_ROOT;
            if (!root.empty() && tryLoad(root + "/assets/business/man/scene.gltf"))
                return true;
        }
#endif

        static const char* relativePaths[] = {
            "assets/business/man/scene.gltf",
            "../assets/business/man/scene.gltf",
        };
        for (const char* p : relativePaths) {
            if (tryLoad(p))
                return true;
        }

        std::cerr << "Note: could not load player avatar (expected assets/business/man/scene.gltf).\n";
        return false;
    }

    /// After `m_gl.init()`, uploads meshes and adds `StaticMeshComponent` + `GpuSkinPaletteComponent`.
    void uploadPlayerAvatarToGpu()
    {
        if (!m_playerAvatarModel || m_playerAvatarPath.empty())
            return;
        if (!m_gl.uploadStaticModel(*m_playerAvatarModel, m_playerAvatarPath)) {
            std::cerr << "OpenGL upload failed for " << m_playerAvatarPath << "\n";
            return;
        }
        StaticMeshComponent sm{};
        sm.assetCacheKey = m_playerAvatarPath;
        // Business man glTF is already Y-up; scale ~1 unit tall (0.02 was for tiny lynx-only tuning).
        sm.modelSpaceRotation = Quat::Identity();
        sm.uniformScale = 1.0f;
        sm.gpuRegistered = true;
        m_registry.addComponent(m_player, sm);
        m_registry.addComponent(m_player, GpuSkinPaletteComponent{});
        std::cout << "Uploaded player avatar to GPU: " << m_playerAvatarPath << "\n";
    }

    /// Cross-fade locomotion clips: Idle = glTF idle, Moving = LeftHand twist, Slowing = forearm bob.
    void updatePlayerLocomotionAnimation(float dt)
    {
        if (m_clipIdle < 0)
            return;
        if (!m_registry.hasComponent<StateMachineComponent>(m_player) ||
            !m_registry.hasComponent<AnimationPlaybackComponent>(m_player))
            return;

        auto& sm = m_registry.getComponent<StateMachineComponent>(m_player).machine;
        auto& play = m_registry.getComponent<AnimationPlaybackComponent>(m_player);
        const std::string& st = sm.getCurrentState();

        constexpr float kFadeSpeed = 3.2f;

        if (m_animCrossfadeActive) {
            play.blendAlpha += dt * kFadeSpeed;
            if (play.blendAlpha >= 1.f) {
                play.primaryClip = play.secondaryClip;
                play.secondaryClip = -1;
                play.blendAlpha = 0.f;
                play.speedSecondary = 0.f;
                play.invalidatePoseCache = true;
                m_animCrossfadeActive = false;
            }
            return;
        }

        int target = m_clipIdle;
        if (st == "Moving")
            target = m_clipMove;
        else if (st == "Slowing")
            target = m_clipSlow;

        if (target >= 0 && target != play.primaryClip) {
            play.secondaryClip = target;
            play.blendAlpha = 0.f;
            play.speedSecondary = 1.f;
            play.loopSecondary = true;
            play.invalidatePoseCache = true;
            m_animCrossfadeActive = true;
        }
    }

    void updatePlayerGpuSkin()
    {
        if (!m_registry.hasComponent<GpuSkinPaletteComponent>(m_player))
            return;
        if (!m_registry.hasComponent<SkeletonInstanceComponent>(m_player) ||
            !m_registry.hasComponent<AnimationPlaybackComponent>(m_player))
            return;

        auto& skel = m_registry.getComponent<SkeletonInstanceComponent>(m_player);
        if (!skel.model || skel.model->skeleton.bones.empty())
            return;

        auto& play = m_registry.getComponent<AnimationPlaybackComponent>(m_player);
        std::vector<Mat4> global;
        sampleAnimationBlendedFull(
            *skel.model,
            play.primaryClip,
            play.timePrimary,
            play.secondaryClip,
            play.timeSecondary,
            play.blendAlpha,
            play.loopPrimary,
            play.loopSecondary,
            global);
        if (global.empty())
            return;

        auto& pal = m_registry.getComponent<GpuSkinPaletteComponent>(m_player);
        computeSkinMatrices(skel.model->skeleton, global, pal.jointSkinMatrices);
    }

    void createCameraRig()
    {
        // Anchor/socket attached to player. This gives us a reusable follow pivot.
        m_cameraSocket = m_registry.createEntity();
        m_registry.addComponent(m_cameraSocket, SocketComponent{
            m_player,
            // World +Z is behind the player while W moves along -Z (see Control).
            {0.0f, 1.5f, 0.85f},
            {},
            {}
        });

        // Camera entity
        m_camera = m_registry.createEntity();
        m_registry.addComponent(m_camera, TransformComponent{});
        m_registry.addComponent(m_camera, WorldTransformComponent{});

        CameraComponent cam;
        cam.active = true;

        // Orbit angles use followLerp; world position uses a slight under-damped spring
        // so the rig eases into place with a small overshoot when you strafe / move.
        cam.enableFollow = true;
        cam.followLerp = 2.2f;
        cam.followPositionSpring = true;
        cam.followSpringFrequency = 3.4f;
        cam.followSpringDampingRatio = 0.72f;

        // Look-at behavior
        cam.enableLookAt = true;
        cam.lookAtTarget = m_player;
        // Aim at upper chest; pitch in CameraSystem centers this in frame vertically.
        cam.lookAtOffset = {0.0f, 1.05f, 0.0f};

        // Orbit behavior
        cam.enableOrbit = true;
        cam.orbitYaw = 0.0f; // +Z offset: behind player when forward is -Z (W)
        cam.orbitPitch = 0.35f;
        cam.orbitDistance = 6.0f;
        cam.orbitSensitivity = 0.58f;
        cam.minPitch = -0.6f;
        cam.maxPitch = 1.0f;

        // Lock-on disabled by default
        cam.enableLockOn = false;
        cam.lockOnTarget = INVALID_ENTITY;

        cam.enableGroundHeightClamp = true;
        cam.groundClearance         = 0.5f;

        m_registry.addComponent(m_camera, cam);

        // Start at the orbit point (CameraSystem owns position when orbit/spring is on;
        // Attachment no longer snaps the camera to the socket each frame).
        {
            auto& playerPos = m_registry.getComponent<Position>(m_player);
            auto& camTransform = m_registry.getComponent<TransformComponent>(m_camera);
            const float yaw = cam.orbitYaw;
            const float pitch = cam.orbitPitch;
            const float dist = cam.orbitDistance;
            const float cosPitch = std::cos(pitch);
            const float sinPitch = std::sin(pitch);
            const float cosYaw = std::cos(yaw);
            const float sinYaw = std::sin(yaw);
            camTransform.position.x = playerPos.x + dist * cosPitch * sinYaw;
            camTransform.position.y = playerPos.y + dist * sinPitch;
            camTransform.position.z = playerPos.z + dist * cosPitch * cosYaw;
        }

        // Socket rig for other consumers; inheritPosition=false so AttachmentSystem does not
        // overwrite transform.position — CameraSystem owns the camera world position.
        m_registry.addComponent(m_camera, AttachComponent{
            m_player,
            m_cameraSocket,
            {0.0f, 0.0f, 0.0f},
            {},
            false,
            true
        });
    }

    void tryPlayTauntShowcaseSequence()
    {
        if (m_tauntSequenceEntity == INVALID_ENTITY || !m_registry.hasComponent<SequenceComponent>(m_tauntSequenceEntity))
            return;
        auto& sc = m_registry.getComponent<SequenceComponent>(m_tauntSequenceEntity);
        if (sc.player.playing)
            return;
        sc.player.play();
    }

    void beginTauntShowcaseCameraAndEnemies()
    {
        if (!m_registry.hasComponent<CameraComponent>(m_camera))
            return;

        auto& cam = m_registry.getComponent<CameraComponent>(m_camera);
        m_savedOrbitYaw              = cam.orbitYaw;
        m_savedOrbitPitch            = cam.orbitPitch;
        m_savedOrbitDistance         = cam.orbitDistance;
        m_savedCurrentYaw            = cam.currentYaw;
        m_savedCurrentPitch          = cam.currentPitch;
        m_savedFollowVelocity        = cam.followPositionVelocity;

        m_tauntSequenceCameraOverride = true;

        cam.orbitYaw      = 0.0f;
        cam.orbitPitch    = std::min(cam.maxPitch, 1.05f);
        cam.orbitDistance = 17.0f;
        cam.currentYaw    = cam.orbitYaw;
        cam.currentPitch  = cam.orbitPitch;

        for (Entity e : m_enemies)
        {
            if (!m_registry.hasComponent<StateMachineComponent>(e))
                continue;
            m_registry.getComponent<StateMachineComponent>(e).machine.forceTransitionTo("Taunt");
        }
    }

    void finishTauntShowcaseSequence()
    {
        m_tauntSequenceCameraOverride = false;

        if (m_registry.hasComponent<CameraComponent>(m_camera))
        {
            auto& cam                   = m_registry.getComponent<CameraComponent>(m_camera);
            cam.orbitYaw                = m_savedOrbitYaw;
            cam.orbitPitch              = m_savedOrbitPitch;
            cam.orbitDistance           = m_savedOrbitDistance;
            cam.currentYaw              = m_savedCurrentYaw;
            cam.currentPitch            = m_savedCurrentPitch;
            cam.followPositionVelocity  = m_savedFollowVelocity;
        }

        for (Entity e : m_enemies)
        {
            if (!m_registry.hasComponent<StateMachineComponent>(e))
                continue;
            auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
            if (sm.getCurrentState() == "Taunt")
                sm.forceTransitionTo("Idle");
        }
    }

    void createTauntShowcaseSequence()
    {
        m_tauntSequence = std::make_shared<Sequence>();
        m_tauntSequence->duration = 6.0f;

        SequenceAction start{};
        start.time   = 0.0f;
        start.action = [this](Entity)
        {
            beginTauntShowcaseCameraAndEnemies();
        };
        m_tauntSequence->actions.push_back(std::move(start));

        m_tauntSequenceEntity = m_registry.createEntity();
        SequenceComponent sc{};
        sc.player.sequence = m_tauntSequence;
        sc.onFinished      = [this]()
        {
            finishTauntShowcaseSequence();
        };
        m_registry.addComponent(m_tauntSequenceEntity, std::move(sc));
    }

    // -------------------------------------------------
    // Update
    // -------------------------------------------------

    // SystemUpdateGroup::Environment — streaming anchors, proximity loads, completion → ECS spawn (main thread).
    void updateEnvironmentGroup(double dt)
    {
        (void)dt;
        Vec3 viewer{0.0f, 0.0f, 0.0f};
        if (m_registry.hasComponent<Position>(m_player))
            viewer = m_registry.getComponent<Position>(m_player);
        m_terrainEnv.updateStreaming(m_registry, viewer);
        m_streamingLoadService.update(m_registry);
    }

    // SystemUpdateGroup::Simulation — animation, hierarchy, camera (after Position → Transform).
    void updateSimulationGroup(double dt)
    {
        const float fdt = static_cast<float>(dt);
        syncPlayerSlowingSfx();
        updatePlayerLocomotionAnimation(fdt);
        m_animationSystem.update(m_registry, fdt);
        updatePlayerGpuSkin();
        m_boneSyncSystem.update(m_registry);
        m_transformSystem.update(m_registry);
        m_socketSystem.update(m_registry);
        m_attachmentSystem.update(m_registry);
        m_cameraSystem.update(m_registry, fdt);
        m_cameraGroundClampSystem.update(
            m_registry,
            m_spatialGrid,
            &m_terrainEnv.heightField(),
            m_floorY,
            kLayerWorldStatic,
            fdt);
        m_audioSystem.update(m_registry);
    }

    // SystemUpdateGroup::Physics — collision, facing ray, raycast (after transforms and camera).
    void updatePhysicsGroup(double dt)
    {
        (void)dt;
        m_collisionSystem.update(m_registry, m_spatialGrid);
        m_solidCollisionResponse.update(m_registry);
        m_facingRaySystem.update(m_registry);
        m_raycastSystem.update(m_registry);
    }

    void updateActors(double dt)
    {
        auto actors = m_registry.getEntitiesWith<Position, Velocity, StateMachineComponent>();

        for (auto e : actors)
        {
            auto& sm  = m_registry.getComponent<StateMachineComponent>(e).machine;
            auto& pos = m_registry.getComponent<Position>(e);
            auto& vel = m_registry.getComponent<Velocity>(e);

            if (e == m_player && m_registry.hasComponent<PlayerMovementIntentComponent>(m_player))
            {
                const auto& intent = m_registry.getComponent<PlayerMovementIntentComponent>(m_player);
                vel.x              = intent.horizontalVelX;
                vel.z              = intent.horizontalVelZ;
                if (intent.jumpPressed &&
                    GravitySystem::isGroundedForJump(
                        m_registry, m_player, m_spatialGrid, &m_terrainEnv.heightField()))
                    vel.y = m_playerJumpSpeed;
            }

            if (e == m_player)
            {
                updatePlayerStateIntent(sm, vel);
            }

            sm.update(dt);
            applyStateBehavior(sm, vel, dt);

            if (m_registry.hasComponent<EnemyTagComponent>(e) && sm.getCurrentState() != "Chase" &&
                sm.getCurrentState() != "Taunt")
            {
                applyEnemyWander(e, vel);
            }

            const float fdt = static_cast<float>(dt);
            GravitySystem::applyGravityForEntity(m_registry, e, m_gravity, fdt);

            pos.x += vel.x * fdt;
            pos.y += vel.y * fdt;
            pos.z += vel.z * fdt;

            if (m_registry.hasComponent<MassComponent>(e))
            {
                const auto& mass = m_registry.getComponent<MassComponent>(e);
                GravitySystem::resolveGroundPenetration(
                    m_registry, m_spatialGrid, &m_terrainEnv.heightField(), e, mass);
            }
            else if (pos.y < m_floorY)
            {
                pos.y = m_floorY;
                vel.y = 0.0f;
            }
        }
    }

    void updatePlayerStateIntent(StateMachine& sm, Velocity& vel)
    {
        const bool hasVelocity =
            std::abs(vel.x) > 0.01f ||
            std::abs(vel.z) > 0.01f;

        if (hasVelocity)
        {
            m_lastMoveInputTime = m_elapsedTime;

            if (sm.getCurrentState() == "Idle" || sm.getCurrentState() == "Slowing")
            {
                sm.handleEvent(StateEventType::MoveUp);
            }
        }
        else
        {
            const bool inputExpired = (m_elapsedTime - m_lastMoveInputTime) > m_inputGrace;

            if (inputExpired && sm.getCurrentState() == "Moving")
            {
                sm.handleEvent(StateEventType::Stop);
            }
        }
    }

    void applyStateBehavior(StateMachine& sm, Velocity& vel, double dt)
    {
        const std::string current = sm.getCurrentState();

        if (current == "Slowing")
        {
            const float damping = std::pow(m_slowingFactorPer60Fps, static_cast<float>(dt * 60.0));
            vel.x *= damping;
            vel.z *= damping;

            if (std::abs(vel.x) < m_velocityDeadZone) vel.x = 0.0f;
            if (std::abs(vel.z) < m_velocityDeadZone) vel.z = 0.0f;

            if (vel.x == 0.0f && vel.z == 0.0f)
            {
                sm.handleEvent(StateEventType::Stop);
            }
        }
    }

    /// Slow random walk on XZ; uses EnemyTagComponent::slot to index wander state.
    void applyEnemyWander(Entity e, Velocity& vel)
    {
        const std::uint32_t slot = m_registry.getComponent<EnemyTagComponent>(e).slot;
        if (slot >= m_enemyWander.size())
            return;

        EnemyWanderState& w = m_enemyWander[slot];
        auto&               pos = m_registry.getComponent<Position>(e);

        std::uniform_real_distribution<double> nextGap(0.9, 3.2);
        std::uniform_real_distribution<float>  angle(0.0f, 6.2831855f);

        if (w.nextDirChangeSec < 0.0 || m_elapsedTime >= w.nextDirChangeSec)
        {
            w.nextDirChangeSec = m_elapsedTime + nextGap(m_rng);
            const float a = angle(m_rng);
            w.vx = std::cos(a) * m_enemyWanderSpeed;
            w.vz = std::sin(a) * m_enemyWanderSpeed;
        }

        // Keep the crowd near the origin so they do not drift forever.
        if (std::abs(pos.x) > m_enemyWanderArenaHalf || std::abs(pos.z) > m_enemyWanderArenaHalf)
        {
            float dx = -pos.x;
            float dz = -pos.z;
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len > 1e-4f)
            {
                dx /= len;
                dz /= len;
                w.vx = dx * m_enemyWanderSpeed;
                w.vz = dz * m_enemyWanderSpeed;
                w.nextDirChangeSec = m_elapsedTime + nextGap(m_rng);
            }
        }

        vel.x = w.vx;
        vel.z = w.vz;
    }

    bool isEnemyEntity(Entity e) const
    {
        for (Entity en : m_enemies)
        {
            if (en == e)
            {
                return true;
            }
        }
        return false;
    }

    int enemyIndex1Based(Entity e) const
    {
        for (std::size_t i = 0; i < m_enemies.size(); ++i)
        {
            if (m_enemies[i] == e)
            {
                return static_cast<int>(i) + 1;
            }
        }
        return -1;
    }

    void createPlayerFacingRay()
    {
        // Chest-height socket on the player; world pose updated by SocketSystem from player WorldTransform.
        m_playerRaySocket = m_registry.createEntity();
        m_registry.addComponent(m_playerRaySocket, SocketComponent{
            m_player,
            {0.0f, m_collisionHalfY * 0.35f, 0.0f},
            {},
            {}
        });

        m_facingRayEntity = m_registry.createEntity();
        m_registry.addComponent(m_facingRayEntity, TransformComponent{});
        m_registry.addComponent(m_facingRayEntity, AttachComponent{
            m_player,
            m_playerRaySocket,
            {0.0f, 0.0f, 0.0f},
            {},
            true,
            false
        });
        m_registry.addComponent(m_facingRayEntity, RayComponent{});
        m_registry.addComponent(m_facingRayEntity, RaycastHitComponent{});
        FacingRayDriverComponent facingDrv{};
        facingDrv.cameraEntity        = m_camera;
        facingDrv.rotationAlignEntity = m_player;
        facingDrv.ignoreEntity        = m_player;
        facingDrv.maxDistance         = m_rayMaxDistance;
        facingDrv.layerMask           = kFacingRayEnemyLayerMask;
        facingDrv.sweepRadius         = m_raySweepRadius;
        m_registry.addComponent(m_facingRayEntity, facingDrv);
    }

    void updateCameraInputs(double dt)
    {
        auto& cam = m_registry.getComponent<CameraComponent>(m_camera);

        // Demo orbit motion: automatically rotate slowly around the player.
        // Replace this later with actual mouse delta from browser/native input.
        if (cam.enableOrbit && !cam.enableLockOn)
        {
            cam.inputDeltaX = m_demoOrbitSpeed;
            cam.inputDeltaY = 0.0f;
        }
        else
        {
            cam.inputDeltaX = 0.0f;
            cam.inputDeltaY = 0.0f;
        }

        // Demo lock-on behavior:
        // every few seconds, toggle lock-on to first enemy and back off.
        if (!m_enemies.empty())
        {
            const int cycle = static_cast<int>(m_elapsedTime) % 12;

            if (cycle >= 6 && cycle < 10)
            {
                cam.enableLockOn = true;
                cam.lockOnTarget = m_enemies[0];
            }
            else
            {
                cam.enableLockOn = false;
                cam.lockOnTarget = INVALID_ENTITY;
                cam.lookAtTarget = m_player;
            }
        }

        (void)dt;
    }

    // -------------------------------------------------
    // Render / Debug
    // -------------------------------------------------

    void printTerminalStatusTable()
    {
        std::cout << "\033[2J\033[H";

        auto entityLabel = [this](Entity e) -> std::string {
            if (e == m_player)
                return "Player";
            if (e == m_camera)
                return "Camera";
            if (e == m_handSocketEntity)
                return "Socket(hand)";
            if (e == m_handAttachedEnemy)
                return "Enemy@hand";
            if (e == m_sunLightEntity)
                return "Light(sun)";
            if (e == m_ambientLightEntity)
                return "Light(ambient)";
            if (e == m_handLightEntity)
                return "Light(hand)";
            for (std::size_t i = 0; i < m_enemies.size(); ++i)
            {
                if (m_enemies[i] == e)
                    return std::string("Enemy ") + std::to_string(i + 1);
            }
            return std::string("e") + std::to_string(static_cast<unsigned int>(e));
        };

        auto vec3Cell = [](const Vec3& p) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(2) << p.x << ", " << p.y << ", " << p.z;
            return o.str();
        };

        auto positionFor = [this](Entity e) -> Vec3 {
            if (m_registry.hasComponent<Position>(e))
            {
                const auto& pos = m_registry.getComponent<Position>(e);
                return {pos.x, pos.y, pos.z};
            }
            if (m_registry.hasComponent<TransformComponent>(e))
                return m_registry.getComponent<TransformComponent>(e).position;
            if (m_registry.hasComponent<WorldTransformComponent>(e))
            {
                const Mat4& w = m_registry.getComponent<WorldTransformComponent>(e).world;
                return {w.m[12], w.m[13], w.m[14]};
            }
            return {0.0f, 0.0f, 0.0f};
        };

        auto collisionCell = [this, &entityLabel](Entity e) -> std::string {
            if (!m_registry.hasComponent<CollisionBoxComponent>(e))
                return "-";
            const auto& box = m_registry.getComponent<CollisionBoxComponent>(e);
            if (box.touching.empty())
                return "clear";
            std::ostringstream o;
            o << "touch " << box.touching.size() << " [";
            const std::size_t maxShow = 4;
            for (std::size_t i = 0; i < box.touching.size() && i < maxShow; ++i)
            {
                if (i)
                    o << ",";
                o << entityLabel(box.touching[i]);
            }
            if (box.touching.size() > maxShow)
                o << ",…";
            o << "]";
            return o.str();
        };

        auto stateCell = [this](Entity e) -> std::string {
            if (!m_registry.hasComponent<StateMachineComponent>(e))
                return "-";
            return m_registry.getComponent<StateMachineComponent>(e).machine.getCurrentState();
        };

        const RaycastHitComponent* hitComp =
            (m_facingRayEntity != INVALID_ENTITY && m_registry.hasComponent<RaycastHitComponent>(m_facingRayEntity))
                ? &m_registry.getComponent<RaycastHitComponent>(m_facingRayEntity)
                : nullptr;

        auto rayHitCell = [this, hitComp, &entityLabel](Entity e) -> std::string {
            if (!hitComp || !hitComp->hit)
                return "-";
            if (hitComp->entity != e)
                return "-";
            std::ostringstream o;
            o << std::fixed << std::setprecision(2) << "HIT d=" << hitComp->distance;
            return o.str();
        };

        constexpr int Wname = 16;
        constexpr int Wpos  = 26;
        constexpr int Wcol  = 32;
        constexpr int Wst   = 14;
        constexpr int Wray  = 16;

        std::cout << "\033[1;36m" << std::string(110, '=') << "\033[0m\n";
        std::cout << " ThirdPersonCameraDemo   t=" << std::fixed << std::setprecision(2) << std::setw(8)
                  << m_elapsedTime << " s"
                  << "   player–enemy collision enters: " << m_playerEnemyCollisionEnters << "\n";
        std::cout << "\033[1;36m" << std::string(110, '=') << "\033[0m\n";

        std::cout << " " << std::left << std::setw(Wname) << "Entity" << std::setw(Wpos) << "Position (x,y,z)"
                  << std::setw(Wcol) << "Collision" << std::setw(Wst) << "State" << std::setw(Wray) << "Ray hit"
                  << "\n";
        std::cout << " " << std::string(110, '-') << "\n";

        auto printRow = [&](Entity e) {
            std::cout << " " << std::left << std::setw(Wname) << entityLabel(e) << std::setw(Wpos)
                      << vec3Cell(positionFor(e)) << std::setw(Wcol) << collisionCell(e) << std::setw(Wst)
                      << stateCell(e) << std::setw(Wray) << rayHitCell(e) << "\n";
        };

        printRow(m_player);
        constexpr std::size_t kMaxEnemyRowsInTable = 14;
        std::size_t           enemyRow = 0;
        for (Entity en : m_enemies)
        {
            if (enemyRow < kMaxEnemyRowsInTable)
                printRow(en);
            ++enemyRow;
        }
        if (m_enemies.size() > kMaxEnemyRowsInTable)
        {
            std::cout << " " << std::left << std::setw(Wname) << ("… +" + std::to_string(m_enemies.size() - kMaxEnemyRowsInTable) + " enemies")
                      << "\n";
        }
        printRow(m_camera);
        if (m_handAttachedEnemy != INVALID_ENTITY)
            printRow(m_handAttachedEnemy);

        std::cout << " " << std::string(110, '-') << "\n";
        std::cout << "\033[1;36mFacing ray (gameplay)\033[0m\n";

        if (m_facingRayEntity != INVALID_ENTITY && m_registry.hasComponent<RayComponent>(m_facingRayEntity))
        {
            const auto& ray = m_registry.getComponent<RayComponent>(m_facingRayEntity);
            const Vec3& o   = ray.origin;
            const Vec3& d   = ray.direction;
            std::cout << "  origin: " << std::fixed << std::setprecision(2) << o.x << ", " << o.y << ", " << o.z
                      << "   dir: " << d.x << ", " << d.y << ", " << d.z << "\n";
            std::cout << "  maxDist: " << ray.maxDistance << "  radius: " << ray.radius
                      << "  layerMask: 0x" << std::hex << ray.layerMask << std::dec
                      << "  ignore: " << entityLabel(ray.ignoreEntity) << "\n";
        }

        if (hitComp)
        {
            std::cout << "  result: ";
            if (!hitComp->hit)
            {
                std::cout << "no hit\n";
            }
            else
            {
                const Vec3& hp = hitComp->point;
                std::cout << "hit entity " << entityLabel(hitComp->entity) << "  dist " << std::fixed
                          << std::setprecision(2) << hitComp->distance << "  point " << hp.x << ", " << hp.y << ", "
                          << hp.z << "\n";
            }
        }
        else
        {
            std::cout << "  result: (no ray component)\n";
        }

        std::cout << std::defaultfloat;
    }

    void printCollisionHeader()
    {
        bool playerEnemyOverlap = false;
        {
            const auto& pbox = m_registry.getComponent<CollisionBoxComponent>(m_player);
            for (Entity other : pbox.touching)
            {
                if (isEnemyEntity(other))
                {
                    playerEnemyOverlap = true;
                    break;
                }
            }
        }

        std::cout << "\033[1;36mCollision\033[0m\n";
        std::cout << "  AABB half: ("
                  << m_collisionHalfX << ", "
                  << m_collisionHalfY << ", "
                  << m_collisionHalfZ << ")  grid cell: " << m_spatialGrid.cellSize << "\n";
        std::cout << "  Player–enemy enters: " << m_playerEnemyCollisionEnters
                  << "  overlapping: " << (playerEnemyOverlap ? "yes" : "no") << "\n";
        std::cout << "  Facing ray layers: mask 0x" << std::hex << kFacingRayEnemyLayerMask << std::dec
                  << " (enemies 2–3 only)  sweep r=" << m_raySweepRadius << "\n";

        {
            const auto& rayHit = m_registry.getComponent<RaycastHitComponent>(m_facingRayEntity);
            std::cout << "  Facing ray: ";
            if (rayHit.hit && isEnemyEntity(rayHit.entity))
            {
                const int slot = enemyIndex1Based(rayHit.entity);
                std::cout << "enemy #" << slot << "  dist " << std::fixed << std::setprecision(2)
                          << rayHit.distance << std::defaultfloat << "\n";
            }
            else
            {
                std::cout << "no enemy in view\n";
            }
        }

        std::cout << "\n";
    }

    void printActors()
    {
        static constexpr float kPi = 3.14159265f;

        std::cout << std::left << "Camera view (3D persp.)   Legend:  P player   1-9 enemy   @hand tiny enemy on socket\n";
        std::cout << "\n";

        const auto& camComp = m_registry.getComponent<CameraComponent>(m_camera);
        const auto& camTf   = m_registry.getComponent<TransformComponent>(m_camera);
        const Vec3          camPos = camTf.position;

        Entity lookTarget = m_player;
        if (camComp.enableLockOn && camComp.lockOnTarget != INVALID_ENTITY)
        {
            lookTarget = camComp.lockOnTarget;
        }
        else if (camComp.enableLookAt && camComp.lookAtTarget != INVALID_ENTITY)
        {
            lookTarget = camComp.lookAtTarget;
        }

        const auto& targetTf = m_registry.getComponent<TransformComponent>(lookTarget);
        const float dx =
            (targetTf.position.x + camComp.lookAtOffset.x) - camPos.x;
        const float dy =
            (targetTf.position.y + camComp.lookAtOffset.y) - camPos.y;
        const float dz =
            (targetTf.position.z + camComp.lookAtOffset.z) - camPos.z;

        const float len3 = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float fwdX = (len3 > 1.0e-5f) ? dx / len3 : 0.0f;
        const float fwdY = (len3 > 1.0e-5f) ? dy / len3 : 1.0f;
        const float fwdZ = (len3 > 1.0e-5f) ? dz / len3 : 0.0f;

        const float yaw =
            std::atan2(dx, dz);
        const float horizontalDist = std::sqrt(dx * dx + dz * dz);
        const float pitch =
            std::atan2(dy, std::max(horizontalDist, 1.0e-5f));

        float rgtX = fwdZ;
        float rgtY = 0.0f;
        float rgtZ = -fwdX;
        float rLen = std::sqrt(rgtX * rgtX + rgtZ * rgtZ);
        if (rLen > 1.0e-5f)
        {
            rgtX /= rLen;
            rgtZ /= rLen;
        }
        else
        {
            rgtX = 1.0f;
            rgtZ = 0.0f;
        }

        float upX = fwdY * rgtZ - fwdZ * rgtY;
        float upY = fwdZ * rgtX - fwdX * rgtZ;
        float upZ = fwdX * rgtY - fwdY * rgtX;
        const float uLen = std::sqrt(upX * upX + upY * upY + upZ * upZ);
        if (uLen > 1.0e-5f)
        {
            upX /= uLen;
            upY /= uLen;
            upZ /= uLen;
        }

        constexpr int kCols = 40;
        constexpr int kRows = 12;

        const float fovYRad  = camComp.fov * kPi / 180.0f;
        const float tanHalfY = std::tan(fovYRad * 0.5f);
        const float tanHalfX = tanHalfY * (static_cast<float>(kCols) / static_cast<float>(kRows));

        struct MapPoint
        {
            float x;
            float y;
            float z;
            char  sym;
        };
        std::vector<MapPoint> mapPoints;

        auto actors = m_registry.getEntitiesWith<Position, StateMachineComponent>();

        for (auto e : actors)
        {
            auto& pos = m_registry.getComponent<Position>(e);
            auto& sm  = m_registry.getComponent<StateMachineComponent>(e).machine;

            const std::string type = (e == m_player) ? "Player" : "Enemy";

            if (e == m_player)
            {
                mapPoints.push_back({pos.x, pos.y, pos.z, 'P'});
            }
            else
            {
                int enemyIndex = 0;
                for (; enemyIndex < static_cast<int>(m_enemies.size()); ++enemyIndex)
                {
                    if (m_enemies[static_cast<std::size_t>(enemyIndex)] == e)
                    {
                        break;
                    }
                }
                const char sym =
                    (enemyIndex >= 0 && enemyIndex < 9)
                        ? static_cast<char>('1' + enemyIndex)
                        : 'E';
                mapPoints.push_back({pos.x, pos.y, pos.z, sym});
            }

            (void)sm;
            (void)type;
        }

        const float cellNx = 2.0f / static_cast<float>(kCols);
        const float cellNy = 2.0f / static_cast<float>(kRows);
        const float thresh2 = (cellNx * cellNx + cellNy * cellNy) * 6.25f;

        std::cout << "  yaw(deg) " << std::setw(8) << (yaw * 180.0f / kPi) << "  pitch(deg) " << std::setw(8)
                  << (pitch * 180.0f / kPi) << "  fov " << std::setw(6) << camComp.fov << "  eye "
                  << camPos.x << " " << camPos.y << " " << camPos.z << "\n";

        for (int iz = 0; iz < kRows; ++iz)
        {
            std::cout << "  ";
            for (int ix = 0; ix < kCols; ++ix)
            {
                const float nxC =
                    (static_cast<float>(ix) + 0.5f) / static_cast<float>(kCols) * 2.0f - 1.0f;
                const float nyC =
                    1.0f - (static_cast<float>(iz) + 0.5f) / static_cast<float>(kRows) * 2.0f;

                float bestD2    = thresh2;
                float bestDepth = 1.0e30f;
                char  cell      = '.';

                for (const MapPoint& p : mapPoints)
                {
                    const float vx = p.x - camPos.x;
                    const float vy = p.y - camPos.y;
                    const float vz = p.z - camPos.z;

                    const float depth = vx * fwdX + vy * fwdY + vz * fwdZ;
                    if (depth <= 0.05f)
                    {
                        continue;
                    }

                    const float rx = vx * rgtX + vy * rgtY + vz * rgtZ;
                    const float uy = vx * upX + vy * upY + vz * upZ;

                    const float nx = (rx / depth) / tanHalfX;
                    const float ny = (uy / depth) / tanHalfY;

                    const float d2 = (nx - nxC) * (nx - nxC) + (ny - nyC) * (ny - nyC);
                    if (d2 < bestD2 - 1.0e-6f)
                    {
                        bestD2    = d2;
                        bestDepth = depth;
                        cell      = p.sym;
                    }
                    else if (std::abs(d2 - bestD2) <= 1.0e-6f && depth < bestDepth)
                    {
                        bestDepth = depth;
                        cell      = p.sym;
                    }
                }

                std::cout << cell;
            }
            std::cout << "\n";
        }
    }

    void printActorStateTable()
    {
        std::cout << "\n\033[1;36mActor states\033[0m\n";
        std::cout << "  " << std::left << std::setw(14) << "Actor" << "State\n";
        std::cout << "  " << std::string(28, '-') << "\n";

        auto& playerSm = m_registry.getComponent<StateMachineComponent>(m_player).machine;
        std::cout << "  " << std::setw(14) << "Player" << playerSm.getCurrentState() << "\n";

        for (std::size_t i = 0; i < m_enemies.size(); ++i)
        {
            const Entity      e   = m_enemies[i];
            auto&             sm  = m_registry.getComponent<StateMachineComponent>(e).machine;
            const std::string row = std::string("Enemy ") + std::to_string(i + 1);
            std::cout << "  " << std::setw(14) << row << sm.getCurrentState() << "\n";
        }
    }

    void printCameraDebug()
    {
        auto& camComponent = m_registry.getComponent<CameraComponent>(m_camera);
        auto& camTransform = m_registry.getComponent<TransformComponent>(m_camera);
        auto& socket = m_registry.getComponent<SocketComponent>(m_cameraSocket);
        auto& playerPos = m_registry.getComponent<Position>(m_player);

        std::cout << "\nPlayer Pos: "
                  << playerPos.x << ", "
                  << playerPos.y << ", "
                  << playerPos.z << "\n";

        std::cout << "Socket: "
                  << socket.worldTransform.m[12] << ", "
                  << socket.worldTransform.m[13] << ", "
                  << socket.worldTransform.m[14] << "\n";

        std::cout << "Camera Pos: "
                  << camTransform.position.x << ", "
                  << camTransform.position.y << ", "
                  << camTransform.position.z << "\n";

        std::cout << "Camera Orbit Yaw/Pitch: "
                  << camComponent.orbitYaw << " / "
                  << camComponent.orbitPitch << "\n";

        std::cout << "Camera Mode: "
                  << (camComponent.enableLockOn ? "LockOn" : "Orbit/Follow")
                  << "\n";

        if (camComponent.enableLockOn)
        {
            std::cout << "Lock Target: " << camComponent.lockOnTarget << "\n";
        }

        std::cout << "Time: " << m_elapsedTime << "\n";
    }

    void printInstructions()
    {
        std::cout << "Controls: WASD (A/D strafe flipped) camera-relative | Mouse look yaw/pitch | Space jump | Esc/Q quit\n";
    }

    /// Full-body skeleton sync + clip indices: embedded idle, then custom hand/forearm clips.
    void attachPlayerSkeletonAnimation()
    {
        if (!m_playerAvatarModel || m_playerAvatarModel->skeleton.bones.empty())
            return;

        m_clipIdle = findIdleClipIndex(*m_playerAvatarModel);
        m_clipMove = appendLeftHandTwistClip(*m_playerAvatarModel);
        m_clipSlow = appendLeftForeArmBobClip(*m_playerAvatarModel);
        if (m_clipMove < 0)
            m_clipMove = m_clipIdle;
        if (m_clipSlow < 0)
            m_clipSlow = m_clipIdle;

        SkeletonInstanceComponent skel{};
        skel.model = m_playerAvatarModel;
        skel.syncBoneIndices.clear();
        skel.syncBoneIndices.reserve(m_playerAvatarModel->skeleton.bones.size());
        for (size_t i = 0; i < m_playerAvatarModel->skeleton.bones.size(); ++i)
            skel.syncBoneIndices.push_back(static_cast<int>(i));

        m_registry.addComponent(m_player, std::move(skel));
        m_registry.addComponent(m_player, SkeletonPoseComponent{});

        AnimationPlaybackComponent play{};
        play.primaryClip = m_clipIdle >= 0 ? m_clipIdle : -1;
        play.secondaryClip = -1;
        play.speedPrimary = 1.f;
        play.speedSecondary = 0.f;
        play.blendAlpha = 0.f;
        play.loopPrimary = true;
        play.loopSecondary = true;
        play.invalidatePoseCache = true;
        m_registry.addComponent(m_player, std::move(play));

        m_animCrossfadeActive = false;

        if (m_clipIdle >= 0)
            std::cout << "Locomotion clips: idle=\"" << m_playerAvatarModel->clips[static_cast<size_t>(m_clipIdle)].name
                      << "\" [" << m_clipIdle << "]  move(custom hand)=" << m_clipMove << "  slow(custom forearm)="
                      << m_clipSlow << "\n";
    }

    /// Socket on `LeftHand_23` (tracks bone via SkeletonPose + SocketSystem). Pyramid = debug draw flag on socket.
    ///
    /// How to attach props: (1) Create an entity with SocketComponent; set `skeletonRoot` = player entity,
    /// `followBoneIndex` = bone index from ModelAsset::skeleton.boneMap, `localOffset`/`localRotation` for grip
    /// offset. (2) Run SocketSystem after TransformSystem so `worldTransform` updates. (3) Child entities use
    /// AttachComponent with `socketEntity` = this socket; position follows `SocketComponent.worldTransform`.
    void createHandSocketAndAttachedEnemy()
    {
        if (!m_playerAvatarModel)
            return;
        const auto it = m_playerAvatarModel->skeleton.boneMap.find("LeftHand_23");
        if (it == m_playerAvatarModel->skeleton.boneMap.end()) {
            std::cerr << "LeftHand_23 not in boneMap; hand socket skipped.\n";
            return;
        }
        const int handBi = it->second;

        m_handSocketEntity = m_registry.createEntity();
        SocketComponent sock{};
        sock.parentEntity = INVALID_ENTITY;
        sock.localOffset = {0.0f, 0.0f, 0.0f};
        sock.localRotation = {};
        sock.worldTransform = Mat4::Identity();
        sock.skeletonRoot = m_player;
        sock.followBoneIndex = handBi;
        sock.debugDrawPyramid = true;
        m_registry.addComponent(m_handSocketEntity, sock);

        m_handAttachedEnemy = m_registry.createEntity();
        m_registry.addComponent(m_handAttachedEnemy, TransformComponent{});
        auto& attTf = m_registry.getComponent<TransformComponent>(m_handAttachedEnemy);
        attTf.scale = {0.28f, 0.28f, 0.28f};
        m_registry.addComponent(m_handAttachedEnemy, WorldTransformComponent{});
        m_registry.addComponent(m_handAttachedEnemy, EnemyTagComponent{999u});
        m_registry.addComponent(m_handAttachedEnemy, AttachComponent{
            m_player,
            m_handSocketEntity,
            {0.0f, 0.0f, 0.0f},
            {},
            true,
            false});
    }

    /// ECS lights consumed by `OpenGLVer2Renderer::applyTexturedSceneLighting` (fragment shader only).
    void createLightingEntities()
    {
        m_sunLightEntity = m_registry.createEntity();
        m_registry.addComponent(m_sunLightEntity, TransformComponent{});
        m_registry.addComponent(m_sunLightEntity, WorldTransformComponent{});
        {
            LightingComponent sun{};
            sun.type              = LightType::Directional;
            sun.useEntityAxis     = false;
            sun.worldDirectionOverride = normalize(Vec3{0.42f, -0.84f, 0.34f});
            sun.color             = {1.0f, 0.96f, 0.88f};
            sun.intensity         = 0.95f;
            sun.specularPower     = 56.0f;
            sun.specularIntensity = 0.28f;
            sun.useHalfLambert    = true;
            m_registry.addComponent(m_sunLightEntity, sun);
        }

        m_ambientLightEntity = m_registry.createEntity();
        {
            LightingComponent amb{};
            amb.type      = LightType::Ambient;
            amb.color     = {0.11f, 0.13f, 0.17f};
            amb.intensity = 1.0f;
            m_registry.addComponent(m_ambientLightEntity, amb);
        }

        if (m_handSocketEntity == INVALID_ENTITY)
            return;

        m_handLightEntity = m_registry.createEntity();
        m_registry.addComponent(m_handLightEntity, TransformComponent{});
        m_registry.addComponent(m_handLightEntity, WorldTransformComponent{});
        {
            LightingComponent hl{};
            hl.type               = LightType::Spot;
            hl.color              = {1.0f, 0.78f, 0.48f};
            hl.intensity          = 4.2f;
            hl.range              = 20.0f;
            hl.attenLinear        = 0.12f;
            hl.attenQuadratic     = 0.28f;
            hl.spotInnerDegrees   = 20.0f;
            hl.spotOuterDegrees   = 38.0f;
            hl.specularPower      = 40.0f;
            hl.specularIntensity  = 0.65f;
            hl.rimIntensity       = 0.14f;
            hl.rimPower           = 3.2f;
            hl.useHalfLambert     = true;
            m_registry.addComponent(m_handLightEntity, hl);
        }
        m_registry.addComponent(m_handLightEntity, AttachComponent{
            m_player,
            m_handSocketEntity,
            {0.0f, 0.0f, 0.0f},
            {},
            true,
            false});
    }

    /// Single active HDRI for `OpenGLVer2Renderer` (first enabled `HdriEnvironmentComponent` wins).
    void createHdriEnvironmentEntity()
    {
        m_hdriEnvEntity = m_registry.createEntity();
        HdriEnvironmentComponent h{};
#ifdef GAME_ENGINE_PROJECT_ROOT
        h.hdriAssetPath = std::string(GAME_ENGINE_PROJECT_ROOT) + "/assets/lighting/hdri.webp";
#else
        h.hdriAssetPath = "assets/lighting/hdri.webp";
#endif
        m_registry.addComponent(m_hdriEnvEntity, h);
    }

    void createPlayerSlowingSfxEntity()
    {
        m_slowingSfxEntity = m_registry.createEntity();
        AudioComponent a{};
#ifdef GAME_ENGINE_PROJECT_ROOT
        a.clipPath = std::string(GAME_ENGINE_PROJECT_ROOT) + "/assets/audio/correct.mp3";
#else
        a.clipPath = "assets/audio/correct.mp3";
#endif
        a.volume     = 0.9f;
        a.loop       = false;
        a.playing    = false;
        a.paused     = false;
        m_registry.addComponent(m_slowingSfxEntity, a);
    }

    /// Fire one-shot SFX when the player FSM enters `Slowing` (see AudioComponent + AudioSystem).
    void syncPlayerSlowingSfx()
    {
        if (m_slowingSfxEntity == INVALID_ENTITY || !m_registry.hasComponent<StateMachineComponent>(m_player))
            return;
        auto& sm                        = m_registry.getComponent<StateMachineComponent>(m_player).machine;
        const std::string cur             = sm.getCurrentState();
        if (cur == "Slowing" && m_prevPlayerFsmStateForSfx != "Slowing")
        {
            auto& sfx               = m_registry.getComponent<AudioComponent>(m_slowingSfxEntity);
            sfx.playing             = true;
            sfx.paused              = false;
        }
        m_prevPlayerFsmStateForSfx = cur;
    }

    // -------------------------------------------------
    // State Machines
    // -------------------------------------------------

    void setupPlayerStateMachine(Entity e)
    {
        StateMachineConfig config;
        config.initialState = "Idle";

        config.states["Idle"] = {
            "Idle",
            nullptr,
            nullptr,
            nullptr,
            {
                {StateEventType::MoveUp,    "Moving", nullptr},
                {StateEventType::MoveDown,  "Moving", nullptr},
                {StateEventType::MoveLeft,  "Moving", nullptr},
                {StateEventType::MoveRight, "Moving", nullptr}
            }
        };

        config.states["Moving"] = {
            "Moving",
            nullptr,
            nullptr,
            nullptr,
            {
                {
                    StateEventType::Stop,
                    "Slowing",
                    [&](Entity entity)
                    {
                        auto& machine = m_registry.getComponent<StateMachineComponent>(entity).machine;
                        return machine.getTimeInState() >= m_minMovingTime;
                    }
                },
                {
                    StateEventType::MoveUp,
                    "Moving",
                    [&](Entity entity)
                    {
                        auto& machine = m_registry.getComponent<StateMachineComponent>(entity).machine;
                        return machine.getTimeInState() >= m_moveRefreshTime;
                    }
                },
                {
                    StateEventType::MoveDown,
                    "Moving",
                    [&](Entity entity)
                    {
                        auto& machine = m_registry.getComponent<StateMachineComponent>(entity).machine;
                        return machine.getTimeInState() >= m_moveRefreshTime;
                    }
                },
                {
                    StateEventType::MoveLeft,
                    "Moving",
                    [&](Entity entity)
                    {
                        auto& machine = m_registry.getComponent<StateMachineComponent>(entity).machine;
                        return machine.getTimeInState() >= m_moveRefreshTime;
                    }
                },
                {
                    StateEventType::MoveRight,
                    "Moving",
                    [&](Entity entity)
                    {
                        auto& machine = m_registry.getComponent<StateMachineComponent>(entity).machine;
                        return machine.getTimeInState() >= m_moveRefreshTime;
                    }
                }
            }
        };

        config.states["Slowing"] = {
            "Slowing",
            nullptr,
            nullptr,
            nullptr,
            {
                {StateEventType::MoveUp,    "Moving", nullptr},
                {StateEventType::MoveDown,  "Moving", nullptr},
                {StateEventType::MoveLeft,  "Moving", nullptr},
                {StateEventType::MoveRight, "Moving", nullptr},
                {
                    StateEventType::Stop,
                    "Idle",
                    [&](Entity entity)
                    {
                        auto& machine = m_registry.getComponent<StateMachineComponent>(entity).machine;
                        return machine.getTimeInState() >= m_minSlowingTime;
                    }
                }
            }
        };

        m_registry.addComponent(e, StateMachineComponent{});
        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
        sm.initialize(e, config);
    }

    void setupEnemyStateMachine(Entity e)
    {
        StateMachineConfig config;
        config.initialState = "Idle";

        config.states["Idle"] = {
            "Idle",
            nullptr,
            nullptr,
            nullptr,
            {
                {StateEventType::UserDetected, "Flee", nullptr}
            }
        };

        config.states["Flee"] = {
            "Flee",
            [&](Entity enemy)
            {
                m_registry.getComponent<Velocity>(enemy).x = -2.0f;
            },
            nullptr,
            nullptr,
            {}
        };

        // Run toward the player each tick; entered when the facing ray hits this enemy.
        config.states["Chase"] = {
            "Chase",
            nullptr,
            [this](Entity enemy, double dt)
            {
                (void)dt;
                if (!m_registry.hasComponent<Velocity>(enemy) || !m_registry.hasComponent<Position>(enemy))
                    return;
                if (!m_registry.hasComponent<Position>(m_player))
                    return;

                auto&       vel = m_registry.getComponent<Velocity>(enemy);
                const auto& pos = m_registry.getComponent<Position>(enemy);
                const auto& ppos = m_registry.getComponent<Position>(m_player);

                float dx = ppos.x - pos.x;
                float dz = ppos.z - pos.z;
                const float lenSq = dx * dx + dz * dz;
                if (lenSq > 0.02f)
                {
                    const float inv = 1.0f / std::sqrt(lenSq);
                    dx *= inv;
                    dz *= inv;
                    vel.x = dx * m_chaseSpeed;
                    vel.z = dz * m_chaseSpeed;
                }
                else
                {
                    vel.x = 0.0f;
                    vel.z = 0.0f;
                }
            },
            nullptr,
            {}
        };

        // Scripted “dance”: fast Y spin + slight pitch wobble (see taunt sequence).
        config.states["Taunt"] = {
            "Taunt",
            [this](Entity enemy)
            {
                if (!m_registry.hasComponent<Velocity>(enemy))
                    return;
                auto& v       = m_registry.getComponent<Velocity>(enemy);
                v.x = v.y = v.z = 0.0f;
            },
            [this](Entity enemy, double dt)
            {
                (void)dt;
                if (!m_registry.hasComponent<Velocity>(enemy) || !m_registry.hasComponent<TransformComponent>(enemy) ||
                    !m_registry.hasComponent<EnemyTagComponent>(enemy))
                    return;
                auto& v = m_registry.getComponent<Velocity>(enemy);
                v.x     = 0.0f;
                v.z     = 0.0f;

                const std::uint32_t slot = m_registry.getComponent<EnemyTagComponent>(enemy).slot;
                const float         phase = static_cast<float>(slot) * 0.37f;
                const float         t     = static_cast<float>(m_elapsedTime) * 5.0f + phase;
                const float spin          = t * 2.8f;
                const float wobble        = std::sin(t * 2.1f) * 0.22f;

                auto&       tf = m_registry.getComponent<TransformComponent>(enemy);
                const Quat  qY = quatNormalize(quatFromAxisAngleRad({0.0f, 1.0f, 0.0f}, spin));
                const Quat  qX = quatNormalize(quatFromAxisAngleRad({1.0f, 0.0f, 0.0f}, wobble));
                tf.rotation    = quatNormalize(quatMul(qY, qX));
            },
            nullptr,
            {}
        };

        m_registry.addComponent(e, StateMachineComponent{});
        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
        sm.initialize(e, config);
    }

    /// If the gameplay facing ray hits an enemy, they transition to Chase.
    void syncEnemyAggroFromFacingRay()
    {
        if (m_facingRayEntity == INVALID_ENTITY || !m_registry.hasComponent<RaycastHitComponent>(m_facingRayEntity))
            return;

        const auto& hit = m_registry.getComponent<RaycastHitComponent>(m_facingRayEntity);
        if (!hit.hit || hit.entity == INVALID_ENTITY)
            return;
        if (!isEnemyEntity(hit.entity))
            return;
        if (!m_registry.hasComponent<StateMachineComponent>(hit.entity))
            return;

        auto& sm = m_registry.getComponent<StateMachineComponent>(hit.entity).machine;
        if (sm.getCurrentState() == "Chase" || sm.getCurrentState() == "Taunt")
            return;

        sm.forceTransitionTo("Chase");
    }

private:
    Registry m_registry;

    std::unique_ptr<Control> m_control;

    AssetManager m_assetManager;
    ThreadService m_threadService;
    StreamingLoadService m_streamingLoadService{m_assetManager, &m_threadService};
    StreamingAssetCache m_streamingCache{m_assetManager};

    TransformSystem m_transformSystem;
    PositionToTransformSystem       m_positionToTransformSystem;
    CollisionLastPositionSyncSystem m_collisionLastPositionSyncSystem;
    FacingRaySystem                 m_facingRaySystem;
    AnimationSystem m_animationSystem;
    BoneSyncSystem m_boneSyncSystem;
    SocketSystem m_socketSystem;
    AttachmentSystem m_attachmentSystem;
    CameraSystem               m_cameraSystem;
    CameraGroundClampSystem    m_cameraGroundClampSystem;
    SpatialGridSystem m_spatialGrid;
    RaycastSystem     m_raycastSystem{m_spatialGrid};
    CollisionSystem              m_collisionSystem;
    SolidCollisionResponseSystem m_solidCollisionResponse;

    TerrainEnvironmentSystem m_terrainEnv;
    Entity                   m_terrainSettingsEntity{INVALID_ENTITY};

    Entity m_player{};

    std::shared_ptr<ModelAsset> m_playerAvatarModel;
    std::string m_playerAvatarPath;

    int  m_clipIdle = -1;
    int  m_clipMove = -1;
    int  m_clipSlow = -1;
    bool m_animCrossfadeActive = false;

    Entity m_handSocketEntity{INVALID_ENTITY};
    Entity m_handAttachedEnemy{INVALID_ENTITY};
    Entity m_sunLightEntity{INVALID_ENTITY};
    Entity m_ambientLightEntity{INVALID_ENTITY};
    Entity m_handLightEntity{INVALID_ENTITY};
    Entity m_hdriEnvEntity{INVALID_ENTITY};
    Entity m_slowingSfxEntity{INVALID_ENTITY};
    std::string m_prevPlayerFsmStateForSfx;
    Entity m_camera{};
    Entity m_cameraSocket{};
    Entity m_playerRaySocket{INVALID_ENTITY};
    Entity m_facingRayEntity{INVALID_ENTITY};

    std::vector<Entity> m_enemies;

    struct EnemyWanderState {
        double nextDirChangeSec = -1.0;
        float  vx               = 0.0f;
        float  vz               = 0.0f;
    };
    std::vector<EnemyWanderState> m_enemyWander;
    std::mt19937                    m_rng{std::random_device{}()};

    double m_elapsedTime = 0.0;
    double m_lastMoveInputTime = 0.0;
    bool m_shouldClose = false;

    const double m_inputGrace = 0.15;
    const double m_minMovingTime = 0.12;
    const double m_moveRefreshTime = 0.08;
    const double m_minSlowingTime = 0.18;

    const float m_slowingFactorPer60Fps = 0.92f;
    const float m_velocityDeadZone = 0.015f;

    const float m_demoOrbitSpeed = 0.35f;

    const float m_floorY  = 0.0f;
    const float m_gravity = -28.0f;
    /// Applied by gameplay when jump intent is active and the player is grounded (terrain / fallback).
    const float m_playerJumpSpeed = 9.0f;

    /// Horizontal speed for enemy random walk (units / second).
    const float m_enemyWanderSpeed     = 0.42f;
    const float m_enemyWanderArenaHalf = 32.0f;

    /// Run speed when an enemy is chasing the player (units / second).
    const float m_chaseSpeed = 3.2f;

    const float m_collisionHalfX = 0.4f;
    const float m_collisionHalfY = 0.55f;
    const float m_collisionHalfZ = 0.4f;

    std::uint64_t m_playerEnemyCollisionEnters = 0;

    /// Mouse delta → orbit input (used with centered cursor; window-coordinate deltas).
    float m_mouseSensitivity = 0.0048f;

    OpenGLVer2Renderer m_gl;
    AudioSystem m_audioSystem;

    SequenceSystem m_sequenceSystem;
    Entity         m_tauntSequenceEntity{INVALID_ENTITY};
    std::shared_ptr<Sequence> m_tauntSequence;
    bool           m_tauntSequenceCameraOverride{false};
    float          m_savedOrbitYaw{};
    float          m_savedOrbitPitch{};
    float          m_savedOrbitDistance{};
    float          m_savedCurrentYaw{};
    float          m_savedCurrentPitch{};
    Vec3           m_savedFollowVelocity{};
    bool           m_prevTauntKeyDown{false};

    const float m_rayMaxDistance = 40.0f;
    // Swept sphere radius for facing query (thick ray). 0 would be a line only.
    const float m_raySweepRadius = 0.55f;
};