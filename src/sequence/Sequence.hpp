#pragma once
#include <vector>
#include "SequenceAction.hpp"

class Sequence {
public:
    float duration{0.0f};
    std::vector<SequenceAction> actions;

    void reset()
    {
        for (auto& a : actions) {
            a.executed = false;
        }
    }

    void update(float time)
    {
        for (auto& a : actions) {
            if (!a.executed && time >= a.time) {
                if (a.action) {
                    a.action(a.target);
                }
                a.executed = true;
            }
        }
    }
};