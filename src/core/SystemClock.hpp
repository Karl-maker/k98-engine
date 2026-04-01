#pragma once

#include "IClock.hpp"

#include <chrono>
#include <thread>

class SystemClock final : public IClock
{
public:
    double nowSeconds() const override
    {
        using Clock = std::chrono::steady_clock;
        const auto now = Clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }

    void sleepSeconds(double seconds) const override
    {
        if (seconds <= 0.0)
        {
            return;
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    }
};