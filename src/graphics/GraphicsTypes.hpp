#pragma once

#include <string>

struct GraphicsInitOptions {
    int swapInterval = 1;
};

using OpenGLInitOptions = GraphicsInitOptions;

struct OpenGLDebugHudSnapshot {
    bool enabled = false;
    float fps = 0.f;
    int entityCount = 0;
    std::string locomotionState;
    /// Domain movement FSM (e.g. Idle / Walk / Sprint).
    std::string movementState;
    /// When `hudHealthMax > 0` and `hudHealthCurrent >= 0`, drawn on the debug HUD (same overlay as FPS).
    float hudHealthCurrent = -1.f;
    float hudHealthMax = 0.f;
    int targetFpsPreset = 60;
    /// Multi-line debug text drawn in the top-right when non-empty (requires `enabled`).
    std::string debugDetail;
};
