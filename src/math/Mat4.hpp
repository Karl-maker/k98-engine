#pragma once
#include "Vec3.hpp"
#include "Quat.hpp"

struct Mat4 {
    float m[16]{};

    static Mat4 Identity() {
        Mat4 mat;
        mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
        return mat;
    }

    static Mat4 FromTR(const Vec3& pos, const Quat& rot) {
        Mat4 mat = Identity();
        // Simplified (no real quaternion math for brevity)
        mat.m[12] = pos.x;
        mat.m[13] = pos.y;
        mat.m[14] = pos.z;
        return mat;
    }
};