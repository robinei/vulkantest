#include <vector>
#include <memory>
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
#include "AssetLoader.h"
#include "JobSystem.h"
#include <nvrhi/utils.h>


#include "Camera.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
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
#pragma clang diagnostic pop


static SdlLogger sdlLogger;
Logger *const logger = &sdlLogger;


#define SDL_CHECK(Cond) do { \
    if (!(Cond)) { \
        logger->critical(#Cond " failed: %s", SDL_GetError()); \
        return 1; \
    } \
} while(0)


static int EventWatcherCallback(void *userdata, SDL_Event *event) {
    DeviceManager *deviceManager = static_cast<DeviceManager *>(userdata);
    switch (event->type) {
    case SDL_WINDOWEVENT_RESIZED:
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        deviceManager->requestRecreateSwapchain();
        break;
    }
    return 0;
}


static std::atomic<int> dtorCount;
static std::atomic<int> ctorCount;
static std::atomic<int> cctorCount;
static std::atomic<int> mctorCount;
static std::atomic<int> cassignCount;
static std::atomic<int> massignCount;
struct Counter {
    __attribute__((noinline)) ~Counter() { ++dtorCount; }
    __attribute__((noinline)) Counter() { ++ctorCount; }
    __attribute__((noinline)) Counter(const Counter &) { ++cctorCount; }
    __attribute__((noinline)) Counter(Counter &&) { ++mctorCount; }
    __attribute__((noinline)) Counter& operator=(const Counter& other) { ++cassignCount; return *this; }
    __attribute__((noinline)) Counter& operator=(Counter&& other) noexcept { ++massignCount; return *this; }
};


struct LineVertex {
    glm::vec3 position;
    glm::vec4 color;
};
#define MAX_LINE_VERTS 4096
static LineVertex lineVerts[MAX_LINE_VERTS];
static int numLineVerts;
static void emitLine(const glm::vec3 &a, const glm::vec3 &b, const glm::vec4 &color) {
    LineVertex *v = lineVerts + numLineVerts;
    v[0].position = a;
    v[0].color = color;
    v[1].position = b;
    v[1].color = color;
    numLineVerts += 2;
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


struct SkyboxVertex {
    float position[3];
};
static const SkyboxVertex skyboxVertices[] = {
    { -1.0f,  1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f, -1.0f },
    { -1.0f,  1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f },
    { -1.0f, -1.0f,  1.0f },
    {  1.0f, -1.0f,  1.0f },
};
static unsigned short skyboxIndices[] = {
    0, 1, 2,    // side 1
    2, 1, 3,
    4, 0, 6,    // side 2
    6, 0, 2,
    7, 5, 6,    // side 3
    6, 5, 4,
    3, 1, 7,    // side 4
    7, 1, 5,
    4, 5, 0,    // side 5
    0, 5, 1,
    3, 7, 2,    // side 6
    2, 7, 6,
};



int main(int argc, char* argv[]) {
    JobSystem::start();
    DeviceCreationParameters params;
    params.logger = logger;
    params.enableDebugRuntime = true;
    params.enableNvrhiValidationLayer = true;
    params.vsyncEnabled = true;

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    SDL_CHECK(SDL_Vulkan_LoadLibrary(nullptr) == 0);
    SDL_Window *window = SDL_CreateWindow("vulkantest",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        params.backBufferWidth, params.backBufferHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP);
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
            logger->critical("Error creating Vulkan surface: %s", SDL_GetError());
            return false;
        }
        assert(surface);
        logger->debug("Created SDL Vulkan surface.");
        return true;
    };

    std::unique_ptr<DeviceManager> deviceManager(DeviceManager::create(nvrhi::GraphicsAPI::VULKAN));
    SDL_AddEventWatch(EventWatcherCallback, deviceManager.get());
    deviceManager->createWindowDeviceAndSwapChain(params);
    nvrhi::IDevice *device = deviceManager->getDevice();
    nvrhi::CommandListHandle commandList = device->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
    AssetLoader::initialize(device);
    
    nvrhi::BufferHandle lineVertexBuffer;
    nvrhi::GraphicsPipelineHandle lineGraphicsPipeline;
    nvrhi::BindingSetHandle lineBindingSet;
    {
        JobScope scope;
        auto vertShader = AssetLoader::getShader("trivial_color.vert.spv", nvrhi::ShaderType::Vertex);
        auto fragShader = AssetLoader::getShader("trivial_color.frag.spv", nvrhi::ShaderType::Pixel);
        scope.dispatch();

        lineVertexBuffer = device->createBuffer(nvrhi::BufferDesc()
            .setByteSize(sizeof(lineVerts))
            .setIsVertexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true));

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)*16));
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(LineVertex, position))
                .setElementStride(sizeof(LineVertex)),
            nvrhi::VertexAttributeDesc()
                .setName("COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setOffset(offsetof(LineVertex, color))
                .setElementStride(sizeof(LineVertex)),
        };
        nvrhi::InputLayoutHandle inputLayout = device->createInputLayout(attributes, 2, vertShader->get());

        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
            .setPrimType(nvrhi::PrimitiveType::LineList)
            .setInputLayout(inputLayout)
            .setVertexShader(vertShader->get())
            .setPixelShader(fragShader->get())
            .addBindingLayout(bindingLayout);
        pipelineDesc.renderState.rasterState.setCullNone();
        pipelineDesc.renderState.depthStencilState.setDepthTestEnable(false);
        lineGraphicsPipeline = device->createGraphicsPipeline(pipelineDesc, deviceManager->getCurrentFramebuffer());
        
        lineBindingSet = device->createBindingSet(nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)*16)), bindingLayout);
    }
    
    nvrhi::BufferHandle vertexBuffer;
    nvrhi::GraphicsPipelineHandle graphicsPipeline;
    nvrhi::BindingSetHandle bindingSet;
    {
        uint64_t start = SDL_GetTicks64();
        JobScope scope;
        auto vertShader = AssetLoader::getShader("trivial_tex.vert.spv", nvrhi::ShaderType::Vertex);
        auto fragShader = AssetLoader::getShader("trivial_tex.frag.spv", nvrhi::ShaderType::Pixel);
        auto texture = AssetLoader::getTexture("space_cubemap.jpg");
        scope.dispatch();
        assert(vertShader->isLoaded());
        assert(fragShader->isLoaded());
        assert(texture->isLoaded());
        uint64_t end = SDL_GetTicks64();
        logger->info("Assets loaded in %d ms", end-start);

        vertexBuffer = device->createBuffer(nvrhi::BufferDesc()
            .setByteSize(sizeof(modelVertices))
            .setIsVertexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true) // enable fully automatic state tracking
            .setDebugName("Vertex Buffer"));
        commandList->open();
        commandList->writeBuffer(vertexBuffer, modelVertices, sizeof(modelVertices));
        commandList->close();
        device->executeCommandList(commandList);

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)*16))
            .addItem(nvrhi::BindingLayoutItem::Sampler(0))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        layoutDesc.bindingOffsets.setSamplerOffset(0);
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

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
        nvrhi::InputLayoutHandle inputLayout = device->createInputLayout(attributes, 3, vertShader->get());

        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
            .setInputLayout(inputLayout)
            .setVertexShader(vertShader->get())
            .setPixelShader(fragShader->get())
            .addBindingLayout(bindingLayout);
        pipelineDesc.renderState.rasterState.setCullNone();
        pipelineDesc.renderState.depthStencilState.setDepthTestEnable(false);
        graphicsPipeline = device->createGraphicsPipeline(pipelineDesc, deviceManager->getCurrentFramebuffer());

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAllFilters(true)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        samplerDesc.setAllFilters(true);
        auto linearClampSampler = device->createSampler(samplerDesc);
        
        bindingSet = device->createBindingSet(nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)*16))
            .addItem(nvrhi::BindingSetItem::Sampler(0, linearClampSampler))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, texture->get())), bindingLayout);
    }

    nvrhi::BufferHandle skyboxVertexBuffer;
    nvrhi::BufferHandle skyboxIndexBuffer;
    nvrhi::GraphicsPipelineHandle skyboxPipeline;
    nvrhi::BindingSetHandle skyboxBindings;
    {
        JobScope scope;
        auto vertShader = AssetLoader::getShader("skybox.vert.spv", nvrhi::ShaderType::Vertex);
        auto fragShader = AssetLoader::getShader("skybox.frag.spv", nvrhi::ShaderType::Pixel);
        auto cubemap = AssetLoader::getTexture("space_cubemap.jpg", nvrhi::TextureDimension::TextureCube);
        scope.dispatch();

        skyboxVertexBuffer = device->createBuffer(nvrhi::BufferDesc()
            .setByteSize(sizeof(skyboxVertices))
            .setIsVertexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true) // enable fully automatic state tracking
            .setDebugName("Vertex Buffer"));
        skyboxIndexBuffer = device->createBuffer(nvrhi::BufferDesc()
            .setByteSize(sizeof(skyboxIndices))
            .setIsIndexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
            .setKeepInitialState(true) // enable fully automatic state tracking
            .setDebugName("Index Buffer"));
        commandList->open();
        commandList->writeBuffer(skyboxVertexBuffer, skyboxVertices, sizeof(skyboxVertices));
        commandList->writeBuffer(skyboxIndexBuffer, skyboxIndices, sizeof(skyboxIndices));
        commandList->close();
        device->executeCommandList(commandList);

        auto layoutDesc = nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float)*16))
            .addItem(nvrhi::BindingLayoutItem::Sampler(0))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        layoutDesc.bindingOffsets.setSamplerOffset(0);
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

        nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                .setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setOffset(offsetof(SkyboxVertex, position))
                .setElementStride(sizeof(SkyboxVertex)),
        };
        nvrhi::InputLayoutHandle inputLayout = device->createInputLayout(attributes, 1, vertShader->get());

        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
            .setInputLayout(inputLayout)
            .setVertexShader(vertShader->get())
            .setPixelShader(fragShader->get())
            .addBindingLayout(bindingLayout);
        pipelineDesc.renderState.rasterState.setCullNone();
        pipelineDesc.renderState.depthStencilState.setDepthTestEnable(false);
        pipelineDesc.renderState.depthStencilState.setDepthWriteEnable(false);
        skyboxPipeline = device->createGraphicsPipeline(pipelineDesc, deviceManager->getCurrentFramebuffer());

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAllFilters(true)
            .setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        samplerDesc.setAllFilters(true);
        auto linearClampSampler = device->createSampler(samplerDesc);

        skyboxBindings = device->createBindingSet(nvrhi::BindingSetDesc()
            .addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(float)*16))
            .addItem(nvrhi::BindingSetItem::Sampler(0, linearClampSampler))
            .addItem(nvrhi::BindingSetItem::Texture_SRV(1, cubemap->get())), bindingLayout);
    }

    logger->debug("Initialized with errors: %s", SDL_GetError());

    uint64_t start = SDL_GetTicks64();
    std::atomic<int> counter(0);
    Counter counterObject;
    {
        JobScope scope;
        for (int i = 0; i < 1000; ++i) {
            Job::enqueue([&counter, &scope, counterObject = std::move(counterObject)] {
                JobScope scope2(scope);
                for (int j = 0; j < 1000; ++j) {
                    Job::enqueue([&counter] {
                        ++counter;
                    });
                }
            });
        }
    }
    uint64_t end = SDL_GetTicks64();
    logger->info("Test counter: %d in %d ms", (int)counter, end-start);
    logger->info("dtorCount %d, ctorCount: %d, cctorCount: %d, mctorCount: %d, cassignCount: %d, massignCount: %d", (int)dtorCount, (int)ctorCount, (int)cctorCount, (int)mctorCount, (int)cassignCount, (int)massignCount);

    TopDownCamera camera;

    Uint64 prevTicks = SDL_GetTicks64();
    bool running = true;
    while (running) {
        Uint64 ticks = SDL_GetTicks64();
        Uint64 tickDiff = ticks - prevTicks;
        prevTicks = ticks;
        float dt = (float)tickDiff / 1000.0f;
        (void)dt;

        camera.setScreenSize(deviceManager->getFramebufferWidth(), deviceManager->getFramebufferHeight());

        JobScope jobScope;
        SDL_Event event;
        while (SDL_PollEvent(&event) && !deviceManager->isRecreateSwapchainRequested()) {
            if (camera.handleSDLEvent(&event)) {
                continue;
            }
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                deviceManager->requestRecreateSwapchain();
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

        camera.update();
        // game update should be here

        jobScope.dispatch(); // let all update jobs finish before we start rendering

        if (deviceManager->beginFrame()) {
            nvrhi::IFramebuffer *framebuffer = deviceManager->getCurrentFramebuffer();

            numLineVerts = 0;
            emitLine(glm::vec3(0), glm::vec3(10, 0, 0), glm::vec4(1, 0, 0, 1));
            emitLine(glm::vec3(0), glm::vec3(0, 10, 0), glm::vec4(0, 1, 0, 1));
            emitLine(glm::vec3(0), glm::vec3(0, 0, 10), glm::vec4(0, 0, 1, 1));

            commandList->open();
            nvrhi::utils::ClearColorAttachment(commandList, framebuffer, 0, nvrhi::Color(0.f));
            nvrhi::utils::ClearDepthStencilAttachment(commandList, framebuffer, 1.f, 0);

            {
                auto graphicsState = nvrhi::GraphicsState()
                    .setPipeline(skyboxPipeline)
                    .setFramebuffer(framebuffer)
                    .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(deviceManager->getFramebufferWidth(), deviceManager->getFramebufferHeight())))
                    .addBindingSet(skyboxBindings)
                    .setIndexBuffer(nvrhi::IndexBufferBinding().setFormat(nvrhi::Format::R16_UINT).setBuffer(skyboxIndexBuffer))
                    .addVertexBuffer(nvrhi::VertexBufferBinding().setSlot(0).setOffset(0).setBuffer(skyboxVertexBuffer));
                commandList->setGraphicsState(graphicsState);
                glm::mat4 vm = camera.getViewMatrix();
                vm[3].x = 0;
                vm[3].y = 0;
                vm[3].z = 0;
                glm::mat4 pvm = camera.getPerspectiveMatrix() * vm;
                commandList->setPushConstants(&pvm, sizeof(pvm));
                commandList->drawIndexed(nvrhi::DrawArguments().setVertexCount(36));
            }

            {
                auto graphicsState = nvrhi::GraphicsState()
                    .setPipeline(graphicsPipeline)
                    .setFramebuffer(framebuffer)
                    .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(deviceManager->getFramebufferWidth(), deviceManager->getFramebufferHeight())))
                    .addBindingSet(bindingSet)
                    .addVertexBuffer(nvrhi::VertexBufferBinding().setSlot(0).setOffset(0).setBuffer(vertexBuffer));
                commandList->setGraphicsState(graphicsState);
                glm::mat4 pvm = camera.getProjectionMatrix() * camera.getViewMatrix();
                commandList->setPushConstants(&pvm, sizeof(pvm));
                commandList->draw(nvrhi::DrawArguments().setVertexCount(6));
            }

            {
                commandList->writeBuffer(lineVertexBuffer, lineVerts, sizeof(LineVertex)*numLineVerts);
                auto graphicsState = nvrhi::GraphicsState()
                    .setPipeline(lineGraphicsPipeline)
                    .setFramebuffer(framebuffer)
                    .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(deviceManager->getFramebufferWidth(), deviceManager->getFramebufferHeight())))
                    .addBindingSet(lineBindingSet)
                    .addVertexBuffer(nvrhi::VertexBufferBinding().setSlot(0).setOffset(0).setBuffer(lineVertexBuffer));
                commandList->setGraphicsState(graphicsState);
                glm::mat4 pvm = camera.getProjectionMatrix() * camera.getViewMatrix();
                commandList->setPushConstants(&pvm, sizeof(pvm));
                commandList->draw(nvrhi::DrawArguments().setVertexCount(numLineVerts));
            }

            commandList->close();
            device->executeCommandList(commandList);

            deviceManager->present();
            AssetLoader::garbageCollect(true);
        }

        device->runGarbageCollection();
    }

    JobSystem::stop();
    AssetLoader::cleanup();

    device->waitForIdle();
    lineBindingSet = nullptr;
    lineVertexBuffer = nullptr;
    lineGraphicsPipeline = nullptr;
    bindingSet = nullptr;
    vertexBuffer = nullptr;
    graphicsPipeline = nullptr;
    skyboxBindings = nullptr;
    skyboxVertexBuffer = nullptr;
    skyboxIndexBuffer = nullptr;
    skyboxPipeline = nullptr;
    commandList = nullptr;
    deviceManager = nullptr;

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    logger->debug("Exited with errors: %s", SDL_GetError());
    return 0;
}
