#ifndef TEXTURE_H
#define TEXTURE_H

#include <d3d12.h>
#include <string>
#include <wrl.h>

struct Texture {
    std::wstring fileName;
    std::wstring filePath;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr; 
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap = nullptr;
    UINT srvHeapIndex = 0;
};

#endif // TEXTURE_H