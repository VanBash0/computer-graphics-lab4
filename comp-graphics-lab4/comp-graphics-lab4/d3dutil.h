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
    static UINT64 calcConstantBufferByteSize(UINT64 byteSize) {
        return (byteSize + 255) & ~255;
    }
    static ComPtr<ID3DBlob> compileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target);
};

#endif // D3DUTIL_H