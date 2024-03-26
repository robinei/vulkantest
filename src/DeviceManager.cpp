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

#include "DeviceManager.h"
#include <nvrhi/utils.h>

static StdoutLogger stdoutLogger;

bool DeviceManager::createInstance(const InstanceParameters& params)
{
    if (m_InstanceCreated)
        return true;

    static_cast<InstanceParameters&>(m_DeviceParams) = params;
    m_MessageCallback.logger = params.logger ? params.logger : &stdoutLogger;

    m_InstanceCreated = createInstanceInternal();
    return m_InstanceCreated;
}

bool DeviceManager::createHeadlessDevice(const DeviceCreationParameters& params)
{
    m_DeviceParams = params;
    m_DeviceParams.headlessDevice = true;

    if (!createInstance(m_DeviceParams))
        return false;

    return createDevice();
}

bool DeviceManager::createWindowDeviceAndSwapChain(const DeviceCreationParameters& params)
{
    m_DeviceParams = params;
    m_DeviceParams.headlessDevice = false;
    m_RequestedVSync = params.vsyncEnabled;

    if (!createInstance(m_DeviceParams))
        return false;

    if (!createDevice())
        return false;

    if (!createSwapChain())
        return false;

    createFramebuffers();

    return true;
}

void DeviceManager::releaseFramebuffers()
{
    m_SwapChainFramebuffers.clear();
}

void DeviceManager::createFramebuffers()
{
    uint32_t backBufferCount = getBackBufferCount();
    m_SwapChainFramebuffers.resize(backBufferCount);
    for (uint32_t index = 0; index < backBufferCount; index++)
    {
        m_SwapChainFramebuffers[index] = getDevice()->createFramebuffer(
            nvrhi::FramebufferDesc().addColorAttachment(getBackBuffer(index)).setDepthAttachment(createDepthBuffer()));
    }
}

nvrhi::TextureHandle DeviceManager::createDepthBuffer()
{
    bool useReverseProjection = false;
    uint32_t sampleCount = 1;
    
    const nvrhi::Format depthFormats[] = {
        nvrhi::Format::D24S8,
        nvrhi::Format::D32S8,
        nvrhi::Format::D32,
        nvrhi::Format::D16 };

    const nvrhi::FormatSupport depthFeatures = 
        nvrhi::FormatSupport::Texture |
        nvrhi::FormatSupport::DepthStencil |
        nvrhi::FormatSupport::ShaderLoad;

    nvrhi::TextureDesc desc;
    desc.width = m_DeviceParams.backBufferWidth;
    desc.height = m_DeviceParams.backBufferHeight;
    desc.isRenderTarget = true;
    desc.useClearValue = true;
    desc.sampleCount = sampleCount;
    desc.dimension = sampleCount > 1 ? nvrhi::TextureDimension::Texture2DMS : nvrhi::TextureDimension::Texture2D;
    desc.keepInitialState = true;
    desc.isUAV = false;
    desc.mipLevels = 1;
    desc.format = nvrhi::utils::ChooseFormat(getDevice(), depthFeatures, depthFormats, 4);
    desc.isTypeless = true;
    desc.initialState = nvrhi::ResourceStates::DepthWrite;
    desc.clearValue = useReverseProjection ? nvrhi::Color(0.f) : nvrhi::Color(1.f);
    desc.debugName = "Depth";
    return getDevice()->createTexture(desc);
}

void DeviceManager::maybeRecreateSwapchain()
{
    if (m_RequestedRecreateSwapchain || (m_DeviceParams.vsyncEnabled != m_RequestedVSync && getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN))
    {
        releaseFramebuffers();

        m_DeviceParams.vsyncEnabled = m_RequestedVSync;

        resizeSwapChain();
        createFramebuffers();
    }

    m_RequestedRecreateSwapchain = false;
    m_DeviceParams.vsyncEnabled = m_RequestedVSync;
}

void DeviceManager::destroy()
{
    m_SwapChainFramebuffers.clear();

    destroyDeviceAndSwapChain();

    m_InstanceCreated = false;
}

nvrhi::IFramebuffer* DeviceManager::getCurrentFramebuffer()
{
    return getFramebuffer(getCurrentBackBufferIndex());
}

nvrhi::IFramebuffer* DeviceManager::getFramebuffer(uint32_t index)
{
    if (index < m_SwapChainFramebuffers.size())
        return m_SwapChainFramebuffers[index];

    return nullptr;
}

DeviceManager* DeviceManager::create(nvrhi::GraphicsAPI api)
{
    switch (api)
    {
#if USE_DX11
    case nvrhi::GraphicsAPI::D3D11:
        return createD3D11();
#endif
#if USE_DX12
    case nvrhi::GraphicsAPI::D3D12:
        return createD3D12();
#endif
#if USE_VULKAN
    case nvrhi::GraphicsAPI::VULKAN:
        return createVK();
#endif
    default:
        assert(false);
        return nullptr;
    }
}
