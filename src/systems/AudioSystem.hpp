#pragma once

#include "../audio/AudioClipCache.hpp"
#include "../audio/AudioEngine.hpp"

class Registry;

// -----------------------------------------------------------------------------
// Drives AudioComponent transport: miniaudio sounds per entity, clip decode cache.
// Call init() once, update() each frame on the main thread, shutdown() on teardown.
// -----------------------------------------------------------------------------
class AudioSystem {
public:
    bool init();
    void shutdown();
    void update(Registry& registry);

private:
    AudioEngine m_engine;
    AudioClipCache m_cache;
};
