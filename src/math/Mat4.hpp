#pragma once

#include "Vec3.hpp"
#include "Quat.hpp"
#include <cmath>

struct Mat4 {
    float m[16]{};

    static Mat4 Identity() {
        Mat4 mat;
        mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
        return mat;
    }

    /// Column-major: rotation from quaternion (x,y,z,w), translation, scale.
    static Mat4 FromTRS(const Vec3& pos, const Quat& rot, const Vec3& scale) {
        Quat q = quatNormalize(rot);
        float x = q.x, y = q.y, z = q.z, w = q.w;
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;

        Mat4 mat = Identity();
        float sx = scale.x, sy = scale.y, sz = scale.z;

        mat.m[0]  = (1.0f - 2.0f * (yy + zz)) * sx;
        mat.m[1]  = (2.0f * (xy + wz)) * sx;
        mat.m[2]  = (2.0f * (xz - wy)) * sx;

        mat.m[4]  = (2.0f * (xy - wz)) * sy;
        mat.m[5]  = (1.0f - 2.0f * (xx + zz)) * sy;
        mat.m[6]  = (2.0f * (yz + wx)) * sy;

        mat.m[8]  = (2.0f * (xz + wy)) * sz;
        mat.m[9]  = (2.0f * (yz - wx)) * sz;
        mat.m[10] = (1.0f - 2.0f * (xx + yy)) * sz;

        mat.m[12] = pos.x;
        mat.m[13] = pos.y;
        mat.m[14] = pos.z;
        mat.m[15] = 1.0f;
        return mat;
    }

    static Mat4 FromTR(const Vec3& pos, const Quat& rot) {
        return FromTRS(pos, rot, {1, 1, 1});
    }

    static Mat4 FromTranslation(const Vec3& t) {
        Mat4 mat = Identity();
        mat.m[12] = t.x;
        mat.m[13] = t.y;
        mat.m[14] = t.z;
        return mat;
    }

    /// Vertical FOV in degrees, column-major, OpenGL NDC depth [-1,1]. Matches typical RH clip space.
    static Mat4 Perspective(float fovYDegrees, float aspect, float zNear, float zFar) {
        float rad = fovYDegrees * 3.14159265358979323846f / 180.0f;
        float tanHalf = std::tan(rad * 0.5f);
        if (tanHalf < 1e-8f || aspect < 1e-8f)
            return Identity();
        float h = 1.0f / tanHalf;
        float w = h / aspect;
        Mat4 r = Identity();
        r.m[0]  = w;
        r.m[5]  = h;
        r.m[10] = -(zFar + zNear) / (zFar - zNear);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        r.m[15] = 0.0f;
        return r;
    }

    static Mat4 FromScale(const Vec3& s) {
        Mat4 m = Identity();
        m.m[0]  = s.x;
        m.m[5]  = s.y;
        m.m[10] = s.z;
        return m;
    }

    Mat4 operator*(const Mat4& b) const {
        return mat4Mul(*this, b);
    }

    static Mat4 mat4Mul(const Mat4& a, const Mat4& b) {
        Mat4 o;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                o.m[c * 4 + r] =
                    a.m[0 * 4 + r] * b.m[c * 4 + 0] +
                    a.m[1 * 4 + r] * b.m[c * 4 + 1] +
                    a.m[2 * 4 + r] * b.m[c * 4 + 2] +
                    a.m[3 * 4 + r] * b.m[c * 4 + 3];
            }
        }
        return o;
    }

    /// Full 4x4 inverse (column-major). Returns identity on singular matrix.
    static Mat4 inverse(const Mat4& mat) {
        const float* m = mat.m;
        float inv[16];

        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        if (std::abs(det) < 1e-12f)
            return Identity();

        det = 1.0f / det;
        Mat4 out;
        for (int i = 0; i < 16; ++i)
            out.m[i] = inv[i] * det;
        return out;
    }

    static Vec3 transformPoint(const Mat4& mat, const Vec3& p) {
        float x = mat.m[0] * p.x + mat.m[4] * p.y + mat.m[8] * p.z + mat.m[12];
        float y = mat.m[1] * p.x + mat.m[5] * p.y + mat.m[9] * p.z + mat.m[13];
        float z = mat.m[2] * p.x + mat.m[6] * p.y + mat.m[10] * p.z + mat.m[14];
        float w = mat.m[3] * p.x + mat.m[7] * p.y + mat.m[11] * p.z + mat.m[15];
        if (std::abs(w) > 1e-8f) {
            float invW = 1.0f / w;
            return {x * invW, y * invW, z * invW};
        }
        return {x, y, z};
    }

    static Vec3 transformDirection(const Mat4& mat, const Vec3& d) {
        float x = mat.m[0] * d.x + mat.m[4] * d.y + mat.m[8] * d.z;
        float y = mat.m[1] * d.x + mat.m[5] * d.y + mat.m[9] * d.z;
        float z = mat.m[2] * d.x + mat.m[6] * d.y + mat.m[10] * d.z;
        return {x, y, z};
    }

    /// Right-handed view matrix (same convention as typical GLM `lookAtRH`): world → view.
    static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& worldUp) {
        Vec3 f = normalize(Vec3{center.x - eye.x, center.y - eye.y, center.z - eye.z});
        Vec3 upN = normalize(worldUp);
        Vec3 s = normalize(cross(f, upN));
        if (lengthSquared(s) < 1e-10f) {
            Vec3 altUp = (std::abs(upN.y) > 0.99f) ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
            s = normalize(cross(f, altUp));
        }
        Vec3 u = cross(s, f);

        Mat4 m = Identity();
        m.m[0]  = s.x;
        m.m[1]  = u.x;
        m.m[2]  = -f.x;
        m.m[3]  = 0.0f;
        m.m[4]  = s.y;
        m.m[5]  = u.y;
        m.m[6]  = -f.y;
        m.m[7]  = 0.0f;
        m.m[8]  = s.z;
        m.m[9]  = u.z;
        m.m[10] = -f.z;
        m.m[11] = 0.0f;
        m.m[12] = -dot(s, eye);
        m.m[13] = -dot(u, eye);
        m.m[14] = dot(f, eye);
        m.m[15] = 1.0f;
        return m;
    }
};

inline Mat4 mat4Mul(const Mat4& a, const Mat4& b) {
    return Mat4::mat4Mul(a, b);
}
