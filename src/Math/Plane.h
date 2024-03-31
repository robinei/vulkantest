#pragma once

#include "Vec3.h"

struct Plane {
    Vec3 n;
    float d;

    Plane(Vec3 n, float d = 0) : n(n), d(d) {}

    Vec3 rayIntersect(Vec3 p0, Vec3 p1) {
        Vec3 v = normalize(p1 - p0);
        float t = -(dot(p0, n) + d) / dot(v, n);
        return p0 + v * t;
    }

    Vec3 project(Vec3 v) {
        return v - n * dot(v, n);
    }

    static Plane XY(float d = 0) { return Plane(Vec3(0, 0, 1), d); }
    static Plane XZ(float d = 0) { return Plane(Vec3(0, 1, 0), d); }
    static Plane YZ(float d = 0) { return Plane(Vec3(1, 0, 0), d); }
};
