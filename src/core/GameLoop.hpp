#pragma once

#include "IGame.hpp"
#include "IClock.hpp"
#include "GameLoopConfig.hpp"

class GameLoop
{
public:
    GameLoop(IGame& game, const IClock& clock, GameLoopConfig config);

    void run();
    void requestExit();
    bool isRunning() const;

private:
    void validateConfig() const;

private:
    IGame& m_game;
    const IClock& m_clock;
    GameLoopConfig m_config;
    bool m_running = false;
    bool m_exitRequested = false;
};