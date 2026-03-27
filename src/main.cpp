#include "Config.h"
#include "MainWindow.h"
#include "Utils.h"

#include <commctrl.h>
#include <gdiplus.h>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    INITCOMMONCONTROLSEX controls {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    ConfigStore configStore(JoinPath(GetModuleDirectory(), L"screenshot_config.ini"));
    MainWindow mainWindow(instance, &configStore);
    const int result = mainWindow.Create() ? mainWindow.Run() : 1;

    if (gdiplusToken != 0) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }
    if (SUCCEEDED(comInit)) {
        CoUninitialize();
    }
    return result;
}
