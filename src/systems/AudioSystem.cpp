#include "AudioSystem.hpp"

#include "../components/AudioComponent.hpp"
#include "../core/assets/importers/AudioImporter.hpp"
#include "../ecs/Registry.hpp"

#include <miniaudio.h>

#include <iostream>
#include <unordered_map>

namespace {

struct EntityVoice {
    ma_sound sound{};
    bool initialized = false;
    std::string lastResolvedPath;
    bool wasPlaying = false;
    bool wasPaused = false;
    bool started = false;
};

std::unordered_map<Entity, EntityVoice> g_entityVoices;

static void shutdownAllVoices()
{
    for (auto& kv : g_entityVoices) {
        if (kv.second.initialized) {
            ma_sound_uninit(&kv.second.sound);
            kv.second.initialized = false;
        }
    }
    g_entityVoices.clear();
}

} // namespace

bool AudioSystem::init()
{
    return m_engine.init();
}

void AudioSystem::shutdown()
{
    shutdownAllVoices();
    m_cache.clear();
    m_engine.shutdown();
}

void AudioSystem::update(Registry& registry)
{
    ma_engine* eng = m_engine.engine();
    if (!eng)
        return;

    // Drop voices for removed entities.
    for (auto it = g_entityVoices.begin(); it != g_entityVoices.end();) {
        if (!registry.hasComponent<AudioComponent>(it->first)) {
            if (it->second.initialized) {
                ma_sound_uninit(&it->second.sound);
                it->second.initialized = false;
            }
            it = g_entityVoices.erase(it);
        } else {
            ++it;
        }
    }

    for (Entity e : registry.getEntitiesWith<AudioComponent>()) {
        auto& ac = registry.getComponent<AudioComponent>(e);
        if (ac.clipPath.empty())
            continue;

        const std::string resolved = AudioImporter::resolveFilesystemPath(ac.clipPath);
        if (!AudioImporter::isSupportedAudioFile(resolved)) {
            static bool once = false;
            if (!once) {
                std::cerr << "AudioSystem: unsupported or unknown audio extension: " << resolved << "\n";
                once = true;
            }
            continue;
        }

        std::shared_ptr<CachedAudioBuffer> clip = m_cache.getOrLoad(resolved);
        if (!clip || !clip->valid)
            continue;

        EntityVoice& voice = g_entityVoices[e];
        if (!voice.initialized || voice.lastResolvedPath != resolved) {
            if (voice.initialized) {
                ma_sound_uninit(&voice.sound);
                voice.initialized = false;
            }
            if (ma_sound_init_from_data_source(eng, &clip->buffer, 0, nullptr, &voice.sound) != MA_SUCCESS) {
                std::cerr << "AudioSystem: ma_sound_init_from_data_source failed: " << resolved << "\n";
                continue;
            }
            voice.initialized = true;
            voice.lastResolvedPath = resolved;
            voice.wasPlaying = false;
            voice.wasPaused = false;
            voice.started = false;
        }

        ma_sound_set_looping(&voice.sound, ac.loop ? MA_TRUE : MA_FALSE);

        float vol = ac.volume;
        if (!ac.playing || ac.paused)
            vol = 0.0f;
        ma_sound_set_volume(&voice.sound, vol);

        const bool edgePlay = ac.playing && !voice.wasPlaying;
        const bool unpaused = voice.wasPaused && !ac.paused && ac.playing;

        if (!ac.playing) {
            ma_sound_stop(&voice.sound);
            voice.started = false;
        } else if (ac.playing && !ac.paused) {
            if (edgePlay) {
                ma_sound_seek_to_pcm_frame(&voice.sound, 0);
                ma_sound_start(&voice.sound);
                voice.started = true;
            } else if (unpaused) {
                ma_sound_start(&voice.sound);
            }
            if (!ac.loop && voice.started && !ma_sound_is_playing(&voice.sound)) {
                ac.playing = false;
                voice.started = false;
            }
        }

        voice.wasPlaying = ac.playing;
        voice.wasPaused = ac.paused;
    }
}
