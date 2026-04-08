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
    int targetFpsPreset = 60;
    /// Multi-line debug text drawn in the top-right when non-empty (requires `enabled`).
    std::string debugDetail;
};
