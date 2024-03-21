#include <vector>
#include <cstdint>
#include <cassert>

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#define VK_USE_PLATFORM_WIN32_KHR
#else /* _WIN32 */
#define VK_USE_PLATFORM_XLIB_KHR
#endif /* _WIN32 */


#include <X11/Xlib.h>
#undef None
#undef Always
#include <nvrhi/vulkan.h>
#include <nvrhi/validation.h>


#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>



#define SDL_CHECK(Cond) do { \
    if (!(Cond)) { \
        SDL_Log(#Cond " failed: %s", SDL_GetError()); \
        exit(1); \
    } \
} while(0)

#define VK_CHECK(Call) do { \
    VkResult result = (Call); \
    if (result != VK_SUCCESS) { \
        SDL_Log(#Call " failed with error code: %d", (int)result); \
        exit(1); \
    } \
} while(0)

class MessageCallback : public nvrhi::IMessageCallback {
public:
    virtual void message(nvrhi::MessageSeverity severity, const char* messageText) override {
        SDL_Log("NVRHI message: %s", messageText);
    }
};


static uint32_t screenWidth = 640;
static uint32_t screenHeight = 480;
static VkInstance vkInst;
static VkPhysicalDevice physicalDevice;
static VkDevice device;
static VkSurfaceKHR surface;
static VkQueue graphicsQueue;
static VkQueue presentQueue;
static VkFormat surfaceFormat;
static VkColorSpaceKHR colorSpace;
static VkSwapchainKHR swapchain;

static MessageCallback messageCallback;
static nvrhi::DeviceHandle nvrhiDevice;
static std::vector<nvrhi::TextureHandle> swapchainTextures;

static void createDevice(SDL_Window* window) {
    uint32_t extensionCount;
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr));
    std::vector<const char *> extensionNames(extensionCount);
    SDL_CHECK(SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data()));
    for (uint32_t i = 0; i < extensionCount; ++i) {
        SDL_Log("required extension: %s", extensionNames[i]);
    }

    const VkInstanceCreateInfo instInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // sType
        nullptr,                                // pNext
        0,                                      // flags
        nullptr,                                // pApplicationInfo
        0,                                      // enabledLayerCount
        nullptr,                                // ppEnabledLayerNames
        extensionCount,                         // enabledExtensionCount
        extensionNames.data(),                  // ppEnabledExtensionNames
    };
    VK_CHECK(vkCreateInstance(&instInfo, nullptr, &vkInst));

    uint32_t physicalDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(vkInst, &physicalDeviceCount, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(vkInst, &physicalDeviceCount, physicalDevices.data()));
    physicalDevice = physicalDevices[0];
    SDL_Log("Device count: %d", physicalDeviceCount);

    uint32_t extensionPropertyCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionPropertyCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionPropertyCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionPropertyCount, extensionProperties.data());
    SDL_Log("extension cound: %d", extensionPropertyCount);

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    surface = nullptr;
    SDL_CHECK(SDL_Vulkan_CreateSurface(window, vkInst, &surface));
    assert(surface);

    uint32_t graphicsQueueIndex = UINT32_MAX;
    uint32_t presentQueueIndex = UINT32_MAX;
    uint32_t i = 0;
    for (VkQueueFamilyProperties queueFamily : queueFamilies) {
        if (graphicsQueueIndex == UINT32_MAX && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            graphicsQueueIndex = i;
        if (presentQueueIndex == UINT32_MAX) {
            VkBool32 support;
            VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &support));
            if (support)
                presentQueueIndex = i;
        }
        ++i;
    }
    SDL_Log("graphicsQueueIndex: %d, presentQueueIndex: %d", graphicsQueueIndex, presentQueueIndex);
    assert(graphicsQueueIndex != UINT32_MAX);
    assert(presentQueueIndex != UINT32_MAX);
    assert(graphicsQueueIndex == presentQueueIndex);

    float queuePriorities[1] = {0.0};
    VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                    // pNext
        0,                                          // flags
        graphicsQueueIndex,                         // queueFamilyIndex
        1,                                          // queueCount
        queuePriorities,                            // pQueuePriorities
    };
    
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.shaderClipDistance = VK_TRUE;
    const char* deviceExtensionNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,   // sType
        nullptr,                                // pNext
        0,                                      // flags
        1,                                      // queueCreateInfoCount
        &queueInfo,                             // pQueueCreateInfos
        0,                                      // enabledLayerCount
        nullptr,                                // ppEnabledLayerNames
        1,                                      // enabledExtensionCount
        deviceExtensionNames,                   // ppEnabledExtensionNames
        &deviceFeatures,                        // pEnabledFeatures
    };
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

    // dynamically load Vulkan function pointers
    const vk::DynamicLoader dl;
    const PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkInst, vkGetInstanceProcAddr, device);

    vkGetDeviceQueue(device, graphicsQueueIndex, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueIndex, 0, &presentQueue);

    // Get the list of VkFormat's that are supported:
    uint32_t formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL));
    std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfFormats.data()));
    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        assert(formatCount >= 1);
        surfaceFormat = surfFormats[0].format;
    }
    colorSpace = surfFormats[0].colorSpace;


    nvrhi::vulkan::DeviceDesc deviceDesc;
    deviceDesc.errorCB = &messageCallback;
    deviceDesc.physicalDevice = physicalDevice;
    deviceDesc.device = device;
    deviceDesc.graphicsQueue = graphicsQueue;
    deviceDesc.graphicsQueueIndex = graphicsQueueIndex;
    deviceDesc.deviceExtensions = deviceExtensionNames;
    deviceDesc.numDeviceExtensions = 1;

    nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);
    nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);
    nvrhiDevice = nvrhiValidationLayer; // make the rest of the application go through the validation layer
}

static void createSwapchain() {
    VkSwapchainKHR oldSwapchain = swapchain;

    VkSurfaceCapabilitiesKHR surfCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCapabilities));

    uint32_t presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL));
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

    VkExtent2D swapchainExtent;
    // width and height are either both -1, or both not -1.
    if (surfCapabilities.currentExtent.width == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width = screenWidth;
        swapchainExtent.height = screenHeight;
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent;
        screenWidth = surfCapabilities.currentExtent.width;
        screenHeight = surfCapabilities.currentExtent.height;
    }
    
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Determine the number of VkImage's to use in the swap chain (we desire to
    // own only 1 image at a time, besides the images being displayed and
    // queued for display):
    uint32_t desiredNumberOfSwapchainImages = surfCapabilities.minImageCount + 1;
    if ((surfCapabilities.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCapabilities.maxImageCount)) {
        // Application must settle for fewer images than desired:
        desiredNumberOfSwapchainImages = surfCapabilities.maxImageCount;
    }
    
    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfCapabilities.currentTransform;
    }

    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .surface = surface,
        .minImageCount = desiredNumberOfSwapchainImages,
        .imageFormat = surfaceFormat,
        .imageColorSpace = colorSpace,
        .imageExtent =
            {
             .width = swapchainExtent.width, .height = swapchainExtent.height,
            },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = preTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = swapchainPresentMode,
        .clipped = true,
        .oldSwapchain = oldSwapchain,
    };

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain));
    
    // If we just re-created an existing swapchain, we should destroy the old
    // swapchain at this point.
    // Note: destroying the swapchain also cleans up all its associated
    // presentable images once the platform is done with them.
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, oldSwapchain, NULL);
    }

    uint32_t swapchainImageCount;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL));
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()));

    auto textureDesc = nvrhi::TextureDesc()
        .setDimension(nvrhi::TextureDimension::Texture2D)
        .setFormat(nvrhi::Format::RGBA8_UNORM)
        .setWidth(screenWidth)
        .setHeight(screenHeight)
        .setIsRenderTarget(true)
        .setDebugName("Swap Chain Image");

    swapchainTextures.clear();
    for (VkImage vkImage : swapchainImages) {
        nvrhi::TextureHandle swapChainTexture = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(vkImage), textureDesc);
        swapchainTextures.push_back(swapChainTexture);
    }
}




int main(int argc, char* argv[]) {
    SDL_CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    SDL_CHECK(SDL_Vulkan_LoadLibrary(nullptr) == 0);
    SDL_Window* window = SDL_CreateWindow("cppgame", 0, 0, screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    SDL_CHECK(window);

    createDevice(window);
    createSwapchain();
    SDL_Log("Initialized with errors: %s", SDL_GetError());

    bool running = true;
    while (running) {
        SDL_Event windowEvent;
        while (SDL_PollEvent(&windowEvent)) {
            if (windowEvent.type == SDL_QUIT) {
                running = false;
                break;
            }

            // draw here

            VK_CHECK(vkDeviceWaitIdle(device));
        }
    }

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(vkInst, nullptr);
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    SDL_Log("Cleaned up with errors: %s", SDL_GetError());
    return 0;
}
