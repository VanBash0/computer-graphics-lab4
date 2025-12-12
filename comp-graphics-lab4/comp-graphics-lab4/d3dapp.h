#ifndef D3DAPP_H
#define D3DAPP_H

#include <d3d12.h>
#include <dxgi1_6.h>
#include "wrl.h"

using namespace Microsoft::WRL;

class D3DApp {
public:
    void initialize();
private:
    ComPtr<IDXGIFactory4> mdxgiFactory;
    ComPtr<ID3D12Device> md3dDevice;

    void enableDebugLayer();
    void createDXGIFactory();
    void createDevice();
};

#endif // D3DAPP_H