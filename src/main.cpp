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
        deviceManager->SetResized();
        break;
    }
    return 0;
}


int main(int argc, char* argv[]) {
    DeviceCreationParameters params;
    params.logger = &logger;
    params.enableDebugRuntime = true;
    params.enableNvrhiValidationLayer = true;

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
    deviceManager->CreateWindowDeviceAndSwapChain(params);
    nvrhi::IDevice *device = deviceManager->GetDevice();
    nvrhi::CommandListHandle commandList = device->createCommandList();

    SDL_AddEventWatch(EventWatcherCallback, deviceManager);
    
    logger.debug("Initialized with errors: %s", SDL_GetError());

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                deviceManager->SetResized();
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

        deviceManager->MaybeRecreateSwapchain();

        if (deviceManager->BeginFrame()) {
            nvrhi::IFramebuffer *framebuffer = deviceManager->GetCurrentFramebuffer();

            commandList->open();
            nvrhi::utils::ClearColorAttachment(commandList, framebuffer, 0, nvrhi::Color(1.f));
            commandList->close();
            device->executeCommandList(commandList);

            deviceManager->Present();
        }

        SDL_Delay(0);
        device->runGarbageCollection();
    }

    device->waitForIdle();
    commandList = nullptr;
    deviceManager->Destroy();
    delete deviceManager;

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    logger.debug("Cleaned up with errors: %s", SDL_GetError());
    return 0;
}
