#pragma once

#include <cassert>
#include <cmath>
#include <cstdio>

struct Vec2 {
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}

    float &operator[](int i) { return (&x)[i]; }
    const float &operator[](int i) const { return (&x)[i]; }
    
    Vec2 withAxis(int axis, float val) const { Vec2 v = *this; v[axis] = val; return v; }
    Vec2 withX(float x) const { Vec2 v = *this; v.x = x; return v; }
    Vec2 withY(float x) const { Vec2 v = *this; v.y = y; return v; }

    Vec2 &operator+=(Vec2 v);
    Vec2 &operator-=(Vec2 v);

    Vec2 operator-() const { return Vec2(-x, -y); }

    void debugPrint(const char *message) {
        printf("%s: Vec2(%.2f, %.2f)\n", message, x, y);
    }

    static Vec2 zero() { return Vec2(0, 0); }
    static Vec2 unitX() { return Vec2(1, 0); }
    static Vec2 unitY() { return Vec2(0, 1); }
    static Vec2 one() { return Vec2(1, 1); }
};

inline Vec2 operator +(Vec2 a, Vec2 b) { return Vec2(a.x + b.x, a.y + b.y); }
inline Vec2 operator -(Vec2 a, Vec2 b) { return Vec2(a.x - b.x, a.y - b.y); }
inline Vec2 operator *(Vec2 v, float f) { return Vec2(v.x * f, v.y * f); }
inline Vec2 operator *(float f, Vec2 v) { return Vec2(f * v.x, f * v.y); }
inline Vec2 operator *(Vec2 a, Vec2 b) { return Vec2(a.x * b.x, a.y * b.y); }
inline Vec2 operator /(Vec2 a, Vec2 b) { return Vec2(a.x / b.x, a.y / b.y); }
inline Vec2 operator /(Vec2 v, float f) { return v * (1.f / f); }

inline Vec2 &Vec2::operator+=(Vec2 v) { *this = *this + v; return *this; }
inline Vec2 &Vec2::operator-=(Vec2 v) { *this = *this - v; return *this; }

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float sqrLength(Vec2 v) { return dot(v, v); }
inline float length(Vec2 v) { return sqrtf(sqrLength(v)); }
inline Vec2 normalize(Vec2 v) { return v / length(v); }
inline Vec2 abs(Vec2 v) { return Vec2(fabs(v.x), fabs(v.y)); }
inline Vec2 inverse(Vec2 v) { return Vec2(1.f / v.x, 1.f / v.y); }
inline float angle(Vec2 a, Vec2 b) { return acosf(dot(a, b) / (length(a) * length(b))); }

inline float clamp(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

inline Vec2 clamp(Vec2 v, Vec2 min, Vec2 max) {
    v.x = clamp(v.x, min.x, max.x);
    v.y = clamp(v.y, min.y, max.y);
    return v;
}
