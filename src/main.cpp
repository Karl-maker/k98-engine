#include "core/SystemClock.hpp"
#include "core/GameLoop.hpp"
#include "core/GameLoopPreset.hpp"
#include "domain/SuperHero.hpp"

#include <iostream>
#include <string>

static void parseArgs(int argc, char** argv, GameLoopPreset& outPreset, bool& outDebugHud)
{
    outPreset   = GameLoopPreset::Fps120;
    outDebugHud = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--120" || a == "--fps=120")
            outPreset = GameLoopPreset::Fps120;
        else if (a == "--60" || a == "--fps=60")
            outPreset = GameLoopPreset::Fps60;
        else if (a == "--debug" || a == "-d")
            outDebugHud = true;
    }
}

int main(int argc, char** argv)
{
    GameLoopPreset preset;
    bool debugHud = true;
    parseArgs(argc, argv, preset, debugHud);

    SuperHero::Settings gameSettings;
    gameSettings.openglDebugHud = debugHud;
    gameSettings.glSwapInterval = 1;
    gameSettings.targetFpsPreset =
        (preset == GameLoopPreset::Fps120) ? 120 : 60;

    SuperHero game(gameSettings);
    SystemClock clock;

    GameLoopConfig config = makeGameLoopConfig(preset);

    std::cout << "SuperHero — presets: --60 | --120   debug: --debug\n";

    GameLoop loop(game, clock, config);
    loop.run();

    return 0;
}
