#pragma once

/// Per-entity edge state for a play-audio hotkey (e.g. GLFW_KEY_F).
struct AudioHotkeyEdgeComponent
{
    bool keyHeld = false;
};
