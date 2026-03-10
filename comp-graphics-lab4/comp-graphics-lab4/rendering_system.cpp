#include "rendering_system.h"

RenderingSystem::RenderingSystem(ID3D12Device* device, UINT width, UINT height) : md3dDevice(device) {
    mGBuffer = std::make_unique<GBuffer>(device, width, height);
}

void RenderingSystem::onResize(UINT newWidth, UINT newHeight) {
    mGBuffer->onResize(newWidth, newHeight);
}

void RenderingSystem::buildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
    UINT cbvSrvDescriptorSize, UINT rtvDescriptorSize) {
    mGBuffer->buildDescriptors(hCpuSrv, hGpuSrv, hCpuRtv, cbvSrvDescriptorSize, rtvDescriptorSize);
}

void RenderingSystem::beginGeometryPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) {
    CD3DX12_RESOURCE_BARRIER barriers[GBuffer::mTexturesNum];
    for (int i = 0; i < GBuffer::mTexturesNum; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(mGBuffer->getResource(i), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    cmdList->ResourceBarrier(GBuffer::mTexturesNum, barriers);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[GBuffer::mTexturesNum];
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    for (int i = 0; i < GBuffer::mTexturesNum; ++i) {
        rtvHandles[i] = mGBuffer->getRtvHandle(i);
        cmdList->ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
    }

    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(GBuffer::mTexturesNum, &rtvHandles[0], TRUE, &dsvHandle);
}

void RenderingSystem::endGeometryPass(ID3D12GraphicsCommandList* cmdList) {
    CD3DX12_RESOURCE_BARRIER barriers[GBuffer::mTexturesNum];
    for (int i = 0; i < GBuffer::mTexturesNum; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(mGBuffer->getResource(i), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
    }
    cmdList->ResourceBarrier(GBuffer::mTexturesNum, barriers);
}

void RenderingSystem::beginLightingPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv) {
    cmdList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
}