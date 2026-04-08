#pragma once

/// Startup options for `MovementTutorial` (see `main.cpp` / CLI).
struct MovementTutorialSettings {
    /// When true, OpenGL draws an overlay (FPS, entity count, locomotion state) and F3 toggles at runtime.
    bool openglDebugHud = false;
    int glSwapInterval = 1;
    /// Label for the debug HUD (matches active `GameLoopPreset`, e.g. 60 or 120).
    int targetFpsPreset = 60;
};
