#pragma once

#include "Vec2.h"

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(Vec2 v, float z = 0) : x(v.x), y(v.y), z(z) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    float &operator[](int i) { return (&x)[i]; }
    const float &operator[](int i) const { return (&x)[i]; }
    
    Vec3 withAxis(int axis, float val) const { Vec3 v = *this; v[axis] = val; return v; }
    Vec3 withX(float x) const { Vec3 v = *this; v.x = x; return v; }
    Vec3 withY(float x) const { Vec3 v = *this; v.y = y; return v; }
    Vec3 withZ(float x) const { Vec3 v = *this; v.z = z; return v; }

    Vec2 xy() { return Vec2(x, y); }

    Vec3 &operator+=(Vec3 v);
    Vec3 &operator-=(Vec3 v);

    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    void debugPrint(const char *message) {
        printf("%s: Vec3(%.2f, %.2f, %.2f)\n", message, x, y, z);
    }
    
    static Vec3 zero() { return Vec3(0, 0, 0); }
    static Vec3 unitX() { return Vec3(1, 0, 0); }
    static Vec3 unitY() { return Vec3(0, 1, 0); }
    static Vec3 unitZ() { return Vec3(0, 0, 1); }
    static Vec3 one() { return Vec3(1, 1, 1); }
};

inline Vec3 operator +(Vec3 a, Vec3 b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator -(Vec3 a, Vec3 b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator *(Vec3 v, float f) { return Vec3(v.x * f, v.y * f, v.z * f); }
inline Vec3 operator *(float f, Vec3 v) { return Vec3(f * v.x, f * v.y, f * v.z); }
inline Vec3 operator *(Vec3 a, Vec3 b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3 operator /(Vec3 a, Vec3 b) { return Vec3(a.x / b.x, a.y / b.y, a.z / b.z); }
inline Vec3 operator /(Vec3 v, float f) { return v * (1.f / f); }

inline Vec3 &Vec3::operator+=(Vec3 v) { *this = *this + v; return *this; }
inline Vec3 &Vec3::operator-=(Vec3 v) { *this = *this - v; return *this; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float sqrLength(Vec3 v) { return dot(v, v); }
inline float length(Vec3 v) { return sqrtf(sqrLength(v)); }
inline Vec3 normalize(Vec3 v) { return v / length(v); }
inline Vec3 abs(Vec3 v) { return Vec3(fabs(v.x), fabs(v.y), fabs(v.z)); }
inline Vec3 inverse(Vec3 v) { return Vec3(1.f / v.x, 1.f / v.y, 1.f / v.z); }
inline float angle(Vec3 a, Vec3 b) { return acosf(dot(a, b) / (length(a) * length(b))); }
inline Vec3 cross(Vec3 a, Vec3 b) { return Vec3(a.y * b.z - b.y * a.z,
                                                a.z * b.x - b.z * a.x,
                                                a.x * b.y - b.x * a.y); }
