#pragma once

#include "Mat4.h"

struct Quat;

inline Quat operator *(Quat a, Quat b);
inline Quat normalize(Quat q);

struct Quat {
    float x, y, z, w;

    Vec3 xyz() { return Vec3(x, y, z); }

    Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { }

    Quat(Vec3 v, float w) : x(v.x), y(v.y), z(v.z), w(w) { }

    float &operator[](int i) { return (&x)[i]; }
    const float &operator[](int i) const { return (&x)[i]; }

    void debugPrint(const char *message) {
        printf("%s: Quat(%.2f, %.2f, %.2f, %.2f)\n", message, x, y, z, w);
    }
    
    void toMatrix(Mat4 &m) const {
        float x2  = 2.f * x;
        float y2  = 2.f * y;
        float z2  = 2.f * z;
        float xw2 = x2 * w;
        float yw2 = y2 * w;
        float zw2 = z2 * w;
        float xx2 = x2 * x;
        float xy2 = y2 * x;
        float xz2 = z2 * x;
        float yy2 = y2 * y;
        float yz2 = z2 * y;
        float zz2 = z2 * z;

        m[0] = 1.f - yy2 - zz2;
        m[4] = xy2 - zw2;
        m[8] = xz2 + yw2;
        m[12] = 0.f;

        m[1] = xy2 + zw2;
        m[5] = 1.f - xx2 - zz2;
        m[9] = yz2 - xw2;
        m[13] = 0.f;

        m[2] = xz2 - yw2;
        m[6] = yz2 + xw2;
        m[10] = 1.f - xx2 - yy2;
        m[14] = 0.f;

        m[3] = 0.f;
        m[7] = 0.f;
        m[11] = 0.f;
        m[15] = 1.f;
    }
    
    static Quat fromEulerAngles(float x, float y, float z) {
        x *= 0.5f * (M_PI / 180.0f);
        y *= 0.5f * (M_PI / 180.0f);
        z *= 0.5f * (M_PI / 180.0f);
        Quat qx = Quat(std::sin(x), 0, 0, std::cos(x));
        Quat qy = Quat(0, std::sin(y), 0, std::cos(y));
        Quat qz = Quat(0, 0, std::sin(z), std::cos(z));
        return qx * qy * qz;
    }

    static Quat fromEulerAngles(Vec3 v) {
        return fromEulerAngles(v.x, v.y, v.z);
    }

    static Quat fromAxisAngle(float x, float y, float z, float angle) {
        return fromAxisAngle(Vec3(x, y, x), angle);
    }

    static Quat fromAxisAngle(Vec3 axis, float angle) {
        float a = 0.5f * angle * (M_PI / 180.0f);
		return Quat(axis * std::sin(a), std::cos(a));
    }

    static Quat fromArc(Vec3 from, Vec3 to) {
        return normalize(Quat(cross(from, to), dot(from, to) +
                              sqrtf(sqrLength(from) * sqrLength(to))));
    }
    
    static Quat identity() { return Quat(0, 0, 0, 1); }
};

inline Quat operator +(Quat a, Quat b) { return Quat(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline Quat operator -(Quat a, Quat b) { return Quat(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
inline Quat operator *(Quat q, float f) { return Quat(q.x * f, q.y * f, q.z * f, q.w * f); }

inline Quat operator *(Quat a, Quat b) {
    return Quat(a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
                a.w*b.y + a.y*b.w + a.z*b.x - a.x*b.z,
                a.w*b.z + a.z*b.w + a.x*b.y - a.y*b.x,
                a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z);
}

inline float dot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float sqrLength(Quat q) { return dot(q, q); }
inline float length(Quat q) { return sqrtf(sqrLength(q)); }
inline Quat normalize(Quat q) { return q * (1.f / length(q)); }
inline Quat conjugate(Quat q) { return Quat(-q.x, -q.y, -q.z, q.w); }

inline Vec3 operator *(Quat q, Vec3 v) {
    Vec3 qvec = q.xyz();
    Vec3 uv(cross(qvec, v));
    Vec3 uuv(cross(qvec, uv));
    return v + ((uv * q.w) + uuv) * 2.0f;
}

inline Quat slerp(Quat q0, Quat q1, float t) {
    // q0 and q1 should be unit length or else
    // something broken will happen.

    // Compute the cosine of the angle between the two vectors.
    float d = dot(q0, q1);

    if(d > 0.9995f) {
        // If the inputs are too close for comfort, linearly interpolate
        // and normalize the result.
        return normalize(q0 + (q1 - q0) * t);
    }

    d = clamp(d, -1.f, 1.f);   // Robustness: Stay within domain of acos()
    float theta0 = acosf(d);   // theta_0 = angle between input vectors
    float theta = theta0 * t;  // theta = angle between q0 and result 

    Quat q2 = normalize(q1 - q0 * d);

    // { q0, q2 } is now an orthonormal basis
    return q0 * cosf(theta) + q2 * sinf(theta);
}
    
