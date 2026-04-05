#pragma once

#include "AudioTypes.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// -----------------------------------------------------------------------------
// Decoded PCM cache (one ma_audio_buffer per resolved file path). Decode runs on
// a temporary std::async thread on cache miss so the main thread stays responsive.
// -----------------------------------------------------------------------------

class AudioClipCache {
public:
    AudioClipCache() = default;

    /// Returns nullptr on failure. Thread-safe; safe to call from AudioSystem::update.
    std::shared_ptr<CachedAudioBuffer> getOrLoad(const std::string& resolvedPath);

    void clear();

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<CachedAudioBuffer>> m_cache;
};
