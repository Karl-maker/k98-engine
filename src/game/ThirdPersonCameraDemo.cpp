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
            {0.0f, 1.5f, 0.0f}, // pivot near upper body
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
        cam.followLerp = 4.0f;
        cam.followPositionSpring = true;
        cam.followSpringFrequency = 3.4f;
        cam.followSpringDampingRatio = 0.72f;

        // Look-at behavior
        cam.enableLookAt = true;
        cam.lookAtTarget = m_player;
        cam.lookAtOffset = {0.0f, 1.2f, 0.0f};

        // Orbit behavior
        cam.enableOrbit = true;
        cam.orbitYaw = 3.1415926f; // start behind player
        cam.orbitPitch = 0.35f;
        cam.orbitDistance = 6.0f;
        cam.orbitSensitivity = 2.5f;
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

            pos.x += vel.vx * static_cast<float>(dt);
            pos.z += vel.vz * static_cast<float>(dt);
        }
    }

    void updatePlayerStateIntent(StateMachine& sm, Velocity& vel)
    {
        const bool hasVelocity =
            std::abs(vel.vx) > 0.01f ||
            std::abs(vel.vz) > 0.01f;

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
            vel.vx *= damping;
            vel.vz *= damping;

            if (std::abs(vel.vx) < m_velocityDeadZone) vel.vx = 0.0f;
            if (std::abs(vel.vz) < m_velocityDeadZone) vel.vz = 0.0f;

            if (vel.vx == 0.0f && vel.vz == 0.0f)
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
        std::cout << std::left
                  << std::setw(8)  << "Entity"
                  << std::setw(10) << "Type"
                  << std::setw(12) << "State"
                  << std::setw(10) << "Time"
                  << std::setw(10) << "PosX"
                  << std::setw(10) << "PosY"
                  << std::setw(10) << "PosZ"
                  << "\n";

        std::cout << "----------------------------------------------------------------\n";

        auto actors = m_registry.getEntitiesWith<Position, StateMachineComponent>();

        for (auto e : actors)
        {
            auto& pos = m_registry.getComponent<Position>(e);
            auto& sm  = m_registry.getComponent<StateMachineComponent>(e).machine;

            const std::string type = (e == m_player) ? "Player" : "Enemy";

            std::cout << std::setw(8)  << e
                      << std::setw(10) << type
                      << std::setw(12) << sm.getCurrentState()
                      << std::setw(10) << sm.getTimeInState()
                      << std::setw(10) << pos.x
                      << std::setw(10) << pos.y
                      << std::setw(10) << pos.z
                      << "\n";
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
        std::cout << "Controls: WASD move player | Q quit\n";
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
                m_registry.getComponent<Velocity>(enemy).vx = -2.0f;
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

    const float m_demoOrbitSpeed = 1.2f;
};