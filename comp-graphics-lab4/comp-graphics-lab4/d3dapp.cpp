#include "d3dapp.h"
#include "fail_checker.h"

void D3DApp::enableDebugLayer() {
    #if defined(_DEBUG) || defined(DEBUG)
    {
    ComPtr<ID3D12Debug> debugController;
    FailCheck(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
    debugController->EnableDebugLayer();
    }
    #endif
}

void D3DApp::createDXGIFactory() {
    FailCheck(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
}

void D3DApp::createDevice() {
    HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&md3dDevice));
    if (FAILED(hardwareResult)) {
        ComPtr<IDXGIAdapter> pWarpAdapter;
        FailCheck(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
        FailCheck(D3D12CreateDevice(pWarpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&md3dDevice)));
    }
}

void D3DApp::initialize() {
    enableDebugLayer();
    createDXGIFactory();
    createDevice();
}