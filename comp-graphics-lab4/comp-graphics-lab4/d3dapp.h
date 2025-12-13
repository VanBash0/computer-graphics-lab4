#ifndef D3DAPP_H
#define D3DAPP_H

#include <d3d12.h>
#include <dxgi1_6.h>
#include "wrl.h"
#include "game_timer.h"
#include <string>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

class D3DApp {
public:
    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    ~D3DApp();

    void initializeDX();
    int run();

    bool initMainWindow(HINSTANCE hInstance, int nCmdShow);
    LRESULT handleMsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onResize();

    static D3DApp* getApp() { return mApp; }
    HWND getMainWnd() const { return mhMainWnd; }
    HINSTANCE getAppInst() const { return mhAppInst; }
    float getAspectRatio() const { return static_cast<float>(mClientWidth) / static_cast<float>(mClientHeight); }

    bool get4xMsaaState() const { return m4xMsaaState; }
    void set4xMsaaState(bool value) { m4xMsaaState = value; }

    ID3D12Resource* getCurrentBackBufferResource() const { return mSwapChainBuffer[mCurrBackBuffer].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() const;

    void flushCommandQueue();
    void calculateFrameStats();
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
    ComPtr<ID3D12Resource> mDepthStencilBuffer;

    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    D3D12_VIEWPORT mViewport{};
    D3D12_RECT mScissorRect {};
    UINT mRtvDescriptorSize = 0;
    UINT mDsvDescriptorSize = 0;
    UINT mCbvSrvDescriptorSize = 0;
    UINT m4xMsaaQuality = 0;

    HINSTANCE mhAppInst = nullptr;
    HWND mhMainWnd = nullptr;
    std::wstring mMainWndCaption = L"The App";
    GameTimer mTimer;

    UINT64 mCurrentFence = 0;
    UINT mClientWidth = 800;
    UINT mClientHeight = 600;
    bool m4xMsaaState = false;
    int mCurrBackBuffer = 0;

    bool mAppPaused = false;
    bool mMinimized = false;
    bool mMaximized = false;
    bool mResizing = false;
    bool mFullscreenState = false;

    static D3DApp* mApp;

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
    void setDepthBufferBeingDepthBuffer();
    void setViewport();
    void setScissorRect();

    void onMouseDown(WPARAM btnState, int x, int y) {};
    void onMouseUp(WPARAM btnState, int x, int y) {};
    void onMouseMove(WPARAM btnState, int x, int y) {};
    void update(const GameTimer& gt) {};
    void draw(const GameTimer& gt) {};
};

#endif // D3DAPP_H