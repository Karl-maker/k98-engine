#include "../core/GameLoop.hpp"
#include "../ecs/Registry.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

// Components
struct Position {
    float x, y, z;
};

struct Velocity {
    float vx, vy, vz;
};

struct Health {
    int hp;
};

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

        // Create a few enemies
        for (int i = 0; i < 3; ++i)
        {
            Entity e = m_registry.createEntity();

            m_registry.addComponent<Position>(e, {float(i * 2), 0.0f, 0.0f});
            m_registry.addComponent<Velocity>(e, {1.0f + i, 0.0f, 0.0f});
            m_registry.addComponent<Health>(e, {100});

            m_entities.push_back(e);
        }
    }

    void onInput() override
    {
        // No input for now
    }

    void onUpdate(double fixedDeltaTime) override
    {
        m_elapsedTime += fixedDeltaTime;

        // 🔥 Circular movement
        for (size_t i = 0; i < m_entities.size(); ++i)
        {
            Entity entity = m_entities[i];

            auto& pos = m_registry.getComponent<Position>(entity);

            // Each entity gets its own radius + speed
            float radius = 2.0f + i;        // different circle sizes
            float speed  = 1.0f + i * 0.5f; // different speeds

            float angle = static_cast<float>(m_elapsedTime * speed);

            pos.x = radius * std::cos(angle);
            pos.z = radius * std::sin(angle);

            // Keep Y constant (flat circle)
            pos.y = 0.0f;
        }

        if (m_elapsedTime >= m_totalDuration)
        {
            m_shouldClose = true;
        }
    }

    void onRender(double alpha) override
    {
        // Clear screen (basic)
        std::cout << "\033[2J\033[H";

        std::cout << "=== ECS ENTITY TABLE ===\n\n";

        // Table header
        std::cout << std::left
                  << std::setw(10) << "Entity"
                  << std::setw(15) << "PosX"
                  << std::setw(15) << "PosY"
                  << std::setw(15) << "PosZ"
                  << std::setw(15) << "Velocity"
                  << std::setw(10) << "HP"
                  << "\n";

        std::cout << "-------------------------------------------------------------\n";

        // Render each entity
        for (auto entity : m_entities)
        {
            auto& pos = m_registry.getComponent<Position>(entity);
            auto& vel = m_registry.getComponent<Velocity>(entity);
            auto& hp  = m_registry.getComponent<Health>(entity);

            std::cout << std::left
                      << std::setw(10) << entity
                      << std::setw(15) << pos.x
                      << std::setw(15) << pos.y
                      << std::setw(15) << pos.z
                      << std::setw(15) << vel.vx
                      << std::setw(10) << hp.hp
                      << "\n";
        }

        std::cout << "\nTime: " << m_elapsedTime << "\n";
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

    double m_elapsedTime = 0.0;
    const double m_totalDuration = 5.0;

    bool m_shouldClose = false;
};