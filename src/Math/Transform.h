#pragma once

#include "Quat.h"

struct Transform {
    Quat rotation;
    Vec3 translation;
    Vec3 scale;

    Vec3 applyForward(const Vec3 &localpos) const {
        return rotation * (scale * localpos) + translation;
    }

    Vec3 applyInverse(const Vec3 &worldpos) const {
        return inverse(scale) * (conjugate(rotation) * (worldpos - translation));
    }

    void product(const Transform &a, const Transform &b) {
        rotation = a.rotation * b.rotation;
        scale = a.scale * b.scale;
        translation = a.applyForward(b.translation);
    }

    void make_identity() {
        rotation = Quat::identity();
        translation = Vec3::zero();
        scale = Vec3::one();
    }

    void toMatrix(Mat4 &m) const {
        rotation.toMatrix(m);

        m[12] = translation.x;
        m[13] = translation.y;
        m[14] = translation.z;

        m[0] *= scale.x;
        m[1] *= scale.x;
        m[2] *= scale.x;
        
        m[4] *= scale.y;
        m[5] *= scale.y;
        m[6] *= scale.y;
        
        m[8] *= scale.z;
        m[9] *= scale.z;
        m[10] *= scale.z;
    }

    void getAxes(Vec3 &ax, Vec3 &ay, Vec3 &az) const {
        Mat4 m;
        rotation.toMatrix(m);
        ax = Vec3(m[0], m[1], m[2]);
        ay = Vec3(m[4], m[5], m[6]);
        az = Vec3(m[8], m[9], m[10]);
    }
};
