#pragma once

#include "Math/Mat4.h"
#include "Math/Quat.h"

class FPSCamera {
public:
    FPSCamera() {
        up = Vec3(0, 1, 0);
    }

    void setPos(Vec3 pos) {
        pos = pos;
    }

    void setDir(Vec3 dir) {
        dir = dir;
    }

    void lookAt(Vec3 target) {
        dir = normalize(target - pos);
    }

    Vec3 getPos() {
        return pos;
    }

    Vec3 getDir() {
        return dir;
    }

    void step(float amount) {
        pos += dir * amount;
    }

    void strafe(float amount) {
        Vec3 side = normalize(cross(dir, up));
        pos += side * amount;
    }

    void rise(float amount) {
        Vec3 side = normalize(cross(dir, up));
        Vec3 up = normalize(cross(side, dir));
        pos += up * amount;
    }

    void rotate(float yaw, float pitch) {
        Vec3 side = normalize(cross(dir, up));
        dir = Quat::fromAxisAngle(up, yaw) * dir;
        dir = Quat::fromAxisAngle(side, pitch) * dir;
    }

    void viewMatrix(Mat4 m) {
        m.lookAt(pos, pos + dir, up);
    }

private:
    Vec3 up;
    Vec3 pos;
    Vec3 dir;
};
