#pragma once

/// Cursor deltas for third-person orbit (cursor-disabled / absolute position).
struct ThirdPersonOrbitInputStateComponent
{
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;
    bool cursorSampleInitialized = false;
};
