#include "../src/Master.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    Master engine(hInstance);

    if (!engine.Initialize()) {
        MessageBoxW(NULL, L"Initialization Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    engine.Run();

    return 0;
}