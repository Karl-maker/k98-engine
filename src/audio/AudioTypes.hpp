#pragma once

#include <miniaudio.h>

// Decoded clip in miniaudio's buffer format (shared by cached playback instances).

struct CachedAudioBuffer {
    ma_audio_buffer buffer{};
    bool valid = false;

    ~CachedAudioBuffer();
};
