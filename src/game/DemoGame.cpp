#include "../core/GameLoop.hpp"

#include <iostream>

class DemoGame final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Game started\n";
    }

    void onInput() override
    {
        // Example:
        // Read input from window system, keyboard, controller, etc.
        //
        // For demo purposes, we exit after some updates.
    }

    void onUpdate(double fixedDeltaTime) override
    {
        m_elapsedTime += fixedDeltaTime;
        ++m_updateCount;

        std::cout << "Update #" << m_updateCount
                  << " | dt: " << fixedDeltaTime
                  << " | elapsed: " << m_elapsedTime << "\n";

        if (m_elapsedTime >= 2.0)
        {
            m_shouldClose = true;
        }
    }

    void onRender(double alpha) override
    {
        std::cout << "Render | alpha: " << alpha << "\n";
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
    double m_elapsedTime = 0.0;
    int m_updateCount = 0;
    bool m_shouldClose = false;
};