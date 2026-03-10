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

void RenderingSystem::beginGeometryPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, ID3D12Resource* depthBuffer) {
    CD3DX12_RESOURCE_BARRIER barriers[GBuffer::mTexturesNum + 1];
    for (int i = 0; i < GBuffer::mTexturesNum; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(mGBuffer->getResource(i),
            D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    barriers[GBuffer::mTexturesNum] = CD3DX12_RESOURCE_BARRIER::Transition(depthBuffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    cmdList->ResourceBarrier(GBuffer::mTexturesNum + 1, barriers);

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    for (int i = 0; i < GBuffer::mTexturesNum; ++i) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = mGBuffer->getRtvHandle(i);
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
<<<<<<< Updated upstream

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvStart = mGBuffer->getRtvHandle(0);
    cmdList->OMSetRenderTargets(GBuffer::mTexturesNum, &rtvStart, TRUE, &dsvHandle);
=======
    cmdList->OMSetRenderTargets(GBuffer::mTexturesNum, &rtvHandles[0], TRUE, &dsvHandle);
>>>>>>> Stashed changes
}

void RenderingSystem::endGeometryPass(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* depthBuffer) {
    CD3DX12_RESOURCE_BARRIER barriers[GBuffer::mTexturesNum + 1];
    for (int i = 0; i < GBuffer::mTexturesNum; ++i) {
        barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(mGBuffer->getResource(i), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    barriers[GBuffer::mTexturesNum] = CD3DX12_RESOURCE_BARRIER::Transition(depthBuffer,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

    cmdList->ResourceBarrier(GBuffer::mTexturesNum + 1, barriers);
}

void RenderingSystem::beginLightingPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv) {
    cmdList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
}