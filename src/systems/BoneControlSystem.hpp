#pragma once

#include "../ecs/Registry.hpp"
#include "../components/BoneControlComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../math/MathOps.hpp"

class BoneControlSystem
{
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<BoneControlComponent, PoseComponent>();

        for (auto e : entities) {
            auto& control = registry.getComponent<BoneControlComponent>(e);
            auto& pose = registry.getComponent<PoseComponent>(e);

            for (auto& [boneIndex, transform] : control.overrides) {
                if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.localPose.size())
                    continue;
                if (control.additive) {
                    auto& lp = pose.localPose[static_cast<size_t>(boneIndex)];
                    lp.position.x += transform.position.x;
                    lp.position.y += transform.position.y;
                    lp.position.z += transform.position.z;
                    lp.rotation = quatNormalize(quatMul(lp.rotation, transform.rotation));
                    lp.scale.x *= transform.scale.x;
                    lp.scale.y *= transform.scale.y;
                    lp.scale.z *= transform.scale.z;
                } else {
                    pose.localPose[static_cast<size_t>(boneIndex)] = transform;
                }
            }

            pose.dirty = true;
        }
    }
};
