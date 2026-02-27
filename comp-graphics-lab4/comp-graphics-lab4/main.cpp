#include "box_app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int showCmd) {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }

    BoxApp app(hInstance);
    if (!app.initMainWindow(hInstance, showCmd))
        return 0;

    app.initializeDX();
    app.buildResources();
    app.onResize();
    return app.run();
}