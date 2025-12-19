#include "d3dapp.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    D3DApp theApp(hInstance);
    if (!theApp.initMainWindow(hInstance, showCmd)) return 0;
    theApp.initializeDX();
    return theApp.run();
}