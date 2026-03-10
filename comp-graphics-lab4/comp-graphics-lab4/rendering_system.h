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
    void beginLightingPass(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
private:
    std::unique_ptr<GBuffer> mGBuffer;
    ID3D12Device* md3dDevice = nullptr;
};

#endif // RENDERING_SYSTEM_H