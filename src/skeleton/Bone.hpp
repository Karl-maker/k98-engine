#pragma once

#include <string>
#include "../math/Vec3.hpp"
#include "../math/Quat.hpp"
#include "../math/Mat4.hpp"

/// Runtime skeleton bone (ECS). Distinct from `ModelAsset::Bone` (importer POD).
struct SkeletonBone {
    std::string name;
    int parentIndex = -1;

    Vec3 localPosition{};
    Quat localRotation{0.f, 0.f, 0.f, 1.f};
    Vec3 localScale{1.f, 1.f, 1.f};

    Vec3 bindPosition{};
    Quat bindRotation{0.f, 0.f, 0.f, 1.f};
    Vec3 bindScale{1.f, 1.f, 1.f};

    Mat4 inverseBind = Mat4::Identity();
};
