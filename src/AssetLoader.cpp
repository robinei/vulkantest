#include "AssetLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


static char *loadFile(const char *path, size_t &size) {
    FILE *fp = fopen(path, "rb");
    assert(fp);
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    size_t n = fread(buf, 1, size, fp);
    assert(n == size);
    buf[size] = '\0';
    return buf;
}

AssetLoader::AssetLoader(nvrhi::IDevice *device, Logger *logger) : device(device), logger(logger), isCommandListOpen(false) {
    commandList = device->createCommandList();
}

nvrhi::ShaderHandle AssetLoader::loadShader(const char *path, nvrhi::ShaderType type) {
    size_t size;
    char *buf = loadFile(path, size);
    nvrhi::ShaderHandle handle = device->createShader(nvrhi::ShaderDesc(type), buf, size);
    free(buf);
    return handle;
}

nvrhi::TextureHandle AssetLoader::loadTexture(const char *path) {
    int width, height, comp;
    int ok = stbi_info(path, &width, &height, &comp);
    assert(ok);

    int req_comp = 0;
    nvrhi::Format format;
    switch (comp) {
        case 1: format = nvrhi::Format::R8_UNORM; break;
        case 2: format = nvrhi::Format::RG8_UNORM; break;
        case 3:
        case 4: format = nvrhi::Format::SRGBA8_UNORM; req_comp = 4; break;
        default: assert(false);
    }

    stbi_uc *buf = stbi_load(path, &width, &height, &comp, req_comp);
    assert(buf);

    auto textureDesc = nvrhi::TextureDesc()
        .setDimension(nvrhi::TextureDimension::Texture2D)
        .setWidth(width)
        .setHeight(height)
        .setFormat(format)
        .setInitialState(nvrhi::ResourceStates::ShaderResource)
        .setKeepInitialState(true)
        .setDebugName(path);
    nvrhi::TextureHandle texture = device->createTexture(textureDesc);
    assert(texture);
    logger->debug("Loaded texture %s (%d x %d x %d/%d)", path, width, height, comp, req_comp);

    if (!isCommandListOpen) {
        isCommandListOpen = true;
        commandList->open();
    }
    commandList->writeTexture(texture, /* arraySlice = */ 0, /* mipLevel = */ 0, buf, width*req_comp);
    free(buf);

    return texture;
}

void AssetLoader::finishResouceUploads() {
    if (isCommandListOpen) {
        isCommandListOpen = false;
        commandList->close();
        device->executeCommandList(commandList);
    }
}