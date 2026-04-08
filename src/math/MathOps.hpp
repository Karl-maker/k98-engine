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

inline Vec3 vec3Lerp(const Vec3& a, const Vec3& b, float t)
{
    if (t < 0.f)
        t = 0.f;
    else if (t > 1.f)
        t = 1.f;
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t};
}

inline Quat quatNormalize(const Quat& q)
{
    const float lenSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (lenSq < 1e-24f)
        return {0.f, 0.f, 0.f, 1.f};
    const float inv = 1.0f / std::sqrt(lenSq);
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

/// Unit quaternion: rotation by `radians` around axis (axis need not be pre-normalized).
inline Quat quatAxisAngle(Vec3 axis, float radians)
{
    const Vec3 n = normalize(axis);
    if (lengthSquared(n) < 1e-12f)
        return {0.f, 0.f, 0.f, 1.f};
    const float h = radians * 0.5f;
    const float s = std::sin(h);
    return quatNormalize({n.x * s, n.y * s, n.z * s, std::cos(h)});
}

inline Quat quatMul(const Quat& a, const Quat& b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

/// Unit quaternion inverse (same as conjugate).
inline Quat quatInverseUnit(const Quat& q)
{
    return {-q.x, -q.y, -q.z, q.w};
}

/// Minimal rotation (unit quat) aligning `from` to `to` (both unit length).
inline Quat quatRotationBetweenUnit(const Vec3& from, const Vec3& to)
{
    const float d = dot(from, to);
    if (d > 0.999999f)
        return {0.f, 0.f, 0.f, 1.f};
    if (d < -0.999999f) {
        Vec3 orth = cross({1.f, 0.f, 0.f}, from);
        if (lengthSquared(orth) < 1e-12f)
            orth = cross({0.f, 1.f, 0.f}, from);
        orth = normalize(orth);
        return {orth.x, orth.y, orth.z, 0.f};
    }
    const Vec3 c = cross(from, to);
    return quatNormalize({c.x, c.y, c.z, 1.f + d});
}

/// Spherical linear interpolation between unit quaternions (`t` in [0, 1]).
inline Quat quatSlerp(Quat a, Quat b, float t)
{
    if (t < 0.f)
        t = 0.f;
    else if (t > 1.f)
        t = 1.f;
    float cosHalf = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (cosHalf < 0.f) {
        b = {-b.x, -b.y, -b.z, -b.w};
        cosHalf = -cosHalf;
    }
    if (cosHalf > 0.9995f) {
        return quatNormalize({
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z),
            a.w + t * (b.w - a.w)});
    }
    const float half0 = std::acos(cosHalf);
    const float sinHalf0 = std::sin(half0);
    const float w1 = std::sin((1.f - t) * half0) / sinHalf0;
    const float w2 = std::sin(t * half0) / sinHalf0;
    return quatNormalize({
        a.x * w1 + b.x * w2,
        a.y * w1 + b.y * w2,
        a.z * w1 + b.z * w2,
        a.w * w1 + b.w * w2});
}
