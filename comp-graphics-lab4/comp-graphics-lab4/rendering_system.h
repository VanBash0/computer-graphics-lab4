#ifndef RENDERING_SYSTEM_H
#define RENDERING_SYSTEM_H

#include "d3dutil.h"
#include "gbuffer.h"
#include <memory>

class RenderingSystem {
public:
    RenderingSystem(ID3D12Device* device, UINT width, UINT height);

    GBuffer* getGBuffer() const { return mGBuffer.get(); }

    void buildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
        UINT cbvSrvDescriptorSize, UINT rtvDescriptorSize);

    void onResize(UINT newWidth, UINT newHeight);
    
    void beginGeometryPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void endGeometryPass(ID3D12GraphicsCommandList* cmdList);
    void beginLightingPass(ID3D12GraphicsCommandList* cmdList);
    void bindLightingTarget(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void endLightingPass(ID3D12GraphicsCommandList* cmdList);
    void beginPostProcessPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv);

    CD3DX12_GPU_DESCRIPTOR_HANDLE getLightingSrvHandle() const;
private:
    void buildLightingBuffer();

    std::unique_ptr<GBuffer> mGBuffer;
    ID3D12Device* md3dDevice = nullptr;
    UINT mWidth = 0;
    UINT mHeight = 0;

    ComPtr<ID3D12Resource> mLightingBuffer;
    DXGI_FORMAT mLightingFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhLightingCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhLightingGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhLightingCpuRtv;
    UINT mCbvSrvDescriptorSize = 0;
    UINT mRtvDescriptorSize = 0;
};

#endif // RENDERING_SYSTEM_H
