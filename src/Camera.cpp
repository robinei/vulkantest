#include "Camera.h"
#include "Math/Quat.h"
#include <SDL2/SDL.h>

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
                yaw += a;
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
        dist = clamp(dist, 10.0f, 1000.0f);
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
    Vec3 dir = Quat::fromAxisAngle(Vec3(0, 0, 1), yaw) * Vec3(1, 0, 0);
    Vec3 forward = -dir;
    Vec3 right = cross(dir, Vec3(0, 0, 1));
    dir = Quat::fromAxisAngle(right, pitch) * dir;
    //Vec3 up = cross(right, dir);
    Vec3 pos = focus + dir * dist;
    
    if (orthogonal) {
        float dim = dist*0.5f;
        projectionMatrix.toOrtho(-dim*aspectRatio, dim*aspectRatio,
                                    -dim, dim,
                                    -10000.0f, 10000.0f);
    } else {
        projectionMatrix.toPerspective(45.0f, aspectRatio, 0.1f, 10000.0f);
    }

    viewMatrix.toLookAt(pos,
                        focus,
                        Vec3(0, 0, 1));

    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);

    /*if (!rotating) {
        cursorPos = screen_to_world(mx, my, screenWidth, screenHeight);
    }*/

    Vec3 motion(0, 0, 0);
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] || (mx == 0 && !rotating)) {
        motion += right;
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] || (mx == screenWidth - 1 && !rotating)) {
        motion -= right;
    }
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] || (my == 0 && !rotating)) {
        motion += forward;
    }
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] || (my == screenHeight - 1 && !rotating)) {
        motion -= forward;
    }
    if (sqrLength(motion) > 0) {
        motion = normalize(motion);
        focus += motion * sqrtf(dist) * 0.2f;
    }
}
