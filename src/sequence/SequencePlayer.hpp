#pragma once
#include <memory>
#include "Sequence.hpp"

class SequencePlayer {
public:
    std::shared_ptr<Sequence> sequence;

    float time{0.0f};
    bool playing{false};
    bool finished{false};

    void play()
    {
        if (!sequence) return;

        time = 0.0f;
        playing = true;
        finished = false;
        sequence->reset();
    }

    void update(float dt)
    {
        if (!playing || !sequence) return;

        time += dt;

        sequence->update(time);

        if (time >= sequence->duration) {
            playing = false;
            finished = true;
        }
    }
};