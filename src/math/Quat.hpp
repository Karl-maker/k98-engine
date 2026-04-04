#pragma once

#include <cmath>
#include <algorithm>

struct Quat {
    float x{0}, y{0}, z{0}, w{1};

    static Quat Identity() { return {0, 0, 0, 1}; }
};

inline float quatDot(const Quat& a, const Quat& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline float quatLengthSq(const Quat& q) {
    return quatDot(q, q);
}

inline Quat quatNormalize(const Quat& q) {
    float lenSq = quatLengthSq(q);
    if (lenSq < 1e-12f)
        return Quat::Identity();
    float inv = 1.0f / std::sqrt(lenSq);
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

inline Quat quatMul(const Quat& a, const Quat& b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

inline Quat quatSlerp(const Quat& a, const Quat& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float cosHalf = quatDot(a, b);
    Quat b2 = b;
    if (cosHalf < 0.0f) {
        cosHalf = -cosHalf;
        b2 = {-b.x, -b.y, -b.z, -b.w};
    }
    if (cosHalf > 0.9995f) {
        Quat r{
            a.x + t * (b2.x - a.x),
            a.y + t * (b2.y - a.y),
            a.z + t * (b2.z - a.z),
            a.w + t * (b2.w - a.w)
        };
        return quatNormalize(r);
    }
    float half = std::acos(cosHalf);
    float sinHalf = std::sin(half);
    float invSin = 1.0f / sinHalf;
    float aW = std::sin((1.0f - t) * half) * invSin;
    float bW = std::sin(t * half) * invSin;
    return quatNormalize(Quat{
        a.x * aW + b2.x * bW,
        a.y * aW + b2.y * bW,
        a.z * aW + b2.z * bW,
        a.w * aW + b2.w * bW
    });
}
