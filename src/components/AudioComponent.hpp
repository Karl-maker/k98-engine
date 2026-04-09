#pragma once

#include <string>

// -----------------------------------------------------------------------------
// AudioComponent — logical clip + transport. Playback is driven by AudioSystem
// using miniaudio (see AudioEngine). Register before addComponent<AudioComponent>().
//
// - clipPath: asset path (.mp3, .wav, …); resolved via AudioImporter::resolveFilesystemPath.
// - playing: when true, sound should be audible (unless paused); rising edge restarts from 0.
// - paused: when true, output muted (volume 0) while retaining transport intent.
// - loop: passed to ma_sound_set_looping.
// - volume: linear 0–1 (applied when not paused).
// -----------------------------------------------------------------------------

struct AudioComponent {
    std::string clipPath;
    float volume = 1.0f;
    bool loop = false;
    bool playing = false;
    bool paused = false;
};