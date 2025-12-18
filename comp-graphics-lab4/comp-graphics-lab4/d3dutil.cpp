#include "d3dutil.h"
#include "fail_checker.h"

ComPtr<ID3D12Resource> D3DUtil::createDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
        const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer) {
    ComPtr<ID3D12Resource> defaultBuffer;
    failCheck(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
        D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize), D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));
    failCheck(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize), D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
    return defaultBuffer;
}