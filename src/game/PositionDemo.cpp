#pragma once

#include "../core/IGame.hpp"
#include "../ecs/Registry.hpp"
#include "../statemachine/StateMachine.hpp"
#include "../statemachine/StateMachineTypes.hpp"
#include "../control/Control.hpp"
#include "Components.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <memory>

class PositionDemo final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Game started\n";

        m_registry.registerComponent<Position>();
        m_registry.registerComponent<Velocity>();
        m_registry.registerComponent<Health>();
        m_registry.registerComponent<StateMachineComponent>();

        // -------------------------
        // PLAYER
        // -------------------------
        m_player = m_registry.createEntity();

        m_registry.addComponent<Position>(m_player, {0,0,0});
        m_registry.addComponent<Velocity>(m_player, {0,0,0}); // 
        m_registry.addComponent<Health>(m_player, {200});

        setupPlayerStateMachine(m_player);
        m_entities.push_back(m_player);

      
        m_control = std::make_unique<Control>(m_player, 5.0f);

        // -------------------------
        // ENEMIES
        // -------------------------
        for (int i = 0; i < 3; i++)
        {
            Entity e = m_registry.createEntity();

            m_registry.addComponent<Position>(e, {float(i * 3), 0, 0});
            m_registry.addComponent<Velocity>(e, {0,0,0});
            m_registry.addComponent<Health>(e, {100});

            setupEnemyStateMachine(e);
            m_entities.push_back(e);
        }
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
    
        static double lastMoveInputTime = 0.0;
        const double inputGrace = 0.15; // tweak 0.1–0.2
    
        for (auto e : m_entities)
        {
            auto& sm  = m_registry.getComponent<StateMachineComponent>(e).machine;
            auto& pos = m_registry.getComponent<Position>(e);
            auto& vel = m_registry.getComponent<Velocity>(e);
    
            // -------------------------
            // PLAYER: detect movement intent
            // -------------------------
            if (e == m_player)
            {
                bool hasVelocity = std::abs(vel.vx) > 0.01f || std::abs(vel.vz) > 0.01f;
    
                if (hasVelocity)
                {
                    lastMoveInputTime = m_elapsedTime;
    
                    // ensure we are in Moving
                    if (sm.getCurrentState() != "Moving")
                    {
                        sm.handleEvent(StateEventType::MoveUp); // any move event works
                    }
                }
                else
                {
                    bool shouldStop = (m_elapsedTime - lastMoveInputTime) > inputGrace;
    
                    if (shouldStop && sm.getCurrentState() == "Moving")
                    {
                        sm.handleEvent(StateEventType::Stop);
                    }
                }
            }
    
            // -------------------------
            // UPDATE FSM
            // -------------------------
            sm.update(dt);
    
            // -------------------------
            // SLOWING BEHAVIOR
            // -------------------------
            if (sm.getCurrentState() == "Slowing")
            {
                vel.vx *= 0.9f;
                vel.vz *= 0.9f;
    
                if (std::abs(vel.vx) < 0.01f) vel.vx = 0.0f;
                if (std::abs(vel.vz) < 0.01f) vel.vz = 0.0f;
    
                if (vel.vx == 0.0f && vel.vz == 0.0f)
                {
                    sm.handleEvent(StateEventType::Stop);
                }
            }
    
            // -------------------------
            // APPLY MOVEMENT
            // -------------------------
            pos.x += vel.vx * dt;
            pos.z += vel.vz * dt;
        }
    }

    void onRender(double) override
    {
        std::cout << "\033[2J\033[H";

        std::cout << std::left
                  << std::setw(8)  << "Entity"
                  << std::setw(10) << "Type"
                  << std::setw(12) << "State"
                  << std::setw(10) << "Time"
                  << std::setw(10) << "PosX"
                  << std::setw(10) << "PosY"
                  << std::setw(10) << "PosZ"
                  << "\n";

        std::cout << "-------------------------------------------------------------\n";

        for (auto e : m_entities)
        {
            auto& pos = m_registry.getComponent<Position>(e);
            auto& sm  = m_registry.getComponent<StateMachineComponent>(e).machine;

            std::string type = (e == m_player) ? "Player" : "Enemy";

            std::cout << std::setw(8)  << e
                      << std::setw(10) << type
                      << std::setw(12) << sm.getCurrentState()
                      << std::setw(10) << sm.getTimeInState()
                      << std::setw(10) << pos.x
                      << std::setw(10) << pos.y
                      << std::setw(10) << pos.z
                      << "\n";
        }

        std::cout << "\nTime: " << m_elapsedTime << "\n";
        std::cout << "Controls: WASD / Arrows | Q to quit\n";
    }

    void onStop() override
    {
        std::cout << "Game stopped\n";
    }

    bool shouldClose() const override
    {
        return m_shouldClose;
    }

private:

    void setupPlayerStateMachine(Entity e)
    {
        StateMachineConfig config;
        config.initialState = "Idle";

        // -------------------------
        // IDLE
        // -------------------------
        config.states["Idle"] = {
            "Idle",
            [](Entity){ std::cout << "Enter Idle\n"; },
            nullptr,
            nullptr,
            {
                {StateEventType::MoveUp,    "Moving", nullptr},
                {StateEventType::MoveDown,  "Moving", nullptr},
                {StateEventType::MoveLeft,  "Moving", nullptr},
                {StateEventType::MoveRight, "Moving", nullptr}
            }
        };

        // -------------------------
        // MOVING
        // -------------------------
        config.states["Moving"] = {
            "Moving",
            [](Entity){ std::cout << "Enter Moving\n"; },
            nullptr,
            nullptr,
            {
                {
                    StateEventType::Stop,
                    "Slowing",
                    [&](Entity e)
                    {
                        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
                
                        return sm.getTimeInState() > 0.35;
                    }
                },
                {
                    StateEventType::MoveUp,
                    "Moving",
                    [&](Entity e)
                    {
                        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
                        return sm.getTimeInState() > 0.1; // small buffer
                    }
                },
                {
                    StateEventType::MoveDown,
                    "Moving",
                    [&](Entity e)
                    {
                        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
                        return sm.getTimeInState() > 0.1; // small buffer
                    }
                },
                {
                    StateEventType::MoveLeft,
                    "Moving",
                    [&](Entity e)
                    {
                        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
                        return sm.getTimeInState() > 0.1; // small buffer
                    }
                },
                {
                    StateEventType::MoveRight,
                    "Moving",
                    [&](Entity e)
                    {
                        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
                        return sm.getTimeInState() > 0.1; // small buffer
                    }
                }
            }
        };

        // -------------------------
        // SLOWING
        // -------------------------
        config.states["Slowing"] = {
            "Slowing",
            [](Entity){ std::cout << "Enter Slowing\n"; },
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
                    [&](Entity e)
                    {
                        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
                        return sm.getTimeInState() > 0.2; // deceleration time
                    }
                }
            }
        };

        m_registry.addComponent<StateMachineComponent>(e, {});
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
            [&](Entity e){
                m_registry.getComponent<Velocity>(e).vx = -2;
            },
            nullptr,
            nullptr,
            {}
        };

        m_registry.addComponent<StateMachineComponent>(e, {});
        auto& sm = m_registry.getComponent<StateMachineComponent>(e).machine;
        sm.initialize(e, config);
    }

private:
    Registry m_registry;
    std::vector<Entity> m_entities;

    std::unique_ptr<Control> m_control;

    Entity m_player;
    double m_elapsedTime = 0.0;
    bool m_shouldClose = false;
};