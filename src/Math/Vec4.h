#pragma once

#include "Vec3.h"

struct Vec4 {
    float x, y, z, w;

    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(Vec3 v, float w = 0) : x(v.x), y(v.y), z(v.z), w(w) {}
    Vec4(Vec2 v, float z = 0, float w = 0) : x(v.x), y(v.y), z(z), w(w) {}
    Vec4(float x, float y, float z, float w = 0) : x(x), y(y), z(z), w(w) {}
    
    float &operator[](int i) { return (&x)[i]; }
    const float &operator[](int i) const { return (&x)[i]; }

    Vec4 operator-() const { return Vec4(-x, -y, -z, -w); }

    void debugPrint(const char *message) {
        printf("%s: Vec4(%.2f, %.2f, %.2f, %.2f)\n", message, x, y, z, w);
    }
    
    Vec4 withAxis(int axis, float val) const { Vec4 v = *this; v[axis] = val; return v; }
    Vec4 withX(float x) const { Vec4 v = *this; v.x = x; return v; }
    Vec4 withY(float x) const { Vec4 v = *this; v.y = y; return v; }
    Vec4 withZ(float x) const { Vec4 v = *this; v.z = z; return v; }
    Vec4 withW(float x) const { Vec4 v = *this; v.w = w; return v; }

    Vec2 xy() { return Vec2(x, y); }
    Vec3 xyz() { return Vec3(x, y, z); }

    static Vec4 zero() { return Vec4(0, 0, 0, 0); }
    static Vec4 unitX() { return Vec4(1, 0, 0, 0); }
    static Vec4 unitY() { return Vec4(0, 1, 0, 0); }
    static Vec4 unitZ() { return Vec4(0, 0, 1, 0); }
    static Vec4 unitW() { return Vec4(0, 0, 0, 1); }
    static Vec4 one() { return Vec4(1, 1, 1, 1); }
};

inline Vec4 operator +(Vec4 a, Vec4 b) { return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline Vec4 operator -(Vec4 a, Vec4 b) { return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
inline Vec4 operator *(Vec4 v, float f) { return Vec4(v.x * f, v.y * f, v.z * f, v.w * f); }
inline Vec4 operator *(float f, Vec4 v) { return Vec4(f * v.x, f * v.y, f * v.z, f * v.w); }
inline Vec4 operator *(Vec4 a, Vec4 b) { return Vec4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
inline Vec4 operator /(Vec4 a, Vec4 b) { return Vec4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
inline Vec4 operator /(Vec4 v, float f) { return v * (1.f / f); }

inline float dot(Vec4 a, Vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float sqrLength(Vec4 v) { return dot(v, v); }
inline float length(Vec4 v) { return sqrtf(sqrLength(v)); }
inline Vec4 normalize(Vec4 v) { return v / length(v); }
inline Vec4 abs(Vec4 v) { return Vec4(fabs(v.x), fabs(v.y), fabs(v.z), fabs(v.w)); }
inline Vec4 inverse(Vec4 v) { return Vec4(1.f / v.x, 1.f / v.y, 1.f / v.z, 1.f / v.w); }
inline float angle(Vec4 a, Vec4 b) { return acosf(dot(a, b) / (length(a) * length(b))); }
