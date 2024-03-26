#include <vector>
#include <cstdint>
#include <cassert>

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#undef None
#undef Always
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "DeviceManager.h"
#include <nvrhi/utils.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class SdlLogger : public Logger {
    void logMessage(LogLevel level, const char *messageText) override {
        switch (level) {
        case Logger::LogLevel::Debug:
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, messageText);
            break;
        case Logger::LogLevel::Info:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, messageText);
            break;
        case Logger::LogLevel::Warning:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, messageText);
            break;
        case Logger::LogLevel::Error:
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, messageText);
            break;
        case Logger::LogLevel::Critical:
            SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, messageText);
            break;
        }
    }
};

static SdlLogger logger;


#define SDL_CHECK(Cond) do { \
    if (!(Cond)) { \
        logger.critical(#Cond " failed: %s", SDL_GetError()); \
        return 1; \
    } \
} while(0)


static int EventWatcherCallback(void *userdata, SDL_Event *event) {
    DeviceManager *deviceManager = static_cast<DeviceManager *>(userdata);
    switch (event->type) {
    case SDL_WINDOWEVENT_RESIZED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        deviceManager->RequestRecreateSwapchain();
        break;
    }
    return 0;
}

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

static nvrhi::ShaderHandle loadShader(nvrhi::IDevice *device, const char *path, nvrhi::ShaderType type) {
    size_t size;
    char *buf = loadFile(path, size);
    nvrhi::ShaderHandle handle = device->createShader(nvrhi::ShaderDesc(type), buf, size);
    free(buf);
    return handle;
}

static nvrhi::TextureHandle loadTexture(nvrhi::IDevice *device, nvrhi::CommandListHandle commandList, const char *filename) {
    int width, height, comp;
    int ok = stbi_info(filename, &width, &height, &comp);
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

    stbi_uc *buf = stbi_load(filename, &width, &height, &comp, req_comp);
    assert(buf);

    auto textureDesc = nvrhi::TextureDesc()
        .setDimension(nvrhi::TextureDimension::Texture2D)
        .setWidth(width)
        .setHeight(height)
        .setFormat(format)
        .setInitialState(nvrhi::ResourceStates::ShaderResource)
        .setKeepInitialState(true)
        .setDebugName(filename);
    nvrhi::TextureHandle texture = device->createTexture(textureDesc);
    assert(texture);
    logger.debug("Loaded texture %s (%d x %d x %d/%d)", filename, width, height, comp, req_comp);

    commandList->writeTexture(texture, /* arraySlice = */ 0, /* mipLevel = */ 0, buf, width*req_comp);
    free(buf);

    return texture;
}

struct Vertex {
    float position[3];
    float color[4];
    float texCoord[2];
};

static const Vertex modelVertices[] = {
    { { -1.f, -1.f, 0.f }, { 1.f, 0.f, 0.f, 1.f }, { 0.f, 1.f } },
    { { -1.f,  1.f, 0.f }, { 0.f, 1.f, 0.f, 1.f }, { 0.f, 0.f } },
    { {  1.f,  1.f, 0.f }, { 1.f, 0.f, 1.f, 1.f }, { 1.f, 0.f } },

    { {  1.f, -1.f, 0.f }, { 0.f, 0.f, 1.f, 1.f }, { 1.f, 1.f } },
    { { -1.f, -1.f, 0.f }, { 1.f, 0.f, 0.f, 1.f }, { 0.f, 1.f } },
    { {  1.f,  1.f, 0.f }, { 1.f, 0.f, 1.f, 1.f }, { 1.f, 0.f } },
};

static const float identityMatrix[16] = {
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1,
};

int main(int argc, char* argv[]) {
    DeviceCreationParameters params;
    params.logger = &logger;
    params.enableDebugRuntime = true;
    params.enableNvrhiValidationLayer = true;
    params.vsyncEnabled = true;

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    SDL_CHECK(SDL_Vulkan_LoadLibrary(nullptr) == 0);
    SDL_Window *window = SDL_CreateWindow("vulkantest",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        params.backBufferWidth, params.backBufferHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    SDL_CHECK(window);
    
    uint32_t extensionCount;
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr));
    std::vector<const char *> extensionNames(extensionCount);
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data()));
    for (const char *extensionName : extensionNames) {
        params.requiredVulkanInstanceExtensions.push_back(extensionName);
    }

    params.createSurfaceCallback = [&](VkInstance vkInst, VkSurfaceKHR *surface) {
        if (!SDL_Vulkan_CreateSurface(window, vkInst, surface)) {
            logger.critical("Error creating Vulkan surface: %s", SDL_GetError());
            return false;
        }
        assert(surface);
        logger.debug("Created SDL Vulkan surface.");
        return true;
    };

    DeviceManager *deviceManager = DeviceManager::Create(nvrhi::GraphicsAPI::VULKAN);
    SDL_AddEventWatch(EventWatcherCallback, deviceManager);
    deviceManager->CreateWindowDeviceAndSwapChain(params);
    nvrhi::IDevice *device = deviceManager->GetDevice();
    nvrhi::CommandListHandle commandList = device->createCommandList();

    nvrhi::BufferHandle vertexBuffer = device->createBuffer(nvrhi::BufferDesc()
        .setByteSize(sizeof(modelVertices))
        .setIsVertexBuffer(true)
        .setInitialState(nvrhi::ResourceStates::VertexBuffer)
        .setKeepInitialState(true) // enable fully automatic state tracking
        .setDebugName("Vertex Buffer"));
    nvrhi::ShaderHandle vertShader = loadShader(device, "shaders/trivial_tex.vert.spv", nvrhi::ShaderType::Vertex);
    nvrhi::ShaderHandle fragShader = loadShader(device, "shaders/trivial_tex.frag.spv", nvrhi::ShaderType::Pixel);
    commandList->open();
    nvrhi::TextureHandle texture = loadTexture(device, commandList, "assets/skyboxes/default_right1.jpg");
    commandList->writeBuffer(vertexBuffer, modelVertices, sizeof(modelVertices));
    commandList->close();
    device->executeCommandList(commandList);
    
    nvrhi::BindingLayoutHandle bindingLayout;
    nvrhi::GraphicsPipelineHandle graphicsPipeline;
    {
        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)*16))
            .addItem(nvrhi::BindingLayoutItem::Sampler(0))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        layoutDesc.bindingOffsets.setSamplerOffset(0);
        bindingLayout = device->createBindingLayout(layoutDesc);

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(Vertex, position))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setOffset(offsetof(Vertex, color))
                .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                .setName("TEXCOORD")
                .setFormat(nvrhi::Format::RG32_FLOAT)
                .setOffset(offsetof(Vertex, texCoord))
                .setElementStride(sizeof(Vertex)),
        };
        nvrhi::InputLayoutHandle inputLayout = device->createInputLayout(attributes, 3, vertShader);

        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
            .setInputLayout(inputLayout)
            .setVertexShader(vertShader)
            .setPixelShader(fragShader)
            .addBindingLayout(bindingLayout);
        //pipelineDesc.renderState.rasterState.setCullNone();
        //pipelineDesc.renderState.depthStencilState.setDepthTestEnable(false);
        graphicsPipeline = device->createGraphicsPipeline(pipelineDesc, deviceManager->GetCurrentFramebuffer());
    }

    auto samplerDesc = nvrhi::SamplerDesc()
        .setAllFilters(true)
        .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    samplerDesc.setAllFilters(true);
    auto linearClampSampler = device->createSampler(samplerDesc);

    logger.debug("Initialized with errors: %s", SDL_GetError());
    nvrhi::BindingSetHandle bindingSet = device->createBindingSet(nvrhi::BindingSetDesc()
        .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)*16))
        .addItem(nvrhi::BindingSetItem::Sampler(0, linearClampSampler))
        .addItem(nvrhi::BindingSetItem::Texture_SRV(1, texture)), bindingLayout);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) && !deviceManager->IsRecreateSwapchainRequested()) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                deviceManager->RequestRecreateSwapchain();
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_q:
                case SDLK_ESCAPE:
                    running = false;
                    break;
                }
                break;
            }
        }

        if (deviceManager->BeginFrame()) {
            nvrhi::IFramebuffer *framebuffer = deviceManager->GetCurrentFramebuffer();

            commandList->open();
            nvrhi::utils::ClearColorAttachment(commandList, framebuffer, 0, nvrhi::Color(0.f));
            nvrhi::utils::ClearDepthStencilAttachment(commandList, framebuffer, 1.f, 0);

            auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(graphicsPipeline)
                .setFramebuffer(framebuffer)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(deviceManager->GetFramebufferWidth(), deviceManager->GetFramebufferHeight())))
                .addBindingSet(bindingSet)
                .addVertexBuffer(nvrhi::VertexBufferBinding().setSlot(0).setOffset(0).setBuffer(vertexBuffer));
            commandList->setGraphicsState(graphicsState);

            commandList->setPushConstants(identityMatrix, sizeof(identityMatrix));

            commandList->draw(nvrhi::DrawArguments().setVertexCount(6));

            commandList->close();
            device->executeCommandList(commandList);

            deviceManager->Present();
        }

        device->runGarbageCollection();
    }

    device->waitForIdle();
    linearClampSampler = nullptr;
    bindingSet = nullptr;
    vertexBuffer = nullptr;
    graphicsPipeline = nullptr;
    bindingLayout = nullptr;
    texture = nullptr;
    vertShader = nullptr;
    fragShader = nullptr;
    commandList = nullptr;
    deviceManager->Destroy();
    delete deviceManager;

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    logger.debug("Cleaned up with errors: %s", SDL_GetError());
    return 0;
}
