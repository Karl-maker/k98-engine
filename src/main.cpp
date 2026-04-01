#include "core/SystemClock.hpp"
#include "game/DemoGame.cpp"

#include <iostream>

int main()
{
    DemoGame game;
    SystemClock clock;

    GameLoopConfig config;
    config.targetUpdatesPerSecond = 60;
    config.maxFramesPerSecond = 60;
    config.maxFrameTimeSeconds = 0.25;
    config.maxUpdatesPerFrame = 5;

    GameLoop loop(game, clock, config);
    loop.run();

    return 0;
}