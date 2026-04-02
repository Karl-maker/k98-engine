#include "../core/GameLoop.hpp"
#include "../ecs/Registry.hpp"
#include "../control/Control.hpp"   // 👈 add this
#include "Components.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

class PositionDemo final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Game started\n";

        // Register components
        m_registry.registerComponent<Position>();
        m_registry.registerComponent<Velocity>();
        m_registry.registerComponent<Health>();

        // -------------------------
        // 🔥 Create Player FIRST
        // -------------------------
        m_player = m_registry.createEntity();

        m_registry.addComponent<Position>(m_player, {0.0f, 0.0f, 0.0f});
        m_registry.addComponent<Velocity>(m_player, {0.0f, 0.0f, 0.0f});
        m_registry.addComponent<Health>(m_player, {200});

        m_entities.push_back(m_player);

        // Control system for player
        m_control = std::make_unique<Control>(m_player, 5.0f);

        // -------------------------
        // Create enemies
        // -------------------------
        for (int i = 0; i < 3; ++i)
        {
            Entity e = m_registry.createEntity();

            m_registry.addComponent<Position>(e, {float(i * 2), 0.0f, 0.0f});
            m_registry.addComponent<Velocity>(e, {0.0f, 0.0f, 0.0f});
            m_registry.addComponent<Health>(e, {100});

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

        // -------------------------
        // 🔥 Enemy circular movement
        // -------------------------
        for (size_t i = 0; i < m_entities.size(); ++i)
        {
            Entity entity = m_entities[i];

            if (entity == m_player) continue;

            auto& pos = m_registry.getComponent<Position>(entity);

            float radius = 2.0f + i;
            float speed  = 1.0f + i * 0.5f;

            float angle = static_cast<float>(m_elapsedTime * speed);

            pos.x = radius * std::cos(angle);
            pos.z = radius * std::sin(angle);
            pos.y = 0.0f;
        }

        // -------------------------
        // 🔥 Apply Velocity → Position (player)
        // -------------------------
        {
            auto& pos = m_registry.getComponent<Position>(m_player);
            auto& vel = m_registry.getComponent<Velocity>(m_player);

            pos.x += vel.vx * dt;
            pos.z += vel.vz * dt;
            pos.y = 0.0f;
        }

        if (m_elapsedTime >= m_totalDuration)
        {
            m_shouldClose = true;
        }
    }

    void onRender(double alpha) override
    {
        std::cout << "\033[2J\033[H";

        std::cout << "=== ECS ENTITY TABLE ===\n\n";

        std::cout << std::left
                  << std::setw(10) << "Entity"
                  << std::setw(15) << "PosX"
                  << std::setw(15) << "PosZ"
                  << std::setw(15) << "VelX"
                  << std::setw(10) << "HP"
                  << std::setw(10) << "Type"
                  << "\n";

        std::cout << "-------------------------------------------------------------\n";

        for (auto entity : m_entities)
        {
            auto& pos = m_registry.getComponent<Position>(entity);
            auto& vel = m_registry.getComponent<Velocity>(entity);
            auto& hp  = m_registry.getComponent<Health>(entity);

            std::string type = (entity == m_player) ? "Player" : "Enemy";

            std::cout << std::left
                      << std::setw(10) << entity
                      << std::setw(15) << pos.x
                      << std::setw(15) << pos.z
                      << std::setw(15) << vel.vx
                      << std::setw(10) << hp.hp
                      << std::setw(10) << type
                      << "\n";
        }

        std::cout << "\nTime: " << m_elapsedTime << "\n";
        std::cout << "Controls: WASD to move | Q to quit\n";
    }

    void onStop() override
    {
        std::cout << "\nGame stopped\n";
    }

    bool shouldClose() const override
    {
        return m_shouldClose;
    }

private:
    Registry m_registry;
    std::vector<Entity> m_entities;

    std::unique_ptr<Control> m_control;
    Entity m_player;

    double m_elapsedTime = 0.0;
    const double m_totalDuration = 500.0;

    bool m_shouldClose = false;
};