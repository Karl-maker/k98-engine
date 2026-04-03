#pragma once

#include "../ecs/Registry.hpp"
#include "../game/Components.hpp"
#include "../game/StateEventType.hpp"
#include "../statemachine/StateMachine.hpp"

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
        auto& sm = registry.getComponent<StateMachineComponent>(m_player).machine;
        auto& vel = registry.getComponent<Velocity>(m_player);
    
        vel.vx = 0.0f;
        vel.vz = 0.0f;
    
        bool moving = false;
    
        char key;
        while (read(STDIN_FILENO, &key, 1) > 0)
        {
            switch (key)
            {
                case 'w': case 'W':
                    vel.vz = -m_speed;
                    sm.handleEvent(StateEventType::MoveUp);
                    break;

                case 's': case 'S':
                    vel.vz = m_speed;
                    sm.handleEvent(StateEventType::MoveDown);
                    break;

                case 'a': case 'A':
                    vel.vx = -m_speed;
                    sm.handleEvent(StateEventType::MoveLeft);
                    break;

                case 'd': case 'D':
                    vel.vx = m_speed;
                    sm.handleEvent(StateEventType::MoveRight);
                    break;
    
                case 'q': case 'Q':
                    m_shouldClose = true;
                    break;
            }
        }
    
        if (!moving)
        {
            sm.handleEvent(StateEventType::Stop);
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
    double m_lastInputTime = 0.0;
    termios m_oldt{};
    int m_oldf{};
};