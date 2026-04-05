#include "AudioClipCache.hpp"

#include <future>
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

std::shared_ptr<CachedAudioBuffer> AudioClipCache::getOrLoad(const std::string& resolvedPath)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(resolvedPath);
        if (it != m_cache.end())
            return it->second;
    }

    std::shared_ptr<CachedAudioBuffer> decoded = std::async(std::launch::async, [resolvedPath]() {
        return decodeFileToBufferImpl(resolvedPath);
    }).get();

    if (!decoded)
        return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(resolvedPath);
    if (it != m_cache.end())
        return it->second;
    m_cache[resolvedPath] = decoded;
    return decoded;
}

void AudioClipCache::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}
