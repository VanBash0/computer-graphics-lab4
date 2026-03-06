#include "gbuffer.h"
#include "fail_checker.h"

void GBuffer::buildResources() {
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mWidth;
    texDesc.Height = mHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    CD3DX12_CLEAR_VALUE optClear(DXGI_FORMAT_UNKNOWN, clearColor);

    for (int i = 0; i < mTexturesNum; ++i) {
        texDesc.Format = mFormats[i];
        optClear.Format = mFormats[i];

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        failCheck(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            &optClear,
            IID_PPV_ARGS(&mTextures[i])));
    }
}

GBuffer::GBuffer(ID3D12Device* device, UINT width, UINT height) : device(device), mWidth(width), mHeight(height) {
    buildResources();
}

void GBuffer::buildDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
    UINT cbvSrvDescriptorSize, UINT rtvDescriptorSize) {

    mhCpuSrv = hCpuSrv;
    mhGpuSrv = hGpuSrv;
    mhCpuRtv = hCpuRtv;

    mCbvSrvDescriptorSize = cbvSrvDescriptorSize;
    mRtvDescriptorSize = rtvDescriptorSize;

    for (UINT i = 0; i < mTexturesNum; ++i) {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mhCpuRtv, i, mRtvDescriptorSize);
        device->CreateRenderTargetView(mTextures[i].Get(), nullptr, rtvHandle);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mhCpuSrv, i, mCbvSrvDescriptorSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = mFormats[i];
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        device->CreateShaderResourceView(mTextures[i].Get(), &srvDesc, srvHandle);
    }
}

CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::getRtvHandle(int index) {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(mhCpuRtv, index, mRtvDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::getSrvHandle() {
    return mhGpuSrv;
}

void GBuffer::onResize(UINT newWidth, UINT newHeight) {
    if (mWidth != newWidth || mHeight != newHeight) {
        mWidth = newWidth;
        mHeight = newHeight;
        buildResources();
        buildDescriptors(mhCpuSrv, mhGpuSrv, mhCpuRtv, mCbvSrvDescriptorSize, mRtvDescriptorSize);
    }
}