#ifndef D3DAPP_H
#define D3DAPP_H

#include <d3d12.h>
#include <dxgi1_6.h>
#include "wrl.h"

using namespace Microsoft::WRL;

class D3DApp {
public:
    void initialize();
    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() const;
private:
    static const int swapChainBufferCount = 2;

    ComPtr<IDXGIFactory4> mdxgiFactory;
    ComPtr<ID3D12Device> md3dDevice;
    ComPtr<ID3D12Fence> mFence;
    ComPtr<ID3D12CommandQueue> mCommandQueue;
    ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;
    ComPtr<IDXGISwapChain> mSwapChain;
    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12DescriptorHeap> mDsvHeap;
    ComPtr<ID3D12Resource> mSwapChainBuffer[swapChainBufferCount];

    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT mRtvDescriptorSize;
    UINT mDsvDescriptorSize;
    UINT mCbvSrvDescriptorSize;
    UINT m4xMsaaQuality;

    UINT mClientWidth = 800;
    UINT mClientHeight = 600;
    bool m4xMsaaState = true;
    HWND mhMainWnd = nullptr;
    int mCurrBackBuffer = 0;

    void enableDebugLayer();
    void createDXGIFactory();
    void createDevice();
    void setFenceAndDescriptorsSize();
    void checkMSAASupport();
    void createCommandObjects();
    void createSwapChain();
    void createDescriptorHeaps();
    void createRenderTargetViews();
    void createDepthStencilBufferView();
};

#endif // D3DAPP_H