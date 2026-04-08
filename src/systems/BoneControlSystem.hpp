#pragma once

#include "../components/BoneControlComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../ecs/Registry.hpp"
#include "../math/MathOps.hpp"

class BoneControlSystem {
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<BoneControlComponent, PoseComponent>();

        for (auto e : entities) {
            auto& control = registry.getComponent<BoneControlComponent>(e);
            auto& pose = registry.getComponent<PoseComponent>(e);

            for (auto& [boneIndex, entry] : control.overrides) {
                if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= pose.localPose.size())
                    continue;
                const float w = entry.blendWeight;
                if (w <= 1e-8f)
                    continue;

                const BoneTransform& t = entry.value;
                auto& lp = pose.localPose[static_cast<size_t>(boneIndex)];

                if (control.additive) {
                    lp.position.x += t.position.x * w;
                    lp.position.y += t.position.y * w;
                    lp.position.z += t.position.z * w;
                    const Quat id{0.f, 0.f, 0.f, 1.f};
                    const Quat deltaRot = quatSlerp(id, t.rotation, w);
                    lp.rotation = quatNormalize(quatMul(lp.rotation, deltaRot));
                    lp.scale.x *= 1.f + w * (t.scale.x - 1.f);
                    lp.scale.y *= 1.f + w * (t.scale.y - 1.f);
                    lp.scale.z *= 1.f + w * (t.scale.z - 1.f);
                } else {
                    lp.position = vec3Lerp(lp.position, t.position, w);
                    lp.rotation = quatSlerp(lp.rotation, t.rotation, w);
                    lp.scale = vec3Lerp(lp.scale, t.scale, w);
                }
            }

            pose.dirty = true;
        }
    }
};
