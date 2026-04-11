#include "AudioClipCache.hpp"

#include <iostream>
#include <vector>

static std::shared_ptr<CachedAudioBuffer> decodeFileToBufferImpl(const std::string& resolvedPath)
{
    ma_decoder decoder{};
    // Decode to interleaved f32; stereo output is fine for mono sources (upmix).
    ma_decoder_config decConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);
    if (ma_decoder_init_file(resolvedPath.c_str(), &decConfig, &decoder) != MA_SUCCESS) {
        std::cerr << "AudioClipCache: ma_decoder_init_file failed: " << resolvedPath << "\n";
        return nullptr;
    }

    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    ma_format fmt = ma_format_unknown;
    ma_decoder_get_data_format(&decoder, &fmt, &channels, &sampleRate, nullptr, 0);
    if (channels == 0) {
        ma_decoder_uninit(&decoder);
        std::cerr << "AudioClipCache: zero channels: " << resolvedPath << "\n";
        return nullptr;
    }

    constexpr ma_uint64 kChunkFrames = 4096;
    std::vector<float> accum;
    ma_uint64 totalFramesRead = 0;
    std::vector<float> chunk(static_cast<size_t>(kChunkFrames * channels));

    for (;;) {
        ma_uint64 got = 0;
        ma_result rr = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &got);
        if (rr != MA_SUCCESS || got == 0)
            break;
        accum.insert(accum.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(got * channels));
        totalFramesRead += got;
    }
    ma_decoder_uninit(&decoder);

    if (accum.empty() || totalFramesRead == 0) {
        std::cerr << "AudioClipCache: empty decode: " << resolvedPath << "\n";
        return nullptr;
    }

    auto out = std::make_shared<CachedAudioBuffer>();
    ma_audio_buffer_config bufCfg =
        ma_audio_buffer_config_init(ma_format_f32, channels, totalFramesRead, accum.data(), nullptr);
    if (ma_audio_buffer_init_copy(&bufCfg, &out->buffer) != MA_SUCCESS) {
        std::cerr << "AudioClipCache: ma_audio_buffer_init_copy failed: " << resolvedPath << "\n";
        return nullptr;
    }
    out->valid = true;
    return out;
}

AudioClipCache::AudioClipCache()
{
    m_worker = std::thread([this] { workerLoop(); });
}

AudioClipCache::~AudioClipCache()
{
    m_stop.store(true, std::memory_order_release);
    m_cv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

void AudioClipCache::workerLoop()
{
    for (;;) {
        std::string path;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return m_stop.load(std::memory_order_acquire) || !m_queue.empty(); });
            if (m_stop.load(std::memory_order_acquire) && m_queue.empty())
                return;
            if (m_queue.empty())
                continue;
            path = std::move(m_queue.front());
            m_queue.pop();
        }

        std::shared_ptr<CachedAudioBuffer> decoded = decodeFileToBufferImpl(path);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_inQueueOrDecoding.erase(path);
            if (decoded && decoded->valid)
                m_cache[path] = std::move(decoded);
            else
                m_decodeFailed.insert(path);
        }
    }
}

void AudioClipCache::requestDecode(const std::string& resolvedPath)
{
    if (resolvedPath.empty())
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cache.count(resolvedPath) || m_decodeFailed.count(resolvedPath))
        return;
    if (m_inQueueOrDecoding.count(resolvedPath))
        return;
    m_inQueueOrDecoding.insert(resolvedPath);
    m_queue.push(resolvedPath);
    m_cv.notify_one();
}

void AudioClipCache::pollDecodeProgress()
{
    // Completes are applied in worker; optional hook for future metrics.
}

std::shared_ptr<CachedAudioBuffer> AudioClipCache::tryGetCached(const std::string& resolvedPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(resolvedPath);
    if (it == m_cache.end())
        return nullptr;
    return it->second;
}

void AudioClipCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
    m_inQueueOrDecoding.clear();
    m_decodeFailed.clear();
    while (!m_queue.empty())
        m_queue.pop();
}
