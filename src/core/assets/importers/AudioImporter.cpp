#include "AudioImporter.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace {

std::string lowerExt(const std::string& path)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return {};
    std::string ext = path.substr(dot);
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

} // namespace

namespace AudioImporter {

bool isSupportedAudioFile(const std::string& path)
{
    const std::string ext = lowerExt(path);
    return ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg" || ext == ".m4a";
}

std::string resolveFilesystemPath(const std::string& path)
{
    if (path.empty())
        return {};
    {
        std::ifstream test(path, std::ios::binary);
        if (test.good())
            return path;
    }
#ifdef GAME_ENGINE_PROJECT_ROOT
    std::string root = GAME_ENGINE_PROJECT_ROOT;
    if (!root.empty() && root.back() != '/')
        root += '/';
    const std::string combined = root + path;
    {
        std::ifstream test(combined, std::ios::binary);
        if (test.good())
            return combined;
    }
#endif
    return path;
}

} // namespace AudioImporter
