#pragma once

#include <chrono>
#include <string>

/// OpenGL debug HUD toggle, smoothed FPS, and optional detail text (global/session entity).
struct DebugHudStateComponent
{
    bool enabled = false;
    std::string detailText;
    float fpsSmooth = 0.f;
    bool haveFrameTime = false;
    std::chrono::steady_clock::time_point lastFrameTime{};
    /// Edge-detect for GLFW_KEY_L (or any bound toggle).
    bool toggleKeyHeld = false;
};
