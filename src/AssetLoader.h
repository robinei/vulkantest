#pragma once

#include "Logger.h"
#include <nvrhi/nvrhi.h>

class AssetLoader {
    nvrhi::IDevice *device;
    nvrhi::CommandListHandle commandList;
    Logger *logger;
    bool isCommandListOpen;
public:
    AssetLoader(nvrhi::IDevice *device, Logger *logger);
    nvrhi::ShaderHandle loadShader(const char *path, nvrhi::ShaderType type);
    nvrhi::TextureHandle loadTexture(const char *path);
    void finishResouceUploads();
};
