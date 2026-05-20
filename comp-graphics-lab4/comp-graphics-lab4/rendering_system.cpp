#include "rendering_system.h"
#include "fail_checker.h"

RenderingSystem::RenderingSystem(ID3D12Device* device, UINT width, UINT height) : md3dDevice(device), mWidth(width), mHeight(height) {
    mGBuffer = std::make_unique<GBuffer>(device, width, height);
    buildLightingBuffer();
}

void RenderingSystem::buildLightingBuffer() {
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = mWidth;
    texDesc.Height = mHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mLightingFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    CD3DX12_CLEAR_VALUE clearValue(mLightingFormat, clearColor);
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    failCheck(md3dDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue, IID_PPV_ARGS(&mLightingBuffer)));
}

void RenderingSystem::onResize(UINT newWidth, UINT newHeight) {
    mGBuffer->onResize(newWidth, newHeight);
    if (mWidth != newWidth || mHeight != newHeight) {
        mWidth = newWidth;
        mHeight = newHeight;
        buildLightingBuffer();

        md3dDevice->CreateRenderTargetView(mLightingBuffer.Get(), nullptr, mhLightingCpuRtv);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = mLightingFormat;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        md3dDevice->CreateShaderResourceView(mLightingBuffer.Get(), &srvDesc, mhLightingCpuSrv);
    }
}

void RenderingSystem::buildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
    UINT cbvSrvDescriptorSize, UINT rtvDescriptorSize) {
    mCbvSrvDescriptorSize = cbvSrvDescriptorSize;
    mRtvDescriptorSize = rtvDescriptorSize;

    mGBuffer->buildDescriptors(hCpuSrv, hGpuSrv, hCpuRtv, cbvSrvDescriptorSize, rtvDescriptorSize);

    mhLightingCpuSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(hCpuSrv, GBuffer::mTexturesNum, cbvSrvDescriptorSize);
    mhLightingGpuSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(hGpuSrv, GBuffer::mTexturesNum, cbvSrvDescriptorSize);
    mhLightingCpuRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(hCpuRtv, GBuffer::mTexturesNum, rtvDescriptorSize);

    md3dDevice->CreateRenderTargetView(mLightingBuffer.Get(), nullptr, mhLightingCpuRtv);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mLightingFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    md3dDevice->CreateShaderResourceView(mLightingBuffer.Get(), &srvDesc, mhLightingCpuSrv);
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

void RenderingSystem::beginLightingPass(ID3D12GraphicsCommandList* cmdList) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mLightingBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barrier);
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(mhLightingCpuRtv, clearColor, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &mhLightingCpuRtv, FALSE, nullptr);
}

void RenderingSystem::endLightingPass(ID3D12GraphicsCommandList* cmdList) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mLightingBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &barrier);
}

void RenderingSystem::beginPostProcessPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv) {
    cmdList->OMSetRenderTargets(1, &backBufferRtv, FALSE, nullptr);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE RenderingSystem::getLightingSrvHandle() const {
    return mhLightingGpuSrv;
}
