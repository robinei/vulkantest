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
#include <nvrhi/utils.h>

#include "DeviceManager.h"
#include "AssetLoader.h"
#include "JobSystem.h"
#include "DebugLines.h"
#include "SkyBox.h"
#include "Camera.h"
#include "Logger.h"


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
class SdlLogger : public Logger {
    void logMessage(LogLevel level, const char *messageText) override {
        switch (level) {
        case Logger::LogLevel::Debug: SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case Logger::LogLevel::Info: SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case Logger::LogLevel::Warning: SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case Logger::LogLevel::Error: SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case Logger::LogLevel::Critical: SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        }
    }
};
struct MessageCallback : public nvrhi::IMessageCallback {
    void message(nvrhi::MessageSeverity severity, const char* messageText) override {
        switch (severity) {
        case nvrhi::MessageSeverity::Info: SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case nvrhi::MessageSeverity::Warning: SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case nvrhi::MessageSeverity::Error: SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        case nvrhi::MessageSeverity::Fatal: SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, messageText); break;
        }
    }
};
#pragma clang diagnostic pop


static SDL_Window *window;
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

struct DelegateImpl : public DeviceManagerDelegate {
    bool createSurfaceCallback(VkInstance instance, VkSurfaceKHR *surface) override {
        if (!SDL_Vulkan_CreateSurface(window, instance, surface)) {
            logger->critical("Error creating Vulkan surface: %s", SDL_GetError());
            return false;
        }
        assert(surface);
        logger->debug("Created SDL Vulkan surface.");
        return true;
    }
};

int main(int argc, char* argv[]) {
    JobSystem::start();

    DelegateImpl delegate;
    MessageCallback messageCallback;
    DeviceCreationParameters params;
    params.delegate = &delegate;
    params.messageCallback = &messageCallback;
    params.enableDebugRuntime = true;
    params.enableNvrhiValidationLayer = true;
    params.vsyncEnabled = true;

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    SDL_CHECK(SDL_Vulkan_LoadLibrary(nullptr) == 0);
    window = SDL_CreateWindow("vulkantest",
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

    std::unique_ptr<DeviceManager> deviceManager(DeviceManager::create(nvrhi::GraphicsAPI::VULKAN));
    SDL_AddEventWatch(EventWatcherCallback, deviceManager.get());
    deviceManager->createWindowDeviceAndSwapChain(params);
    logger->debug("Initialized with errors: %s", SDL_GetError());

    nvrhi::IDevice *device = deviceManager->getDevice();
    nvrhi::CommandListHandle commandList = device->createCommandList();

    AssetLoader::initialize(device);
    initDebugLines();
    initSkyBox();
    setSkyBoxTexture("space_cubemap.jpg");

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
        clearDebugLines();
        drawDebugLine(glm::vec3(0), glm::vec3(10, 0, 0), glm::vec4(1, 0, 0, 1));
        drawDebugLine(glm::vec3(0), glm::vec3(0, 10, 0), glm::vec4(0, 1, 0, 1));
        drawDebugLine(glm::vec3(0), glm::vec3(0, 0, 10), glm::vec4(0, 0, 1, 1));

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
        JobSystem::runPendingMainJobs();

        RenderContext renderContext;
        renderContext.device = device;
        renderContext.framebuffer = deviceManager->getCurrentFramebuffer();
        renderContext.camera = &camera;
        renderContext.viewport = nvrhi::Viewport(deviceManager->getFramebufferWidth(), deviceManager->getFramebufferHeight());
        renderContext.commandList = commandList;

        updateSkyBox(renderContext);
        updateDebugLines(renderContext);

        if (deviceManager->beginFrame()) {
            renderContext.framebuffer = deviceManager->getCurrentFramebuffer();
            commandList->open();
            {
                nvrhi::utils::ClearColorAttachment(commandList, renderContext.framebuffer, 0, nvrhi::Color(0.f));
                nvrhi::utils::ClearDepthStencilAttachment(commandList, renderContext.framebuffer, 1.f, 0);
                renderSkyBox(renderContext);
                renderDebugLines(renderContext);
            }
            commandList->close();
            device->executeCommandList(commandList);

            deviceManager->present();
            AssetLoader::garbageCollect(true);
        }

        device->runGarbageCollection();
    }

    JobSystem::stop();
    deinitSkyBox();
    deinitDebugLines();
    AssetLoader::cleanup();

    device->waitForIdle();
    commandList = nullptr;
    deviceManager = nullptr;

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    logger->debug("Exited with errors: %s", SDL_GetError());
    return 0;
}
