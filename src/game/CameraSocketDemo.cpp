#pragma once

#include "../core/IGame.hpp"
#include "../ecs/Registry.hpp"
#include "../control/Control.hpp"
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

class CameraSocketDemo final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Camera Socket Demo Started\n";

        registerComponents();
        createPlayer();
        createEnemies();
        createCameraRig();
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

        // order matters
        m_transformSystem.update(m_registry);
        m_socketSystem.update(m_registry);
        m_attachmentSystem.update(m_registry);
        m_cameraSystem.update(m_registry);
    }

    void onRender(double) override
    {
        std::cout << "\033[2J\033[H";

        printActors();
        printCameraDebug();
    }

    void onStop() override
    {
        std::cout << "Camera Socket Demo Stopped\n";
    }

    bool shouldClose() const override
    {
        return m_shouldClose;
    }

private:
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

        m_control = std::make_unique<Control>(m_player, 5.0f);
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
        }
    }

    void createCameraRig()
    {
        m_socket = m_registry.createEntity();

        m_registry.addComponent(m_socket, SocketComponent{
            m_player,
            {0.0f, 2.0f, -5.0f}, // behind and above player
            {},
            {}
        });

        m_camera = m_registry.createEntity();

        m_registry.addComponent(m_camera, TransformComponent{});
        m_registry.addComponent(m_camera, WorldTransformComponent{});
        m_registry.addComponent(m_camera, CameraComponent{});

        m_registry.addComponent(m_camera, AttachComponent{
            m_player,
            m_socket,
            {0.0f, 0.0f, 0.0f},
            {},
            true,
            true
        });
    }

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
        auto& playerPos = m_registry.getComponent<Position>(m_player);
        auto& playerWorld = m_registry.getComponent<WorldTransformComponent>(m_player);
        auto& socket = m_registry.getComponent<SocketComponent>(m_socket);
        auto& cam = m_registry.getComponent<TransformComponent>(m_camera);

        std::cout << "\nPlayer Pos: "
                  << playerPos.x << ", "
                  << playerPos.y << ", "
                  << playerPos.z << "\n";

        std::cout << "Player World: "
                  << playerWorld.world.m[12] << ", "
                  << playerWorld.world.m[13] << ", "
                  << playerWorld.world.m[14] << "\n";

        std::cout << "Socket: "
                  << socket.worldTransform.m[12] << ", "
                  << socket.worldTransform.m[13] << ", "
                  << socket.worldTransform.m[14] << "\n";

        std::cout << "Camera: "
                  << cam.position.x << ", "
                  << cam.position.y << ", "
                  << cam.position.z << "\n";

        std::cout << "Time: " << m_elapsedTime << "\n";
        std::cout << "Controls: WASD | Q quit\n";
    }

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
    Entity m_socket{};

    double m_elapsedTime = 0.0;
    double m_lastMoveInputTime = 0.0;
    bool m_shouldClose = false;

    const double m_inputGrace = 0.15;
    const double m_minMovingTime = 0.12;
    const double m_moveRefreshTime = 0.08;
    const double m_minSlowingTime = 0.18;

    const float m_slowingFactorPer60Fps = 0.92f;
    const float m_velocityDeadZone = 0.015f;
};