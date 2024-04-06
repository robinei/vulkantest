#pragma once

#include <nvrhi/nvrhi.h>
#include "Camera.h"

struct RenderContext {
    nvrhi::IDevice *device = nullptr;
    nvrhi::IFramebuffer *framebuffer = nullptr;
    Camera *camera = nullptr;
    nvrhi::Viewport viewport;
    nvrhi::CommandListHandle commandList = nullptr;
};
