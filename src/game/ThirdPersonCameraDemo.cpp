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

#include "../systems/TransformSystem.hpp"
#include "../systems/SocketSystem.hpp"
#include "../systems/AttachmentSystem.hpp"
#include "../systems/CameraSystem.hpp"

#include <iostream>
#include <iomanip>
#include <memory>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

class ThirdPersonCameraDemo final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Third Person Camera Demo Started\n";

        registerComponents();
        createPlayer();
        createEnemies();
        createCameraRig();

        m_control = std::make_unique<Control>(m_player, m_camera, 5.0f);
    }

    void onInput() override
    {
        m_control->handleInput(m_registry);

        if (m_control->shouldClose())
        {
            m_shouldClose = true;
        }
    }

    void onUpdate(double dt) override
    {
        m_elapsedTime += dt;

        updateActors(dt);
        syncPositionToTransform();

        // Order matters
        m_transformSystem.update(m_registry);
        m_socketSystem.update(m_registry);
        m_attachmentSystem.update(m_registry);
        m_cameraSystem.update(m_registry, static_cast<float>(dt));
    }

    void onRender(double) override
    {
        std::cout << "\033[2J\033[H";

        printActors();
     
        printCameraDebug();
        printInstructions();
    }

    void onStop() override
    {
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
    }

    void createPlayer()
    {
        m_player = m_registry.createEntity();

        m_registry.addComponent(m_player, Position{0.0f, 0.0f, 0.0f});
        m_registry.addComponent(m_player, Velocity{0.0f, 0.0f, 0.0f});
        m_registry.addComponent(m_player, Health{200});
        m_registry.addComponent(m_player, TransformComponent{});
        m_registry.addComponent(m_player, WorldTransformComponent{});

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

    void printActors()
    {
        static constexpr float kPi = 3.14159265f;

        std::cout << std::left
                  << "Camera view (3D persp.)   Legend:  P player   1-9 enemy\n";

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
        std::cout << "Controls: WASD move (camera-relative) | Space jump | Q quit\n";
        std::cout << "Demo camera: auto-orbits player, periodically locks onto first enemy\n";
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

    TransformSystem m_transformSystem;
    SocketSystem m_socketSystem;
    AttachmentSystem m_attachmentSystem;
    CameraSystem m_cameraSystem;

    Entity m_player{};
    Entity m_camera{};
    Entity m_cameraSocket{};

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
};