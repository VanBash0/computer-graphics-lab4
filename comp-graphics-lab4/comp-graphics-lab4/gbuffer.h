#ifndef GBUFFER_H
#define GBUFFER_H

#include "d3dutil.h"

class GBuffer {
public:
    GBuffer(ID3D12Device* device, UINT width, UINT height);

    static const int mTexturesNum = 2;

    ID3D12Resource* getResource(int index) { return mTextures[index].Get(); }

    CD3DX12_CPU_DESCRIPTOR_HANDLE getRtvHandle(int index);
    CD3DX12_GPU_DESCRIPTOR_HANDLE getSrvHandle();

    void buildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
        UINT cbvSrvDescriptorSize,
        UINT rtvDescriptorSize);

    void onResize(UINT newWidth, UINT newHeight);
private:
    void buildResources();

    ID3D12Device* device = nullptr;
    UINT mWidth, mHeight;

    Microsoft::WRL::ComPtr<ID3D12Resource> mTextures[mTexturesNum];

    DXGI_FORMAT mFormats[mTexturesNum] = {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
    };

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;
    UINT mCbvSrvDescriptorSize;
    UINT mRtvDescriptorSize;
};

#endif // GBUFFER_H