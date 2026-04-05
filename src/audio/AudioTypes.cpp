#include "AudioTypes.hpp"

CachedAudioBuffer::~CachedAudioBuffer()
{
    if (valid) {
        ma_audio_buffer_uninit(&buffer);
        valid = false;
    }
}
