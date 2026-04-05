#pragma once

#include <memory>

struct ma_engine;

// -----------------------------------------------------------------------------
// Owns miniaudio's high-level engine (playback device + mixing). The engine uses
// an internal device thread for real-time audio I/O and mixing.
// -----------------------------------------------------------------------------
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool init();
    void shutdown();

    ma_engine* engine();
    const ma_engine* engine() const;

private:
    std::unique_ptr<ma_engine> m_engine;
};
