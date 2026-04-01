#pragma once

struct GameLoopConfig
{
    // How many times per second fixed updates should run.
    int targetUpdatesPerSecond = 60;

    // Optional render cap. If <= 0, rendering is uncapped.
    int maxFramesPerSecond = 60;

    // Prevents huge catch-up spikes after pauses or debugging.
    double maxFrameTimeSeconds = 0.25;

    // Safety limit to avoid spiral-of-death from too many updates in one frame.
    int maxUpdatesPerFrame = 5;
};