#pragma once

#include "Vec4.h"
#include <cstring>

struct Mat4 {
    /*
    0  4  8 12
    1  5  9 13
    2  6 10 14
    3  7 11 15
    */

    union {
        float m[16];
        Vec4 v[4];
    };

    Mat4() {
        toDiagonal(0);
    }

    float &operator[](int i) { return m[i]; }
    const float &operator[](int i) const { return m[i]; }

    Vec4 operator *(Vec4 v) {
        return Vec4(
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        );
    }

    Vec3 operator *(Vec3 v) {
        return Vec3(
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]
        );
    }

    void debugPrint(const char *message) {
        printf("%s:\n   Mat4(%.2f, %.2f, %.2f, %.2f\n", message, v[0][0], v[1][0], v[2][0], v[3][0]);
        printf("        %.2f, %.2f, %.2f, %.2f\n", v[0][1], v[1][1], v[2][1], v[3][1]);
        printf("        %.2f, %.2f, %.2f, %.2f\n", v[0][2], v[1][2], v[2][2], v[3][2]);
        printf("        %.2f, %.2f, %.2f, %.2f)\n", v[0][3], v[1][3], v[2][3], v[3][3]);
    }

    void toProduct(const Mat4 &a, const Mat4 &b) {
        v[0] = a.v[0] * b.v[0][0] + a.v[1] * b.v[0][1] + a.v[2] * b.v[0][2] + a.v[3] * b.v[0][3];
        v[1] = a.v[0] * b.v[1][0] + a.v[1] * b.v[1][1] + a.v[2] * b.v[1][2] + a.v[3] * b.v[1][3];
        v[2] = a.v[0] * b.v[2][0] + a.v[1] * b.v[2][1] + a.v[2] * b.v[2][2] + a.v[3] * b.v[2][3];
        v[3] = a.v[0] * b.v[3][0] + a.v[1] * b.v[3][1] + a.v[2] * b.v[3][2] + a.v[3] * b.v[3][3];
    }

    void toDiagonal(float s) {
        v[0][0] = s;
        v[0][1] = 0;
        v[0][2] = 0;
        v[0][3] = 0;

        v[1][0] = 0;
        v[1][1] = s;
        v[1][2] = 0;
        v[1][3] = 0;

        v[2][0] = 0;
        v[2][1] = 0;
        v[2][2] = s;
        v[2][3] = 0;

        v[3][0] = 0;
        v[3][1] = 0;
        v[3][2] = 0;
        v[3][3] = s;
    }

    void toIdentity() {
        toDiagonal(1.0f);
    }

    void transpose() {
        swap(v[1][0], v[0][1]);
        swap(v[2][0], v[0][2]);
        swap(v[3][0], v[0][3]);
        swap(v[2][1], v[1][2]);
        swap(v[3][1], v[1][3]);
        swap(v[3][2], v[2][3]);
    }

    void toLookAt(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f(normalize(center - eye));
        Vec3 s(normalize(cross(f, up)));
        Vec3 u(cross(s, f));

        toIdentity();
        v[0][0] = s.x;
        v[1][0] = s.y;
        v[2][0] = s.z;
        v[0][1] = u.x;
        v[1][1] = u.y;
        v[2][1] = u.z;
        v[0][2] = -f.x;
        v[1][2] = -f.y;
        v[2][2] = -f.z;
        v[3][0] = -dot(s, eye);
        v[3][1] = -dot(u, eye);
        v[3][2] = dot(f, eye);
    }

    void toPerspective(float fovy, float aspect, float zNear, float zFar) {
        float tanHalfFovy = tan(0.5f * fovy * M_PI / 180.0f);
        v[0][0] = 1.0f / (aspect * tanHalfFovy);
        v[1][1] = 1.0f / (tanHalfFovy);
        v[2][2] = -(zFar + zNear) / (zFar - zNear);
        v[2][3] = -1.0f;
        v[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
    }

    void toOrtho(float left, float right, float bottom, float top, float zNear, float zFar) {
        toIdentity();
		v[0][0] = 2.0f / (right - left);
		v[1][1] = 2.0f / (top - bottom);
		v[2][2] = - 2.0f / (zFar - zNear);
		v[3][0] = - (right + left) / (right - left);
		v[3][1] = - (top + bottom) / (top - bottom);
		v[3][2] = - (zFar + zNear) / (zFar - zNear);
    }

private:
    static inline void swap(float &a, float &b) {
        float temp = a;
        a = b;
        b = temp;
    }
};
