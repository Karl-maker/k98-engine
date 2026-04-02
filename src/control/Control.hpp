#pragma once

#include "../ecs/Registry.hpp"
#include "../game/Components.hpp"

#include <cerrno>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

class Control
{
public:
    Control(Entity player, float speed)
        : m_player(player), m_speed(speed)
    {
        setupTerminal();
    }

    ~Control()
    {
        restoreTerminal();
    }

    void handleInput(Registry& registry)
    {
        auto& vel = registry.getComponent<Velocity>(m_player);

        // Reset velocity every frame
        vel.vx = 0.0f;
        vel.vz = 0.0f;

        char key;
        while (read(STDIN_FILENO, &key, 1) > 0)
        {
            // Handle arrow keys (ESC sequences)
            if (key == 27) // ESC
            {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
                if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

                if (seq[0] == '[')
                {
                    switch (seq[1])
                    {
                        case 'W': vel.vz = -m_speed; break; // UP
                        case 'S': vel.vz =  m_speed; break; // DOWN
                        case 'D': vel.vx =  m_speed; break; // RIGHT
                        case 'A': vel.vx = -m_speed; break; // LEFT
                    }
                }
                continue;
            }

            // WASD (fallback / alternative)
            switch (key)
            {
                case 'w': case 'W': vel.vz = -m_speed; break;
                case 's': case 'S': vel.vz =  m_speed; break;
                case 'a': case 'A': vel.vx = -m_speed; break;
                case 'd': case 'D': vel.vx =  m_speed; break;
                case 'q': case 'Q': m_shouldClose = true; break;
            }
        }
    }

    bool shouldClose() const
    {
        return m_shouldClose;
    }

private:
    void setupTerminal()
    {
        tcgetattr(STDIN_FILENO, &m_oldt);
        termios newt = m_oldt;

        newt.c_lflag &= ~(ICANON | ECHO); // raw mode
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        m_oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, m_oldf | O_NONBLOCK);
    }

    void restoreTerminal()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &m_oldt);
        fcntl(STDIN_FILENO, F_SETFL, m_oldf);
    }

private:
    Entity m_player;
    float m_speed;
    bool m_shouldClose = false;

    termios m_oldt{};
    int m_oldf{};
};