#pragma once

#include <vector>

#include "../ecs/Registry.hpp"
#include "../components/SkeletonComponent.hpp"
#include "../components/PoseComponent.hpp"
#include "../math/Mat4.hpp"

class PoseSystem
{
public:
    void update(Registry& registry)
    {
        auto entities = registry.getEntitiesWith<SkeletonComponent, PoseComponent>();

        for (auto e : entities) {
            auto& skeleton = registry.getComponent<SkeletonComponent>(e);
            auto& pose = registry.getComponent<PoseComponent>(e);

            if (!pose.dirty)
                continue;

            const size_t n = skeleton.bones.size();
            if (pose.localPose.size() < n)
                pose.localPose.resize(n);
            pose.worldMatrix.resize(n);

            std::vector<Mat4> locals(n);
            for (size_t i = 0; i < n; ++i) {
                locals[i] = Mat4::FromTRS(
                    pose.localPose[i].position,
                    pose.localPose[i].rotation,
                    pose.localPose[i].scale);
            }

            // glTF bone order is not guaranteed parent-before-child; resolve in passes.
            std::vector<char> done(n, 0);
            for (size_t round = 0; round < n; ++round) {
                bool any = false;
                for (size_t i = 0; i < n; ++i) {
                    if (done[i])
                        continue;
                    const int p = skeleton.bones[i].parentIndex;
                    if (p < 0) {
                        pose.worldMatrix[i] = locals[i];
                        done[i] = 1;
                        any = true;
                    } else if (static_cast<size_t>(p) < n && done[static_cast<size_t>(p)]) {
                        pose.worldMatrix[i] = mat4Mul(pose.worldMatrix[static_cast<size_t>(p)], locals[i]);
                        done[i] = 1;
                        any = true;
                    }
                }
                if (!any)
                    break;
            }
            for (size_t i = 0; i < n; ++i) {
                if (!done[i])
                    pose.worldMatrix[i] = locals[i];
            }

            pose.dirty = false;
        }
    }
};
