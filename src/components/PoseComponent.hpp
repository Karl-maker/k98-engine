#pragma once

#include <vector>
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

struct BoneTransform
{
    Vec3 position{};
    Quat rotation{0.f, 0.f, 0.f, 1.f};
    Vec3 scale{1.f, 1.f, 1.f};
};

struct PoseComponent
{
    std::vector<BoneTransform> localPose;
    std::vector<Mat4> worldMatrix;
    bool dirty = true;
};
