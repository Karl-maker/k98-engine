#pragma once

#include <string>

// -----------------------------------------------------------------------------
// Helpers for resolving and classifying audio asset paths (import pipeline).
// Decoding and GPU/device playback live in AudioClipCache / AudioSystem.
// -----------------------------------------------------------------------------

namespace AudioImporter {

/// Returns true for extensions commonly decoded by miniaudio (via file extension).
bool isSupportedAudioFile(const std::string& path);

/// Tries `path` as-is; if missing, prepends GAME_ENGINE_PROJECT_ROOT when defined.
std::string resolveFilesystemPath(const std::string& path);

} // namespace AudioImporter
