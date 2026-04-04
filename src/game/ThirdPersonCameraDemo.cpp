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

#include "../systems/TransformSystem.hpp"
#include "../systems/SocketSystem.hpp"
#include "../systems/AttachmentSystem.hpp"
#include "../systems/CameraSystem.hpp"
#include "../systems/SpacialGridSystem.hpp"
#include "../systems/CollisionSystem.hpp"
#include "../systems/RaycastSystem.hpp"
#include "../systems/AnimationSystem.hpp"
#include "../systems/BoneSyncSystem.hpp"

#include "../core/assets/AssetManager.hpp"
#include "../core/assets/StreamingLoadService.hpp"
#include "../core/assets/StreamingAssetCache.hpp"
#include "../core/assets/importers/GltfModelImporter.hpp"
#include "../core/assets/AnimationClipData.hpp"
#include "../core/assets/ModelAsset.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

#include "../components/SkeletonInstanceComponent.hpp"
#include "../components/SkeletonPoseComponent.hpp"
#include "../components/AnimationPlaybackComponent.hpp"
#include "../components/BoneInstanceComponent.hpp"
#include "../components/StreamingAnchorComponent.hpp"
#include "../components/StreamableModelComponent.hpp"
#include "../components/MeshRenderProxyComponent.hpp"

#include "../rendering/OpenGLRenderSystem.hpp"

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

class ThirdPersonCameraDemo final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Third Person Camera Demo Started\n";

        registerComponents();

        m_assetManager.registerImporter("gltf", std::make_shared<GltfModelImporter>());
        m_assetManager.registerImporter("glb", std::make_shared<GltfModelImporter>());
        m_streamingLoadService.setTrackedBoneNames({"hand"});

        createPlayer();
        attachPlayerSkeletonAnimation();
        createEnemies();
        createCameraRig();

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

        syncPositionToTransform();
        syncCollisionLastPositions();

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
            m_control = std::make_unique<Control>(m_player, m_camera, 5.0f, 9.0f, true);
        }
        else
        {
            m_control = std::make_unique<Control>(m_player, m_camera, 5.0f, 9.0f, false);
            if (m_gl.window())
            {
                glfwSetInputMode(m_gl.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                int ww = 0, wh = 0;
                glfwGetWindowSize(m_gl.window(), &ww, &wh);
                if (ww > 0 && wh > 0)
                    glfwSetCursorPos(m_gl.window(), static_cast<double>(ww) * 0.5, static_cast<double>(wh) * 0.5);
            }
        }
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
        }
        else
        {
            m_control->handleInput(m_registry);
        }

        if (m_control->shouldClose() || m_gl.shouldClose())
            m_shouldClose = true;
    }

    void onUpdate(double dt) override
    {
        m_elapsedTime += dt;

        updateActors(dt);
        syncPositionToTransform();

        if (m_registry.hasComponent<TransformComponent>(m_player))
        {
            m_streamingCache.setViewerPosition(m_registry.getComponent<TransformComponent>(m_player).position);
        }

        // Order matters: streaming -> animation -> bone sync -> hierarchy transforms -> sockets
        m_streamingLoadService.update(m_registry);
        m_animationSystem.update(m_registry, static_cast<float>(dt));
        m_boneSyncSystem.update(m_registry);
        m_transformSystem.update(m_registry);
        m_socketSystem.update(m_registry);
        m_attachmentSystem.update(m_registry);
        m_cameraSystem.update(m_registry, static_cast<float>(dt));

        m_collisionSystem.update(m_registry, m_spatialGrid);

        updatePlayerFacingRay();
        m_raycastSystem.update(m_registry);
    }

    void onRender(double) override
    {
        printTerminalStatusTable();
        m_gl.renderFrame(m_registry, m_camera, m_player, m_enemies, m_playerAnimBone);
    }

    void onStop() override
    {
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
        m_registry.registerComponent<MeshRenderProxyComponent>();
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
            box.halfSize     = {m_collisionHalfX, m_collisionHalfY, m_collisionHalfZ};
            box.lastPosition = {0.0f, 0.0f, 0.0f};
            box.layer        = kLayerPlayer;
            m_registry.addComponent(m_player, box);
        }

        setupPlayerStateMachine(m_player);
    }

    void createEnemies()
    {
        for (int i = 0; i < 3; ++i)
        {
            Entity e = m_registry.createEntity();
            const float startX = float(i * 3 + 3);

            m_registry.addComponent(e, Position{startX, 0.0f, 0.0f});
            m_registry.addComponent(e, Velocity{0.0f, 0.0f, 0.0f});
            m_registry.addComponent(e, Health{100});
            m_registry.addComponent(e, TransformComponent{});
            m_registry.addComponent(e, WorldTransformComponent{});

            auto& t = m_registry.getComponent<TransformComponent>(e);
            t.position = {startX, 0.0f, 0.0f};

            {
                CollisionBoxComponent box{};
                box.halfSize     = {m_collisionHalfX, m_collisionHalfY, m_collisionHalfZ};
                box.lastPosition = {startX, 0.0f, 0.0f};
                // Bit flags: enemy slot i uses bit (i+1). Facing ray mask hits only bits 2 and 3 (enemies 2 & 3).
                box.layer = (1u << (i + 1));
                m_registry.addComponent(e, box);
            }

            setupEnemyStateMachine(e);
            m_enemies.push_back(e);
        }
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
        cam.orbitSensitivity = 0.32f;
        cam.minPitch = -0.6f;
        cam.maxPitch = 1.0f;

        // Lock-on disabled by default
        cam.enableLockOn = false;
        cam.lockOnTarget = INVALID_ENTITY;

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

    // -------------------------------------------------
    // Update
    // -------------------------------------------------

    void updateActors(double dt)
    {
        auto actors = m_registry.getEntitiesWith<Position, Velocity, StateMachineComponent>();

        for (auto e : actors)
        {
            auto& sm  = m_registry.getComponent<StateMachineComponent>(e).machine;
            auto& pos = m_registry.getComponent<Position>(e);
            auto& vel = m_registry.getComponent<Velocity>(e);

            if (e == m_player)
            {
                updatePlayerStateIntent(sm, vel);
            }

            sm.update(dt);
            applyStateBehavior(sm, vel, dt);

            vel.y += m_gravity * static_cast<float>(dt);

            pos.x += vel.x * static_cast<float>(dt);
            pos.y += vel.y * static_cast<float>(dt);
            pos.z += vel.z * static_cast<float>(dt);

            if (pos.y < m_floorY)
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

    void syncPositionToTransform()
    {
        auto entities = m_registry.getEntitiesWith<Position, TransformComponent>();

        for (auto e : entities)
        {
            auto& pos = m_registry.getComponent<Position>(e);
            auto& transform = m_registry.getComponent<TransformComponent>(e);

            transform.position = {pos.x, pos.y, pos.z};
        }
    }

    void syncCollisionLastPositions()
    {
        auto entities = m_registry.getEntitiesWith<CollisionBoxComponent, TransformComponent>();
        for (auto e : entities)
        {
            const auto& t   = m_registry.getComponent<TransformComponent>(e).position;
            auto&       box = m_registry.getComponent<CollisionBoxComponent>(e);
            box.lastPosition = t;
        }
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
    }

    void updatePlayerFacingRay()
    {
        auto&       rayTf    = m_registry.getComponent<TransformComponent>(m_facingRayEntity);
        const auto& playerTf = m_registry.getComponent<TransformComponent>(m_player);
        rayTf.rotation       = playerTf.rotation;

        auto&       camComp = m_registry.getComponent<CameraComponent>(m_camera);
        const auto& camTf   = m_registry.getComponent<TransformComponent>(m_camera);

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
            (targetTf.position.x + camComp.lookAtOffset.x) - camTf.position.x;
        const float dz =
            (targetTf.position.z + camComp.lookAtOffset.z) - camTf.position.z;
        const float hLen = std::sqrt(dx * dx + dz * dz);

        Vec3 dir{0.0f, 0.0f, -1.0f};
        if (hLen > 1.0e-5f)
        {
            dir.x = dx / hLen;
            dir.y = 0.0f;
            dir.z = dz / hLen;
        }

        auto& ray = m_registry.getComponent<RayComponent>(m_facingRayEntity);
        // Origin from attached transform (socket on player → follows movement).
        ray.origin       = rayTf.position;
        ray.direction    = normalize(dir);
        ray.maxDistance  = m_rayMaxDistance;
        ray.ignoreEntity = m_player;
        // Only collide with layers for enemy "2" and "3" (bits 2 and 3); enemy "1" uses bit 1 only.
        ray.layerMask = kFacingRayEnemyLayerMask;
        ray.radius    = m_raySweepRadius;
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
            if (e == m_playerAnimBone)
                return "Bone(hand)";
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
        for (Entity en : m_enemies)
            printRow(en);
        printRow(m_camera);
        if (m_playerAnimBone != INVALID_ENTITY)
            printRow(m_playerAnimBone);

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

        std::cout << std::left << "Camera view (3D persp.)   Legend:  P player   1-9 enemy";
        if (m_playerAnimBone != INVALID_ENTITY &&
            m_registry.hasComponent<WorldTransformComponent>(m_playerAnimBone))
        {
            const Mat4& w = m_registry.getComponent<WorldTransformComponent>(m_playerAnimBone).world;
            std::cout << "   | bone(hand) T: (" << std::fixed << std::setprecision(2)
                      << w.m[12] << ", " << w.m[13] << ", " << w.m[14] << ")"
                      << std::defaultfloat;
        }
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

    /// In-memory skeleton + clip on the player; one tracked bone entity for gameplay/debug.
    void attachPlayerSkeletonAnimation()
    {
        auto model = std::make_shared<ModelAsset>();
        Bone b0;
        b0.name = "root";
        b0.parentIndex = -1;
        b0.restTranslation = {0.0f, 0.0f, 0.0f};
        Bone b1;
        b1.name = "hand";
        b1.parentIndex = 0;
        b1.restTranslation = {0.0f, 1.0f, 0.0f};
        model->skeleton.bones = {b0, b1};
        model->skeleton.boneMap["root"] = 0;
        model->skeleton.boneMap["hand"] = 1;

        AnimationClipData clip;
        clip.name = "wave";
        clip.durationSec = 2.0f;
        ClipBoneChannel rotCh;
        rotCh.boneIndex = 1;
        rotCh.path = AnimChannelPath::Rotation;
        rotCh.quatKeys.push_back({0.0f, quatNormalize(Quat{0.0f, 0.0f, 0.0f, 1.0f})});
        // Y + X rotation so the hand arc is obvious in world space + full matrix debug draw.
        rotCh.quatKeys.push_back({1.0f, quatNormalize(Quat{0.2f, 0.35f, 0.1f, 0.9f})});
        rotCh.quatKeys.push_back({2.0f, quatNormalize(Quat{0.0f, 0.0f, 0.0f, 1.0f})});
        clip.channels.push_back(rotCh);
        model->clips.push_back(clip);

        SkeletonInstanceComponent skel{};
        skel.model = model;
        skel.syncBoneIndices = {1};
        m_registry.addComponent(m_player, std::move(skel));
        m_registry.addComponent(m_player, SkeletonPoseComponent{});
        m_registry.addComponent(m_player, AnimationPlaybackComponent{});

        m_playerAnimBone = m_registry.createEntity();
        m_registry.addComponent(m_playerAnimBone, BoneInstanceComponent{m_player, 1});
        m_registry.addComponent(m_playerAnimBone, WorldTransformComponent{});
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

        m_registry.addComponent(e, StateMachineComponent{});
        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
        sm.initialize(e, config);
    }

private:
    Registry m_registry;

    std::unique_ptr<Control> m_control;

    AssetManager m_assetManager;
    StreamingLoadService m_streamingLoadService{m_assetManager};
    StreamingAssetCache m_streamingCache{m_assetManager};

    TransformSystem m_transformSystem;
    AnimationSystem m_animationSystem;
    BoneSyncSystem m_boneSyncSystem;
    SocketSystem m_socketSystem;
    AttachmentSystem m_attachmentSystem;
    CameraSystem m_cameraSystem;
    SpatialGridSystem m_spatialGrid;
    RaycastSystem     m_raycastSystem{m_spatialGrid};
    CollisionSystem   m_collisionSystem;

    Entity m_player{};
    /// Bone index 1 ("hand") driven by in-memory clip on `m_player`.
    Entity m_playerAnimBone{INVALID_ENTITY};
    Entity m_camera{};
    Entity m_cameraSocket{};
    Entity m_playerRaySocket{INVALID_ENTITY};
    Entity m_facingRayEntity{INVALID_ENTITY};

    std::vector<Entity> m_enemies;

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

    const float m_collisionHalfX = 0.4f;
    const float m_collisionHalfY = 0.55f;
    const float m_collisionHalfZ = 0.4f;

    std::uint64_t m_playerEnemyCollisionEnters = 0;

    /// Mouse delta → orbit input (used with centered cursor; window-coordinate deltas).
    float m_mouseSensitivity = 0.0025f;

    OpenGLRenderSystem m_gl;

    const float m_rayMaxDistance = 40.0f;
    // Swept sphere radius for facing query (thick ray). 0 would be a line only.
    const float m_raySweepRadius = 0.55f;

    // Collision layers (bit flags on CollisionBoxComponent::layer). Ray.layerMask selects what can be hit.
    static constexpr uint32_t kLayerPlayer              = 1u << 0;
    static constexpr uint32_t kLayerEnemySlot1 = 1u << 2; // ASCII "2"
    static constexpr uint32_t kLayerEnemySlot2 = 1u << 3; // ASCII "3"
    static constexpr uint32_t kFacingRayEnemyLayerMask  = kLayerEnemySlot1 | kLayerEnemySlot2;
};