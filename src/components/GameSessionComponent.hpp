#pragma once

/// Bootstrap / runtime session for a game instance (one entity is typical).
struct GameSessionComponent
{
    struct Settings
    {
        bool openglDebugHud = false;
        int glSwapInterval = 1;
        int targetFpsPreset = 60;
    };

    Settings settings{};
    bool shouldClose = false;
};
