#pragma once

#include "Vec3.hpp"
#include "Quat.hpp"
#include <cmath>

inline float lengthSquared(const Vec3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length(const Vec3& v)
{
    return std::sqrt(lengthSquared(v));
}

inline Vec3 normalize(const Vec3& v)
{
    const float lenSq = lengthSquared(v);
    if (lenSq < 1e-24f)
        return {0.f, 0.f, 0.f};
    const float inv = 1.0f / std::sqrt(lenSq);
    return {v.x * inv, v.y * inv, v.z * inv};
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

inline float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Quat quatNormalize(const Quat& q)
{
    const float lenSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (lenSq < 1e-24f)
        return {0.f, 0.f, 0.f, 1.f};
    const float inv = 1.0f / std::sqrt(lenSq);
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

inline Quat quatMul(const Quat& a, const Quat& b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
