#pragma once

#include "AudioTypes.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// -----------------------------------------------------------------------------
// Decoded PCM cache (one ma_audio_buffer per resolved file path). Decode runs on
// a single background worker (FIFO) so the main thread never blocks on file I/O
// or PCM expansion. Call `pollDecodeProgress` from the audio/game loop.
// -----------------------------------------------------------------------------

class AudioClipCache {
public:
    AudioClipCache();
    ~AudioClipCache();

    AudioClipCache(const AudioClipCache&) = delete;
    AudioClipCache& operator=(const AudioClipCache&) = delete;

    /// Starts a decode if not cached / not already queued (non-blocking).
    void requestDecode(const std::string& resolvedPath);

    /// Moves completed decodes into the cache; call once per frame from the main thread.
    void pollDecodeProgress();

    /// Returns a finished decode, or nullptr if still loading or failed.
    std::shared_ptr<CachedAudioBuffer> tryGetCached(const std::string& resolvedPath);

    void clear();

private:
    void workerLoop();

    std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<CachedAudioBuffer>> m_cache;
    std::unordered_set<std::string> m_inQueueOrDecoding;
    std::unordered_set<std::string> m_decodeFailed;

    std::queue<std::string> m_queue;
    std::condition_variable m_cv;
    std::thread m_worker;
    std::atomic<bool> m_stop{false};
};
