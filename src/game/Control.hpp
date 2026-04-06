#pragma once

#include "../ecs/Registry.hpp"
#include "../game/Components.hpp"
#include "../components/PlayerMovementIntentComponent.hpp"
#include "../components/StateMachineComponent.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../game/StateEventType.hpp"

#include <cmath>
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

    bool jumpPressed = false;

    bool quit = false;
};

// ----------------------------------
/// Maps hardware input → movement intent + camera deltas. Does not apply physics,
/// terrain checks, or jump velocity — the game does that from PlayerMovementIntentComponent.
class Control
{
public:
    /// When `useTerminalInput` is false (e.g. GLFW window), call `submitInput` instead of `handleInput`.
    Control(Entity player, Entity camera, float speed, bool useTerminalInput = true)
        : m_player(player)
        , m_camera(camera)
        , m_speed(speed)
        , m_useTerminalInput(useTerminalInput)
    {
        if (m_useTerminalInput)
            setupTerminal();
    }

    ~Control()
    {
        if (m_useTerminalInput)
            restoreTerminal();
    }

    void handleInput(Registry& registry)
    {
        if (!m_useTerminalInput)
            return;

        m_rotateLeft = false;
        m_rotateRight = false;

        InputState input{};
        readKeyboard(input);

        if (m_rotateLeft)  input.mouseDeltaX = -0.85f;
        if (m_rotateRight) input.mouseDeltaX =  0.85f;

        applyInput(registry, input);
    }

    void submitInput(Registry& registry, const InputState& input) { applyInput(registry, input); }

    bool shouldClose() const
    {
        return m_shouldClose;
    }

private:
    void applyInput(Registry& registry, const InputState& input)
    {
        applyPlayerMovementIntent(registry, input);
        applyCameraInput(registry, input);

        if (input.quit)
            m_shouldClose = true;
    }

    void readKeyboard(InputState& input)
    {
        char key;

        while (read(STDIN_FILENO, &key, 1) > 0)
        {
            switch (key)
            {
                case 'w': case 'W': input.moveZ -= 1.0f; break;
                case 's': case 'S': input.moveZ += 1.0f; break;
                case 'a': case 'A': input.moveX -= 1.0f; break;
                case 'd': case 'D': input.moveX += 1.0f; break;

                case 'o': case 'O': m_rotateLeft = true; break;
                case 'p': case 'P': m_rotateRight = true; break;

                case 'q': case 'Q': input.quit = true; break;

                case ' ': input.jumpPressed = true; break;
            }
        }
    }

    void applyPlayerMovementIntent(Registry& registry, const InputState& input)
    {
        auto& sm = registry.getComponent<StateMachineComponent>(m_player).machine;
        if (!registry.hasComponent<PlayerMovementIntentComponent>(m_player))
            return;

        auto& intent = registry.getComponent<PlayerMovementIntentComponent>(m_player);

        float fx = 0.0f;
        float fz = -1.0f;
        float rx = 1.0f;
        float rz = 0.0f;

        if (registry.hasComponent<TransformComponent>(m_camera) &&
            registry.hasComponent<CameraComponent>(m_camera))
        {
            auto& camComp = registry.getComponent<CameraComponent>(m_camera);
            auto& camTf   = registry.getComponent<TransformComponent>(m_camera);

            Entity lookTarget = m_player;
            if (camComp.enableLockOn && camComp.lockOnTarget != INVALID_ENTITY)
            {
                lookTarget = camComp.lockOnTarget;
            }
            else if (camComp.enableLookAt && camComp.lookAtTarget != INVALID_ENTITY)
            {
                lookTarget = camComp.lookAtTarget;
            }

            if (lookTarget != INVALID_ENTITY &&
                registry.hasComponent<TransformComponent>(lookTarget))
            {
                const auto& tgtTf = registry.getComponent<TransformComponent>(lookTarget);
                const float dx =
                    (tgtTf.position.x + camComp.lookAtOffset.x) - camTf.position.x;
                const float dz =
                    (tgtTf.position.z + camComp.lookAtOffset.z) - camTf.position.z;
                const float hDist = std::sqrt(dx * dx + dz * dz);
                if (hDist > 1.0e-5f)
                {
                    fx = dx / hDist;
                    fz = dz / hDist;
                }
                else
                {
                    const float yaw = camComp.currentYaw;
                    fx = -std::sin(yaw);
                    fz = -std::cos(yaw);
                }
                rx = fz;
                rz = -fx;
            }
        }

        float mx = input.moveX;
        float mz = input.moveZ;
        const float mLen = std::sqrt(mx * mx + mz * mz);
        if (mLen > 1.0e-5f)
        {
            mx /= mLen;
            mz /= mLen;
        }

        const float fwdIn = -mz;
        intent.horizontalVelX = (mx * rx + fwdIn * fx) * m_speed;
        intent.horizontalVelZ = (mx * rz + fwdIn * fz) * m_speed;
        intent.jumpPressed    = input.jumpPressed;

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
    bool m_useTerminalInput = true;
    bool m_shouldClose = false;

    bool m_rotateLeft = false;
    bool m_rotateRight = false;

    termios m_oldt{};
    int m_oldf{};
};
