#include "d3dapp.h"
#include "d3dx12.h"
#include "fail_checker.h"
#include <cassert>
#include <windowsx.h>
#include "vertex.h"
#include "d3dutil.h"
#include <DirectXColors.h>
#include <filesystem>
#include "directxtex/DirectXTex.h"
using namespace DirectX;

D3DApp* D3DApp::mApp = nullptr;

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return D3DApp::getApp()->handleMsgProc(hwnd, msg, wParam, lParam);
}

void D3DApp::initializeDX() {
    enableDebugLayer();
    createDXGIFactory();
    createDevice();
    setFenceAndDescriptorsSize();
    checkMSAASupport();
    createCommandObjects();
    createSwapChain();
    createDescriptorHeaps();
    onResize();
}

void D3DApp::enableDebugLayer() {
    #if defined(_DEBUG) || defined(DEBUG)
    {
    ComPtr<ID3D12Debug> debugController;
    failCheck(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
    debugController->EnableDebugLayer();
    }
    #endif
}

void D3DApp::createDXGIFactory() {
    failCheck(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
}

void D3DApp::createDevice() {
    HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&md3dDevice));
    if (FAILED(hardwareResult)) {
        ComPtr<IDXGIAdapter> pWarpAdapter;
        failCheck(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
        failCheck(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&md3dDevice)));
    }
}

void D3DApp::setFenceAndDescriptorsSize() {
    failCheck(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DApp::checkMSAASupport() {
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = mBackBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    failCheck(md3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));
    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

void D3DApp::createCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    failCheck(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
    failCheck(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));
    failCheck(md3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mDirectCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));
    mCommandList->Close();
}

void D3DApp::createSwapChain() {
    mSwapChain.Reset();
    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = mClientWidth;
    sd.BufferDesc.Height = mClientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = mBackBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = swapChainBufferCount;
    sd.OutputWindow = mhMainWnd;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    failCheck(mdxgiFactory->CreateSwapChain(mCommandQueue.Get(), &sd, mSwapChain.GetAddressOf()));
}

void D3DApp::createDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = swapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    failCheck(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    failCheck(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::getCurrentBackBuffer() const {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize
    );
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::getDepthStencilView() const {
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::createRenderTargetViews() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < swapChainBufferCount; ++i) {
        failCheck(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf())));
        md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, mRtvDescriptorSize);
    }
}

void D3DApp::createDepthStencilBufferView() {
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = mDepthStencilFormat;
    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    failCheck(md3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    setDepthBufferBeingDepthBuffer();
}

void D3DApp::setDepthBufferBeingDepthBuffer() {
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, getDepthStencilView());

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mCommandList->ResourceBarrier(1, &barrier);
}

void D3DApp::setViewport() {
    mViewport.TopLeftX = 0.0f;
    mViewport.TopLeftY = 0.0f;
    mViewport.Width = static_cast<float>(mClientWidth);
    mViewport.Height = static_cast<float>(mClientHeight);
    mViewport.MinDepth = 0.0f;
    mViewport.MaxDepth = 1.0f;
    mCommandList->RSSetViewports(1, &mViewport);
}

void D3DApp::setScissorRect() {
    mScissorRect.left = 0;
    mScissorRect.top = 0;
    mScissorRect.right = mClientWidth;
    mScissorRect.bottom = mClientHeight;
    mCommandList->RSSetScissorRects(1, &mScissorRect);
}

D3DApp::D3DApp(HINSTANCE hInstance) {
    assert(mApp == nullptr);
    mApp = this;
    mhAppInst = hInstance;
}

D3DApp::~D3DApp() {
    if (md3dDevice != nullptr) {
        flushCommandQueue();
        mApp = nullptr;
    }
}

bool D3DApp::initMainWindow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASS wnd = {};
    wnd.style = CS_HREDRAW | CS_VREDRAW;
    wnd.lpfnWndProc = MainWndProc;
    wnd.cbClsExtra = 0;
    wnd.cbWndExtra = 0;
    wnd.hInstance = hInstance;
    wnd.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wnd.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wnd.lpszClassName = L"MainWnd";
    RegisterClass(&wnd);

    RECT R = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);

    mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, R.right - R.left, R.bottom - R.top,
        nullptr, nullptr, mhAppInst, nullptr);

    if (!mhMainWnd) {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }

    ShowWindow(mhMainWnd, nCmdShow);
    UpdateWindow(mhMainWnd);
    return true;
}

LRESULT D3DApp::handleMsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            mAppPaused = true;
            mTimer.stop();
        }
        else {
            mAppPaused = false;
            mTimer.start();
        }
        return 0;

    case WM_SIZE:
        mClientWidth = LOWORD(lParam);
        mClientHeight = HIWORD(lParam);
        if (md3dDevice) {
            if (wParam == SIZE_MINIMIZED) {
                mMinimized = true;
                mMaximized = false;
                mAppPaused = true;
            }
            else if (wParam == SIZE_MAXIMIZED) {
                mMinimized = false;
                mMaximized = true;
                mAppPaused = false;
                onResize();
            }
            else if (wParam == SIZE_RESTORED) {
                if (mMinimized) {
                    mMinimized = false;
                    onResize();
                }
                else if (mMaximized) {
                    mMaximized = false;
                    onResize();
                }
                else if (mResizing) {
                }
                else onResize();
            }
        }
        return 0;

    case WM_ENTERSIZEMOVE:
        mAppPaused = true;
        mResizing = true;
        mTimer.stop();
        return 0;

    case WM_EXITSIZEMOVE:
        mAppPaused = false;
        mResizing = false;
        mTimer.start();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        onMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        onMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        onMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_KEYUP:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if ((int)wParam == VK_F2)
            set4xMsaaState(!m4xMsaaState);

        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void D3DApp::onResize() {
    assert(md3dDevice);
    assert(mSwapChain);

    flushCommandQueue();

    for (int i = 0; i < swapChainBufferCount; ++i)
        mSwapChainBuffer[i].Reset();

    mDepthStencilBuffer.Reset();

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    mSwapChain->GetDesc(&swapChainDesc);

    failCheck(mSwapChain->ResizeBuffers(swapChainBufferCount, mClientWidth,
        mClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

    mCurrBackBuffer = 0;

    createRenderTargetViews();
    createDepthStencilBufferView();
    setViewport();
    setScissorRect();
}

int D3DApp::run() {
    MSG msg = {};
    mTimer.reset();
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            mTimer.tick();
            if (!mAppPaused) {
                update(mTimer);
                draw(mTimer);
                calculateFrameStats();
            }
            else {
                Sleep(100);
            }
        }
    }
    return static_cast<int>(msg.wParam);
}

void D3DApp::flushCommandQueue() {
    mCurrentFence++;
    failCheck(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        failCheck(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void D3DApp::calculateFrameStats() {
    static int frameCount = 0;
    static float timeElapsed = 0.0f;
    frameCount++;
    if ((mTimer.getTotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = static_cast<float>(frameCount);
        float mspf = 1000.0f / fps;
        std::wstring fpsStr = std::to_wstring(fps);
        std::wstring mspfStr = std::to_wstring(mspf);
        std::wstring windowTitle = mMainWndCaption + L"    FPS: " + fpsStr + L"    MSPF: " + mspfStr;
        SetWindowText(mhMainWnd, windowTitle.c_str());
        frameCount = 0;
        timeElapsed += 1.0f;
    }
}

void D3DApp::createTextureResources(const std::vector<MeshData>& meshes, const std::string& modelFilePath) {
    std::unordered_map<std::string, UINT> pathToIndexMap;
    std::vector<std::string> uniquePaths;
    std::filesystem::path modelDirectory = std::filesystem::path(modelFilePath).parent_path();
    for (auto& mesh : meshes) {
        if (mesh.material.diffuseTexturePath.empty()) continue;

        std::filesystem::path variant = mesh.material.diffuseTexturePath;
        if (variant.is_relative()) variant = modelDirectory / variant;
        std::string path = variant.lexically_normal().string();

        if (pathToIndexMap.find(path) == pathToIndexMap.end()) {
            pathToIndexMap[path] = static_cast<UINT>(uniquePaths.size());
            uniquePaths.push_back(path);
        }
        const_cast<MeshData&>(mesh).material.srvIndex = pathToIndexMap[path];

        if (uniquePaths.empty()) return;

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = static_cast<UINT>(uniquePaths.size());
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        failCheck(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

        mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        mTextureResources.resize(uniquePaths.size());

        for (size_t i = 0; i < uniquePaths.size(); ++i) {
            std::wstring wpath = std::filesystem::path(uniquePaths[i]).wstring();

            TexMetadata metadata;
            ScratchImage scratchImg;
            failCheck(LoadFromWICFile(wpath.c_str(), WIC_FLAGS_NONE, &metadata, scratchImg));

            const Image* img = scratchImg.GetImage(0, 0, 0);

            CD3DX12_RESOURCE_DESC texDesc =
                CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width,
                    static_cast<UINT>(metadata.height),
                    static_cast<UINT16>(metadata.arraySize),
                    static_cast<UINT16>(metadata.mipLevels));

            auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            failCheck(md3dDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&mTextureResources[i])
            ));

            UINT64 uploadBufferSize = GetRequiredIntermediateSize(mTextureResources[i].Get(), 0, 1);
            ComPtr<ID3D12Resource> uploadHeap;

            heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto buffer = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
            failCheck(md3dDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &buffer,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&uploadHeap)
            ));

            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData = img->pixels;
            textureData.RowPitch = img->rowPitch;
            textureData.SlicePitch = img->slicePitch;

            UpdateSubresources(mCommandList.Get(), mTextureResources[i].Get(), uploadHeap.Get(), 0, 0, 1, &textureData);

            auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
                mTextureResources[i].Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            mCommandList->ResourceBarrier(1, &transition);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = mTextureResources[i]->GetDesc().Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = mTextureResources[i]->GetDesc().MipLevels;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

            CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mSrvHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(i), mCbvSrvDescriptorSize);
            md3dDevice->CreateShaderResourceView(mTextureResources[i].Get(), &srvDesc, handle);
        }
    }
}