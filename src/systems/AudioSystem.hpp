#pragma once

#include "../audio/AudioClipCache.hpp"
#include "../audio/AudioEngine.hpp"

#include "../ecs/Entity.hpp"

class Registry;

// -----------------------------------------------------------------------------
// Applies AudioComponent state to miniaudio voices. Playback mixing and device
// I/O run on miniaudio's internal audio thread; clip decode uses std::async on
// cache miss (see AudioClipCache).
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
