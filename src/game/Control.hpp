#pragma once

#include "../ecs/Registry.hpp"
#include "../game/Components.hpp"
#include "../components/StateMachineComponent.hpp"
#include "../components/CameraComponent.hpp"
#include "../game/StateEventType.hpp"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

// ----------------------------------
struct InputState
{
    float moveX = 0.0f;
    float moveZ = 0.0f;

    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;

    bool quit = false;
};

// ----------------------------------
class Control
{
public:
    Control(Entity player, Entity camera, float speed)
        : m_player(player), m_camera(camera), m_speed(speed)
    {
        setupTerminal();
    }

    ~Control()
    {
        restoreTerminal();
    }

    void handleInput(Registry& registry)
    {
        m_rotateLeft = false;
        m_rotateRight = false;

        InputState input{};
        readKeyboard(input);

        if (m_rotateLeft)  input.mouseDeltaX = -2.5f;
        if (m_rotateRight) input.mouseDeltaX =  2.5f;

        applyPlayerMovement(registry, input);
        applyCameraInput(registry, input);

        if (input.quit)
            m_shouldClose = true;
    }

    bool shouldClose() const
    {
        return m_shouldClose;
    }

private:

    void readKeyboard(InputState& input)
    {
        char key;

        while (read(STDIN_FILENO, &key, 1) > 0)
        {
            switch (key)
            {
                case 'w': case 'W': input.moveZ = -1.0f; break;
                case 's': case 'S': input.moveZ =  1.0f; break;
                case 'a': case 'A': input.moveX = -1.0f; break;
                case 'd': case 'D': input.moveX =  1.0f; break;

                case 'o': case 'O': m_rotateLeft = true; break;
                case 'p': case 'P': m_rotateRight = true; break;

                case 'q': case 'Q': input.quit = true; break;
            }
        }
    }

    void applyPlayerMovement(Registry& registry, const InputState& input)
    {
        auto& sm  = registry.getComponent<StateMachineComponent>(m_player).machine;
        auto& vel = registry.getComponent<Velocity>(m_player);

        vel.vx = input.moveX * m_speed;
        vel.vz = input.moveZ * m_speed;

        if (input.moveX != 0.0f || input.moveZ != 0.0f)
            sm.handleEvent(StateEventType::MoveUp);
        else
            sm.handleEvent(StateEventType::Stop);
    }

    void applyCameraInput(Registry& registry, const InputState& input)
    {
        if (!registry.hasComponent<CameraComponent>(m_camera))
            return;

        auto& cam = registry.getComponent<CameraComponent>(m_camera);

        if (!cam.enableOrbit)
            return;

        cam.inputDeltaX = input.mouseDeltaX;
        cam.inputDeltaY = input.mouseDeltaY;
    }

    void setupTerminal()
    {
        tcgetattr(STDIN_FILENO, &m_oldt);
        termios newt = m_oldt;

        newt.c_lflag &= ~(ICANON | ECHO);
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
    Entity m_camera;

    float m_speed;
    bool m_shouldClose = false;
    
    bool m_rotateLeft = false;
    bool m_rotateRight = false;

    termios m_oldt{};
    int m_oldf{};
};