#include "GameLoop.hpp"

#include <stdexcept>
#include <algorithm>

GameLoop::GameLoop(IGame& game, const IClock& clock, GameLoopConfig config)
    : m_game(game), m_clock(clock), m_config(config)
{
    validateConfig();
}

void GameLoop::run()
{
    if (m_running)
    {
        throw std::logic_error("GameLoop is already running.");
    }

    m_running = true;
    m_exitRequested = false;

    const double fixedDeltaTime = 1.0 / static_cast<double>(m_config.targetUpdatesPerSecond);
    const double minFrameDuration =
        (m_config.maxFramesPerSecond > 0)
            ? (1.0 / static_cast<double>(m_config.maxFramesPerSecond))
            : 0.0;

    double previousTime = m_clock.nowSeconds();
    double accumulator = 0.0;

    m_game.onStart();

    while (m_running && !m_exitRequested && !m_game.shouldClose())
    {
        const double frameStartTime = m_clock.nowSeconds();
        double frameTime = frameStartTime - previousTime;
        previousTime = frameStartTime;

        // Clamp frame time so that returning from a breakpoint/window drag/etc
        // does not cause massive update bursts.
        frameTime = std::min(frameTime, m_config.maxFrameTimeSeconds);
        accumulator += frameTime;

        m_game.onInput();

        int updatesProcessed = 0;
        while (accumulator >= fixedDeltaTime &&
               updatesProcessed < m_config.maxUpdatesPerFrame)
        {
            m_game.onUpdate(fixedDeltaTime);
            accumulator -= fixedDeltaTime;
            ++updatesProcessed;
        }

        // If we hit the max updates cap, discard excess accumulated time
        // to avoid infinite catch-up under heavy load.
        if (updatesProcessed == m_config.maxUpdatesPerFrame &&
            accumulator >= fixedDeltaTime)
        {
            accumulator = 0.0;
        }

        const double alpha = accumulator / fixedDeltaTime;
        m_game.onRender(alpha);

        if (minFrameDuration > 0.0)
        {
            const double frameEndTime = m_clock.nowSeconds();
            const double workDuration = frameEndTime - frameStartTime;
            const double remainingTime = minFrameDuration - workDuration;

            if (remainingTime > 0.0)
            {
                m_clock.sleepSeconds(remainingTime);
            }
        }
    }

    m_game.onStop();
    m_running = false;
}

void GameLoop::requestExit()
{
    m_exitRequested = true;
}

bool GameLoop::isRunning() const
{
    return m_running;
}

void GameLoop::validateConfig() const
{
    if (m_config.targetUpdatesPerSecond <= 0)
    {
        throw std::invalid_argument("targetUpdatesPerSecond must be > 0.");
    }

    if (m_config.maxFrameTimeSeconds <= 0.0)
    {
        throw std::invalid_argument("maxFrameTimeSeconds must be > 0.");
    }

    if (m_config.maxUpdatesPerFrame <= 0)
    {
        throw std::invalid_argument("maxUpdatesPerFrame must be > 0.");
    }
}