#pragma once

#include <nvrhi/nvrhi.h>
#include "Camera.h"

struct RenderContext {
    nvrhi::IDevice *device;
    nvrhi::IFramebuffer *framebuffer;
    Camera *camera;
    nvrhi::Viewport viewport;
    nvrhi::CommandListHandle commandList;
};
