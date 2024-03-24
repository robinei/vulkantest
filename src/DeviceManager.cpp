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

static StdoutLogger stdoutLogger;

bool DeviceManager::CreateInstance(const InstanceParameters& params)
{
    if (m_InstanceCreated)
        return true;

    static_cast<InstanceParameters&>(m_DeviceParams) = params;
    m_MessageCallback.logger = params.logger ? params.logger : &stdoutLogger;

    m_InstanceCreated = CreateInstanceInternal();
    return m_InstanceCreated;
}

bool DeviceManager::CreateHeadlessDevice(const DeviceCreationParameters& params)
{
    m_DeviceParams = params;
    m_DeviceParams.headlessDevice = true;

    if (!CreateInstance(m_DeviceParams))
        return false;

    return CreateDevice();
}

bool DeviceManager::CreateWindowDeviceAndSwapChain(const DeviceCreationParameters& params)
{
    m_DeviceParams = params;
    m_DeviceParams.headlessDevice = false;
    m_RequestedVSync = params.vsyncEnabled;

    if (!CreateInstance(m_DeviceParams))
        return false;

    if (!CreateDevice())
        return false;

    if (!CreateSwapChain())
        return false;

    CreateFramebuffers();

    return true;
}

void DeviceManager::ReleaseFramebuffers()
{
    m_SwapChainFramebuffers.clear();
}

void DeviceManager::CreateFramebuffers()
{
    uint32_t backBufferCount = GetBackBufferCount();
    m_SwapChainFramebuffers.resize(backBufferCount);
    for (uint32_t index = 0; index < backBufferCount; index++)
    {
        m_SwapChainFramebuffers[index] = GetDevice()->createFramebuffer(
            nvrhi::FramebufferDesc().addColorAttachment(GetBackBuffer(index)));
    }
}

const DeviceCreationParameters& DeviceManager::GetDeviceParams()
{
    return m_DeviceParams;
}

void DeviceManager::UpdateWindowSize()
{
    if (m_Resized || (m_DeviceParams.vsyncEnabled != m_RequestedVSync && GetGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN))
    {
        ReleaseFramebuffers();

        m_DeviceParams.vsyncEnabled = m_RequestedVSync;

        ResizeSwapChain();
        CreateFramebuffers();
    }

    m_Resized = false;
    m_DeviceParams.vsyncEnabled = m_RequestedVSync;
}

void DeviceManager::Destroy()
{
    m_SwapChainFramebuffers.clear();

    DestroyDeviceAndSwapChain();

    m_InstanceCreated = false;
}

nvrhi::IFramebuffer* DeviceManager::GetCurrentFramebuffer()
{
    return GetFramebuffer(GetCurrentBackBufferIndex());
}

nvrhi::IFramebuffer* DeviceManager::GetFramebuffer(uint32_t index)
{
    if (index < m_SwapChainFramebuffers.size())
        return m_SwapChainFramebuffers[index];

    return nullptr;
}

DeviceManager* DeviceManager::Create(nvrhi::GraphicsAPI api)
{
    switch (api)
    {
#if USE_DX11
    case nvrhi::GraphicsAPI::D3D11:
        return CreateD3D11();
#endif
#if USE_DX12
    case nvrhi::GraphicsAPI::D3D12:
        return CreateD3D12();
#endif
#if USE_VULKAN
    case nvrhi::GraphicsAPI::VULKAN:
        return CreateVK();
#endif
    default:
        assert(false);
        return nullptr;
    }
}
