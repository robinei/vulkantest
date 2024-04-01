#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

class Camera {
protected:
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    float screenWidth;
    float screenHeight;
    float aspectRatio;

public:
    const glm::mat4 &getProjectionMatrix() const { return projectionMatrix; }
    const glm::mat4 &getViewMatrix() const { return viewMatrix; }

    void setScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
        aspectRatio = (float)width / (float)height;
    }

    virtual bool handleSDLEvent(union SDL_Event *event) = 0;
    virtual void update() = 0;
};

class TopDownCamera : public Camera {
    glm::vec3 focus = glm::vec3(0);
    float dist = 100;
    float pitch = 45;
    float yaw = 45;
    bool rotating = false;
    bool orthogonal = false;

public:
    virtual bool handleSDLEvent(union SDL_Event *event) override;
    virtual void update() override;
};
