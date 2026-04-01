#include "../core/GameLoop.hpp"

#include <iostream>
#include <string>

class HelloWorld final : public IGame
{
public:
    void onStart() override
    {
        std::cout << "Game started\n";
    }

    void onInput() override
    {
        // No input needed for this demo
    }

    void onUpdate(double fixedDeltaTime) override
    {
        m_elapsedTime += fixedDeltaTime;

        // Accumulate time for typing effect
        m_typingTimer += fixedDeltaTime;

        // Type one character every interval
        if (m_typingTimer >= m_typingInterval && m_currentIndex < m_text.size())
        {
            m_typingTimer = 0.0;
            m_currentIndex++;
        }

        // Stop after full text is displayed + short delay
        if (m_currentIndex >= m_text.size() && m_elapsedTime >= m_totalDuration)
        {
            m_shouldClose = true;
        }
    }

    void onRender(double alpha) override
    {
        // Render only the visible substring
        std::string visibleText = m_text.substr(0, m_currentIndex);

        // Clear line (simple console trick)
        std::cout << "\r" << visibleText << std::flush;
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
    const std::string m_text = "Hello World";

    double m_elapsedTime = 0.0;
    double m_typingTimer = 0.0;

    const double m_typingInterval = 0.2; // seconds per character
    const double m_totalDuration = 4.0;  // total runtime before exit

    size_t m_currentIndex = 0;
    bool m_shouldClose = false;
};