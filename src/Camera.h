#pragma once

#include "Math/Mat4.h"

class Camera {
protected:
    Mat4 projectionMatrix;
    Mat4 viewMatrix;
    float screenWidth;
    float screenHeight;
    float aspectRatio;

public:
    const Mat4 &getProjectionMatrix() const { return projectionMatrix; }
    const Mat4 &getViewMatrix() const { return viewMatrix; }

    void setScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
        aspectRatio = (float)width / (float)height;
    }

    virtual bool handleSDLEvent(union SDL_Event *event) = 0;
    virtual void update() = 0;
};

class TopDownCamera : public Camera {
    Vec3 focus = Vec3::zero();
    float dist = 100;
    float pitch = 45;
    float yaw = 45;
    bool rotating = false;
    bool orthogonal = false;

public:
    virtual bool handleSDLEvent(union SDL_Event *event) override;
    virtual void update() override;
};
