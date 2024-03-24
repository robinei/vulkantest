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
#endif

#include <nvrhi/nvrhi.h>

#include <functional>
#include <atomic>

#include "Logger.h"


struct InstanceParameters
{
    Logger *logger = nullptr;
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
    uint32_t backBufferWidth = 1280;
    uint32_t backBufferHeight = 720;
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
    std::function<void(VkDeviceCreateInfo&)> deviceCreateInfoCallback;
    std::function<bool(VkInstance, VkSurfaceKHR*)> createSurfaceCallback;

    // This pointer specifies an optional structure to be put at the end of the chain for 'vkGetPhysicalDeviceFeatures2' call.
    // The structure may also be a chain, and must be alive during the device initialization process.
    // The elements of this structure will be populated before 'deviceCreateInfoCallback' is called,
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
    static DeviceManager* Create(nvrhi::GraphicsAPI api);

    bool CreateHeadlessDevice(const DeviceCreationParameters& params);
    bool CreateWindowDeviceAndSwapChain(const DeviceCreationParameters& params);

    // Initializes device-independent objects (DXGI factory, Vulkan instnace).
    // Calling CreateInstance() is required before EnumerateAdapters(), but optional if you don't use EnumerateAdapters().
    // Note: if you call CreateInstance() before Create*Device*(), the values in InstanceParameters must match those
    // in DeviceCreationParameters passed to the device call.
    bool CreateInstance(const InstanceParameters& params);

    // Enumerates adapters or physical devices present in the system.
    // Note: a call to CreateInstance() or Create*Device*() is required before EnumerateAdapters().
    virtual bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) = 0;

    void MaybeRecreateSwapchain(); // call before BeginFrame()

    virtual bool BeginFrame() = 0;
    virtual void Present() = 0;

    [[nodiscard]] virtual nvrhi::IDevice *GetDevice() const = 0;
    [[nodiscard]] virtual const char *GetRendererString() const = 0;
    [[nodiscard]] virtual nvrhi::GraphicsAPI GetGraphicsAPI() const = 0;

    const DeviceCreationParameters& GetDeviceParams();
    [[nodiscard]] bool IsVsyncEnabled() const { return m_DeviceParams.vsyncEnabled; }
    virtual void SetVsyncEnabled(bool enabled) { m_RequestedVSync = enabled; /* will be processed later */ }
    void SetResized() { m_Resized = true; }

    virtual nvrhi::ITexture* GetCurrentBackBuffer() = 0;
    virtual nvrhi::ITexture* GetBackBuffer(uint32_t index) = 0;
    virtual uint32_t GetCurrentBackBufferIndex() = 0;
    virtual uint32_t GetBackBufferCount() = 0;
    nvrhi::IFramebuffer* GetCurrentFramebuffer();
    nvrhi::IFramebuffer* GetFramebuffer(uint32_t index);
    uint32_t GetFramebufferWidth() { return m_DeviceParams.backBufferWidth; }
    uint32_t GetFramebufferHeight() { return m_DeviceParams.backBufferHeight; }

    virtual void Destroy();
    virtual ~DeviceManager() = default;

    virtual bool IsVulkanInstanceExtensionEnabled(const char* extensionName) const { return false; }
    virtual bool IsVulkanDeviceExtensionEnabled(const char* extensionName) const { return false; }
    virtual bool IsVulkanLayerEnabled(const char* layerName) const { return false; }
    virtual void GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const { }
    virtual void GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const { }
    virtual void GetEnabledVulkanLayers(std::vector<std::string>& layers) const { }

protected:
    DeviceCreationParameters m_DeviceParams;
    bool m_RequestedVSync = false;
    bool m_InstanceCreated = false;
    std::atomic<bool> m_Resized;

    std::vector<nvrhi::FramebufferHandle> m_SwapChainFramebuffers;

    DeviceManager() = default;

    void ReleaseFramebuffers();
    void CreateFramebuffers();
    nvrhi::TextureHandle CreateDepthBuffer();

    // device-specific methods
    virtual bool CreateInstanceInternal() = 0;
    virtual bool CreateDevice() = 0;
    virtual bool CreateSwapChain() = 0;
    virtual void DestroyDeviceAndSwapChain() = 0;
    virtual void ResizeSwapChain() = 0;

    struct MessageCallback : public nvrhi::IMessageCallback {
        Logger *logger = nullptr;

        void message(nvrhi::MessageSeverity severity, const char* messageText) override {
            switch (severity) {
            case nvrhi::MessageSeverity::Info:
                logger->logMessage(Logger::LogLevel::Info, messageText);
                break;
            case nvrhi::MessageSeverity::Warning:
                logger->logMessage(Logger::LogLevel::Warning, messageText);
                break;
            case nvrhi::MessageSeverity::Error:
                logger->logMessage(Logger::LogLevel::Error, messageText);
                break;
            case nvrhi::MessageSeverity::Fatal:
                logger->logMessage(Logger::LogLevel::Critical, messageText);
                break;
            }
        }
    } m_MessageCallback;

    Logger *logger() const { return m_MessageCallback.logger; }

private:
#if USE_DX11
    static DeviceManager* CreateD3D11();
#endif
#if USE_DX12
    static DeviceManager* CreateD3D12();
#endif
    static DeviceManager* CreateVK();
};
