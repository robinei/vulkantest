/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

// adapted from https://github.com/NVIDIAGameWorks/donut

#pragma once

#define USE_VULKAN 1

#if USE_DX11 || USE_DX12
#include <DXGI.h>
#endif

#if USE_DX11
#include <d3d11.h>
#endif

#if USE_DX12
#include <d3d12.h>
#endif

#if USE_VULKAN
#include <nvrhi/vulkan.h>
#else
#include <nvrhi/nvrhi.h>
#endif


struct DeviceManagerDelegate {
#if USE_VULKAN
    virtual void deviceCreateInfoCallback(VkDeviceCreateInfo &createInfo) { }
    virtual bool createSurfaceCallback(VkInstance instance, VkSurfaceKHR *surface) = 0;
#endif
};

struct InstanceParameters
{
    nvrhi::IMessageCallback *messageCallback = nullptr;
    bool enableDebugRuntime = false;
    bool headlessDevice = false;

#if USE_VULKAN
    std::vector<std::string> requiredVulkanInstanceExtensions;
    std::vector<std::string> requiredVulkanLayers;
    std::vector<std::string> optionalVulkanInstanceExtensions;
    std::vector<std::string> optionalVulkanLayers;
#endif
};

struct DeviceCreationParameters : public InstanceParameters
{
    DeviceManagerDelegate *delegate;
    uint32_t backBufferWidth = 0;
    uint32_t backBufferHeight = 0;
    uint32_t swapChainBufferCount = 3;
    nvrhi::Format swapChainFormat = nvrhi::Format::SRGBA8_UNORM;
    uint32_t maxFramesInFlight = 2;
    bool enableNvrhiValidationLayer = false;
    bool vsyncEnabled = false;
    bool enableRayTracingExtensions = false; // for vulkan
    bool enableComputeQueue = false;
    bool enableCopyQueue = false;

    // Index of the adapter (DX11, DX12) or physical device (Vk) on which to initialize the device.
    // Negative values mean automatic detection.
    // The order of indices matches that returned by DeviceManager::EnumerateAdapters.
    int adapterIndex = -1;

#if USE_DX11 || USE_DX12
    DXGI_USAGE swapChainUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
#endif

#if USE_VULKAN
    std::vector<std::string> requiredVulkanDeviceExtensions;
    std::vector<std::string> optionalVulkanDeviceExtensions;
    std::vector<size_t> ignoredVulkanValidationMessageLocations;

    // This pointer specifies an optional structure to be put at the end of the chain for 'vkGetPhysicalDeviceFeatures2' call.
    // The structure may also be a chain, and must be alive during the device initialization process.
    // The elements of this structure will be populated before 'delegate.deviceCreateInfoCallback' is called,
    // thereby allowing applications to determine if certain features may be enabled on the device.
    void* physicalDeviceFeatures2Extensions = nullptr;
#endif
};

struct AdapterInfo
{
    std::string name;
    uint32_t vendorID = 0;
    uint32_t deviceID = 0;
    uint64_t dedicatedVideoMemory = 0;
#if USE_DX11 || USE_DX12
    nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
#endif
#if USE_VULKAN
    VkPhysicalDevice vkPhysicalDevice = nullptr;
#endif
};

class DeviceManager
{
public:
    static DeviceManager* create(nvrhi::GraphicsAPI api);

    bool createHeadlessDevice(const DeviceCreationParameters& params);
    bool createWindowDeviceAndSwapChain(const DeviceCreationParameters& params);

    // Initializes device-independent objects (DXGI factory, Vulkan instnace).
    // Calling createInstance() is required before enumerateAdapters(), but optional if you don't use enumerateAdapters().
    // Note: if you call createInstance() before Create*Device*(), the values in InstanceParameters must match those
    // in DeviceCreationParameters passed to the device call.
    bool createInstance(const InstanceParameters& params);

    // Enumerates adapters or physical devices present in the system.
    // Note: a call to createInstance() or create*Device*() is required before enumerateAdapters().
    virtual bool enumerateAdapters(std::vector<AdapterInfo>& outAdapters) = 0;

    virtual bool beginFrame() = 0;
    virtual void present() = 0;

    [[nodiscard]] virtual nvrhi::IDevice *getDevice() const = 0;
    [[nodiscard]] virtual const char *getRendererString() const = 0;
    [[nodiscard]] virtual nvrhi::GraphicsAPI getGraphicsAPI() const = 0;

    const DeviceCreationParameters& getDeviceParams() { return m_DeviceParams; }
    [[nodiscard]] bool isVsyncEnabled() const { return m_DeviceParams.vsyncEnabled; }
    virtual void setVsyncEnabled(bool enabled) { m_RequestedVSync = enabled; /* will be processed later */ }
    void requestRecreateSwapchain() { m_RequestedRecreateSwapchain = true; }
    bool isRecreateSwapchainRequested() { return m_RequestedRecreateSwapchain; }

    virtual nvrhi::ITexture* getCurrentBackBuffer() = 0;
    virtual nvrhi::ITexture* getBackBuffer(uint32_t index) = 0;
    virtual uint32_t getCurrentBackBufferIndex() = 0;
    virtual uint32_t getBackBufferCount() = 0;
    nvrhi::IFramebuffer* getCurrentFramebuffer();
    nvrhi::IFramebuffer* getFramebuffer(uint32_t index);
    uint32_t getFramebufferWidth() { return m_DeviceParams.backBufferWidth; }
    uint32_t getFramebufferHeight() { return m_DeviceParams.backBufferHeight; }

    virtual void destroy();
    virtual ~DeviceManager() = default;

    virtual bool isVulkanInstanceExtensionEnabled(const char* extensionName) const { return false; }
    virtual bool isVulkanDeviceExtensionEnabled(const char* extensionName) const { return false; }
    virtual bool isVulkanLayerEnabled(const char* layerName) const { return false; }
    virtual void getEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const { }
    virtual void getEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const { }
    virtual void getEnabledVulkanLayers(std::vector<std::string>& layers) const { }

protected:
    DeviceCreationParameters m_DeviceParams;
    bool m_RequestedVSync = false;
    bool m_InstanceCreated = false;
    std::atomic<bool> m_RequestedRecreateSwapchain;

    std::vector<nvrhi::FramebufferHandle> m_SwapChainFramebuffers;

    void logMessage(nvrhi::MessageSeverity severity, const char *fmt, ...) const;

    DeviceManager() = default;

    void releaseFramebuffers();
    void createFramebuffers();
    nvrhi::TextureHandle createDepthBuffer();
    void maybeRecreateSwapchain();

    // device-specific methods
    virtual bool createInstanceInternal() = 0;
    virtual bool createDevice() = 0;
    virtual bool createSwapChain() = 0;
    virtual void destroyDeviceAndSwapChain() = 0;
    virtual void resizeSwapChain() = 0;

private:
#if USE_DX11
    static DeviceManager* createD3D11();
#endif
#if USE_DX12
    static DeviceManager* createD3D12();
#endif
    static DeviceManager* createVK();
};
