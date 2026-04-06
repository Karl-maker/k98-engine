#pragma once
#include "../ecs/Registry.hpp"
#include "../components/SequenceComponent.hpp"

class SequenceSystem {
public:
    void update(Registry& registry, float dt)
    {
        auto entities = registry.getEntitiesWith<SequenceComponent>();

        for (auto e : entities) {
            auto& seq = registry.getComponent<SequenceComponent>(e);

            bool wasPlaying = seq.player.playing;

            seq.player.update(dt);

            if (wasPlaying && seq.player.finished) {
                onFinished(e, registry, seq);
            }
        }
    }

private:
    void onFinished(Entity e, Registry& registry, SequenceComponent& seq)
    {
        (void)e;
        if (seq.onFinished)
            seq.onFinished();
    }
};