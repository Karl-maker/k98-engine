#include "AudioEngine.hpp"

#include <miniaudio.h>

#include <new>

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::init()
{
    if (m_engine)
        return true;

    m_engine.reset(new (std::nothrow) ma_engine{});
    if (!m_engine)
        return false;

    if (ma_engine_init(nullptr, m_engine.get()) != MA_SUCCESS) {
        m_engine.reset();
        return false;
    }
    return true;
}

void AudioEngine::shutdown()
{
    if (!m_engine)
        return;
    ma_engine_uninit(m_engine.get());
    m_engine.reset();
}

ma_engine* AudioEngine::engine()
{
    return m_engine.get();
}

const ma_engine* AudioEngine::engine() const
{
    return m_engine.get();
}
