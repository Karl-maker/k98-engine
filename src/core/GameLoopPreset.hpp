#pragma once

#include "GameLoopConfig.hpp"

/// Named fixed-step + render-cap presets for the main loop.
enum class GameLoopPreset {
    Fps60,
    Fps120,
};

inline GameLoopConfig makeGameLoopConfig(GameLoopPreset preset)
{
    GameLoopConfig c{};
    c.maxFrameTimeSeconds = 0.25;
    c.maxUpdatesPerFrame  = 5;
    switch (preset) {
    case GameLoopPreset::Fps60:
        c.targetUpdatesPerSecond = 60;
        c.maxFramesPerSecond     = 60;
        break;
    case GameLoopPreset::Fps120:
        c.targetUpdatesPerSecond = 120;
        c.maxFramesPerSecond     = 120;
        break;
    }
    return c;
}
