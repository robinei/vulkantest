#include "Camera.h"
#include <SDL2/SDL.h>

#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/trigonometric.hpp>


bool TopDownCamera::handleSDLEvent(union SDL_Event *event) {
    switch (event->type) {
    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case SDLK_SPACE:
            orthogonal = !orthogonal;
            return true;
        }
        break;
    case SDL_MOUSEMOTION:
        if (rotating) {
            float a = -360.f * (float)event->motion.xrel / (float)screenWidth;
            do {
                yaw -= a;
            } while (fabsf(yaw) < 0.01f);

            a = 360.f * (float)event->motion.yrel / (float)screenHeight;
            do {
                pitch += a;
            } while (fabsf(pitch) < 0.01f);

            while (yaw < 0.0f) {
                yaw += 360.0f;
            }
            while (yaw > 360.0f) {
                yaw -= 360.0f;
            }

            if (pitch > 89.0f) {
                pitch = 89.0f;
            }
            if (pitch < -89.0f) {
                pitch = -89.0f;
            }
            return true;
        }
        break;
    case SDL_MOUSEWHEEL:
        dist -= (dist * 0.4f * event->wheel.y);
        dist = glm::clamp(dist, 10.0f, 1000.0f);
        return true;
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_RIGHT && !rotating) {
            rotating = true;
            SDL_SetRelativeMouseMode(SDL_TRUE);
            return true;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_RIGHT && rotating) {
            rotating = false;
            SDL_SetRelativeMouseMode(SDL_FALSE);
            return true;
        }
        break;
    }
    return false;
}

void TopDownCamera::update() {
    glm::vec3 dir = angleAxis(glm::radians(yaw), glm::vec3(0, 0, 1)) * glm::vec3(1, 0, 0);
    glm::vec3 forward = -dir;
    glm::vec3 right = cross(dir, glm::vec3(0, 0, 1));

    {
        const Uint8 *keys = SDL_GetKeyboardState(nullptr);
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);

        glm::vec3 motion(0, 0, 0);
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || (mx == 0 && !rotating)) {
            motion -= right;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || (mx == screenWidth - 1 && !rotating)) {
            motion += right;
        }
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || (my == 0 && !rotating)) {
            motion += forward;
        }
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || (my == screenHeight - 1 && !rotating)) {
            motion -= forward;
        }
        if (length(motion) > 0) {
            motion = normalize(motion);
            focus += motion * sqrtf(dist) * 0.2f;
        }
    }

    dir = angleAxis(glm::radians(pitch), right) * dir;
    glm::vec3 pos = focus + dir * dist;
    viewMatrix = glm::lookAt(pos, focus, glm::vec3(0, 0, 1));
    
    if (orthogonal) {
        float dim = dist*0.5f;
        projectionMatrix = glm::ortho(-dim*aspectRatio, dim*aspectRatio,
                                      -dim, dim,
                                      -10000.0f, 10000.0f);
    } else {
        projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 10000.0f);
    }
}
