#ifndef D3DUTIL_H
#define D3DUTIL_H

#include <wrl.h>
#include "d3dx12.h"

using namespace Microsoft::WRL;

class D3DUtil {
public:
    static ComPtr<ID3D12Resource> createDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        ComPtr<ID3D12Resource>& uploadBuffer);
};

#endif // D3DUTIL_H