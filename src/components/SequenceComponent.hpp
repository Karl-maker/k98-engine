#pragma once
#include "../sequence/SequencePlayer.hpp"
#include <functional>

struct SequenceComponent {
    SequencePlayer player;

    /// Optional: called once when `player` transitions from playing to finished (demo / gameplay hooks).
    std::function<void()> onFinished;
};