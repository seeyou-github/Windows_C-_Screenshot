#include "Config.h"
#include "MainWindow.h"
#include "ResourceIds.h"
#include "Utils.h"

#include <commctrl.h>
#include <gdiplus.h>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\ScreenshotTool_7B137E99_85E8_432C_A6A7_EE8AD7647D25");
    if (singleInstanceMutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(
            nullptr,
            LoadStringResource(instance, IDS_MSG_ALREADY_RUNNING).c_str(),
            LoadStringResource(instance, IDS_APP_TITLE).c_str(),
            MB_OK | MB_ICONINFORMATION);
        CloseHandle(singleInstanceMutex);
        return 0;
    }

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
    if (singleInstanceMutex != nullptr) {
        CloseHandle(singleInstanceMutex);
    }
    return result;
}
