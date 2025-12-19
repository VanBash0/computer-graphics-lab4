#include "box_app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int showCmd) {
    BoxApp app(hInstance);
    if (!app.initMainWindow(hInstance, showCmd))
        return 0;

    app.initializeDX();
    app.buildResources();
    app.onResize();
    return app.run();
}